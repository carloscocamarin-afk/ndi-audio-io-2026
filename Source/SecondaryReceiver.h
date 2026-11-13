#pragma once
#include <JuceHeader.h>
#include <Processing.NDI.Lib.h>
#include "SecondaryDevice.h"
#include <atomic>
#include <string>

//==============================================================================
// Segundo receptor NDI. Reproduce una fuente NDI por la SALIDA del dispositivo
// del segundo par (ver SecondaryDevice).
//
// - Usa el "frame-sync" de NDI: adapta el audio recibido al reloj de la tarjeta
//   (aunque las dos tarjetas no esten sincronizadas entre si).
// - Si la fuente no aparece en ~15 s, recrea el receptor. Esto solo ocurre
//   mientras el receptor esta ACTIVADO (boton pulsado).
//==============================================================================
class SecondaryReceiver : private juce::AudioIODeviceCallback,
                          private juce::Timer
{
  public:
    explicit SecondaryReceiver(SecondaryDevice& sharedDevice)
        : device(sharedDevice)
    {
    }

    ~SecondaryReceiver() override
    {
        stop();
    }

    void setNDILib(const NDIlib_v5* newLib)
    {
        lib = newLib;
    }

    //==========================================================================
    // Configuracion (hilo de mensajes)
    void setSourceName(const juce::String& newName)
    {
        sourceName = newName.trim();
    }

    juce::String getSourceName() const
    {
        return sourceName;
    }

    //==========================================================================
    // Control
    bool isWanted() const
    {
        return wanted;
    }

    // Arranca el receptor. Devuelve false si la configuracion no es valida
    // (falta la fuente o elegir la salida en AJUSTES).
    bool start()
    {
        stop();
        lastError.clear();

        if (lib == nullptr)
        {
            lastError = "NDI no disponible";
            return false;
        }
        if (sourceName.isEmpty())
        {
            lastError = "Elige una fuente";
            return false;
        }
        if (!createReceiver())
        {
            lastError = "No se pudo crear el receptor NDI";
            return false;
        }

        wanted = true;
        search_retries = 0;
        search_seconds = 0;
        connections.store(0);
        startTimerHz(1);

        if (!device.attach(this, false, true))
        {
            const auto message = device.getError();
            stop();
            lastError = message;
            return false;
        }

        return true;
    }

    void stop()
    {
        wanted = false;
        stopTimer();
        device.detach(this);
        destroyReceiver();
        connections.store(-1);
        search_retries = 0;
    }

    //==========================================================================
    // Estado para la interfaz
    juce::String getError() const
    {
        return wanted ? (lastError.isNotEmpty() ? lastError : device.getError())
                      : lastError;
    }

    int getRetries() const // reintentos de la tarjeta
    {
        return device.getRetries();
    }

    int getSearchRetries() const // reintentos de la fuente
    {
        return search_retries;
    }

    // 0 = buscando la fuente, >0 = conectado, -1 = apagado
    int getConnections() const
    {
        return connections.load();
    }

    float takePeak()
    {
        return peak.exchange(0.0f);
    }

    bool isAudioRunning() const
    {
        return device.isRunning();
    }

    //==========================================================================
    // Persistencia
    juce::ValueTree getState() const
    {
        juce::ValueTree v("recv2");
        v.setProperty("source", sourceName, nullptr);
        v.setProperty("active", wanted, nullptr);
        return v;
    }

    void restore(const juce::ValueTree& v)
    {
        if (!juce::JUCEApplicationBase::isStandaloneApp())
            return;

        sourceName = v["source"].toString();

        if ((bool)v["active"])
            start();
        else
            stop();
    }

  private:
    //==========================================================================
    // NDI
    bool createReceiver()
    {
        NDIlib_recv_create_v3_t create;
        const std::string source = sourceName.toStdString();
        const std::string receiverName = "NDI Audio IO - Receptor 2";

        create.source_to_connect_to.p_ndi_name = source.c_str();
        create.bandwidth = NDIlib_recv_bandwidth_audio_only; // solo audio
        create.p_ndi_recv_name = receiverName.c_str();

        auto* receiver = lib->recv_create_v3(&create);
        if (receiver == nullptr)
            return false;

        auto* sync = lib->framesync_create(receiver);
        if (sync == nullptr)
        {
            lib->recv_destroy(receiver);
            return false;
        }

        const juce::SpinLock::ScopedLockType sl(lock);
        ndi_recv = receiver;
        framesync = sync;
        return true;
    }

    void destroyReceiver()
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        if (lib != nullptr)
        {
            if (framesync != nullptr)
                lib->framesync_destroy(framesync);
            if (ndi_recv != nullptr)
                lib->recv_destroy(ndi_recv);
        }
        framesync = nullptr;
        ndi_recv = nullptr;
    }

    // Cada segundo: si la fuente no aparece, recrea el receptor cada ~15 s.
    void timerCallback() override
    {
        if (!wanted || !device.isRunning())
        {
            search_seconds = 0;
            return;
        }

        if (connections.load() == 0)
        {
            // NDI ya reconecta solo; esto es un respaldo
            if (++search_seconds >= 15)
            {
                search_seconds = 0;
                ++search_retries;
                destroyReceiver();
                if (!createReceiver())
                    lastError = "No se pudo crear el receptor NDI";
            }
        }
        else
        {
            search_seconds = 0;
            if (connections.load() > 0)
                search_retries = 0;
        }
    }

    //==========================================================================
    // Hilo de audio
    void audioDeviceAboutToStart(juce::AudioIODevice* audioDevice) override
    {
        sample_rate.store((int)audioDevice->getCurrentSampleRate());
        poll_samples = 0;
    }

    void audioDeviceStopped() override
    {
    }

    void audioDeviceIOCallbackWithContext(
        const float* const*, int, float* const* outputChannelData,
        int numOutputChannels, int numSamples,
        const juce::AudioIODeviceCallbackContext&) override
    {
        for (int i = 0; i < numOutputChannels; ++i)
            if (outputChannelData[i] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);

        device.beat();

        const juce::SpinLock::ScopedTryLockType sl(lock);
        if (!sl.isLocked() || framesync == nullptr || ndi_recv == nullptr ||
            lib == nullptr || numOutputChannels <= 0 || numSamples <= 0)
            return;

        NDIlib_audio_frame_v2_t frame{};

        // canales de la fuente (con 0,0,0 devuelve el formato actual)
        lib->framesync_capture_audio(framesync, &frame, 0, 0, 0);
        const int sourceChannels = frame.no_channels;

        lib->framesync_capture_audio(framesync, &frame, sample_rate.load(),
                                     sourceChannels, numSamples);

        float blockPeak = 0.0f;

        if (frame.p_data != nullptr && frame.no_channels > 0)
        {
            const int samples = juce::jmin(numSamples, frame.no_samples);

            for (int ch = 0; ch < numOutputChannels; ++ch)
            {
                if (outputChannelData[ch] == nullptr)
                    continue;

                // fuente mono: se reparte a todas las salidas
                const int sourceCh = frame.no_channels == 1 ? 0 : ch;
                if (sourceCh >= frame.no_channels)
                    continue;

                const float* src = reinterpret_cast<const float*>(
                    reinterpret_cast<const uint8_t*>(frame.p_data) +
                    (size_t)sourceCh * (size_t)frame.channel_stride_in_bytes);

                float* dst = outputChannelData[ch];
                for (int j = 0; j < samples; ++j)
                {
                    dst[j] = src[j];
                    blockPeak = juce::jmax(blockPeak, std::abs(src[j]));
                }
            }
        }

        lib->framesync_free_audio(framesync, &frame);

        if (blockPeak > peak.load(std::memory_order_relaxed))
            peak.store(blockPeak, std::memory_order_relaxed);

        // conexion con la fuente, ~10 veces por segundo
        poll_samples += numSamples;
        if (poll_samples >= juce::jmax(1, sample_rate.load() / 10))
        {
            poll_samples = 0;
            connections.store(lib->recv_get_no_connections(ndi_recv));
        }
    }

    //==========================================================================
    SecondaryDevice& device;
    const NDIlib_v5* lib = nullptr;

    juce::String sourceName;
    juce::String lastError;
    bool wanted = false;
    int search_retries = 0;
    int search_seconds = 0;

    juce::SpinLock lock;
    NDIlib_recv_instance_t ndi_recv = nullptr;
    NDIlib_framesync_instance_t framesync = nullptr;
    int poll_samples = 0;

    std::atomic<float> peak {0.0f};
    std::atomic<int> connections {-1};
    std::atomic<int> sample_rate {48000};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SecondaryReceiver)
};
