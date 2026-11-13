#pragma once
#include <JuceHeader.h>
#include <Processing.NDI.Lib.h>
#include "SecondaryDevice.h"
#include <atomic>
#include <string>
#include <vector>

//==============================================================================
// Segundo emisor NDI. Envia por NDI el audio de la ENTRADA del dispositivo del
// segundo par (ver SecondaryDevice), con un nombre NDI propio.
//
// - Configuracion y control desde el hilo de mensajes (UI).
// - El audio corre en el hilo del dispositivo y se comunica con la UI solo con
//   atomicos.
// - El emisor NDI se mantiene vivo aunque la tarjeta se desconecte, asi los
//   receptores no pierden la conexion mientras la tarjeta se reabre.
//==============================================================================
class SecondarySender : private juce::AudioIODeviceCallback
{
  public:
    explicit SecondarySender(SecondaryDevice& sharedDevice)
        : device(sharedDevice)
    {
    }

    ~SecondarySender() override
    {
        stop();
    }

    void setNDILib(const NDIlib_v5* newLib)
    {
        lib = newLib;
    }

    //==========================================================================
    // Configuracion (hilo de mensajes)
    void setName(const juce::String& newName)
    {
        name = newName.trim();
    }

    juce::String getName() const
    {
        return name;
    }

    // Nombre completo del emisor en la red (vacio si esta apagado)
    juce::String getFullSourceName() const
    {
        return fullName;
    }

    //==========================================================================
    // Control
    bool isWanted() const
    {
        return wanted;
    }

    // Arranca el emisor. Devuelve false si la configuracion no es valida (falta
    // el nombre o elegir la entrada en AJUSTES). Si la tarjeta elegida no se
    // puede abrir, sigue "deseado" y se reintenta solo.
    bool start()
    {
        stop();
        lastError.clear();

        if (lib == nullptr)
        {
            lastError = "NDI no disponible";
            return false;
        }
        if (name.isEmpty())
        {
            lastError = "Falta el nombre NDI";
            return false;
        }

        // emisor NDI (se mantiene mientras dure el envio)
        {
            NDIlib_send_create_t create{};
            const std::string utf8Name = name.toStdString();
            create.p_ndi_name = utf8Name.c_str();
            create.p_groups = nullptr;
            create.clock_video = false;
            create.clock_audio = true;

            auto* instance = lib->send_create(&create);
            if (instance == nullptr)
            {
                lastError = "No se pudo crear el emisor NDI";
                return false;
            }

            const juce::SpinLock::ScopedLockType sl(lock);
            ndi_send = instance;

            // nombre completo en la red, p. ej. "PC (Emisor2)"
            if (const auto* src = lib->send_get_source_name(instance))
                fullName = juce::String::fromUTF8(src->p_ndi_name);
        }

        wanted = true;
        connections.store(0);

        if (!device.attach(this, true, false))
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
        device.detach(this);

        {
            const juce::SpinLock::ScopedLockType sl(lock);
            if (ndi_send != nullptr && lib != nullptr)
                lib->send_destroy(ndi_send);
            ndi_send = nullptr;
        }

        fullName.clear();
        connections.store(-1);
    }

    //==========================================================================
    // Estado para la interfaz
    juce::String getError() const
    {
        return wanted ? device.getError() : lastError;
    }

    int getRetries() const
    {
        return device.getRetries();
    }

    // Receptores conectados. -1 = apagado.
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
        juce::ValueTree v("sender2");
        v.setProperty("name", name, nullptr);
        v.setProperty("active", wanted, nullptr);
        return v;
    }

    void restore(const juce::ValueTree& v)
    {
        // solo tiene sentido en la app independiente
        if (!juce::JUCEApplicationBase::isStandaloneApp())
            return;

        const auto savedName = v["name"].toString();
        name = savedName.isNotEmpty() ? savedName : juce::String("Emisor2");

        if ((bool)v["active"])
            start();
        else
            stop();
    }

  private:
    //==========================================================================
    // Hilo de audio
    void audioDeviceAboutToStart(juce::AudioIODevice* audioDevice) override
    {
        const int numChannels = juce::jmax(
            1, audioDevice->getActiveInputChannels().countNumberOfSetBits());
        const int blockSize =
            juce::jmax(8192, audioDevice->getCurrentBufferSizeSamples() * 2);

        buf.assign((size_t)numChannels * (size_t)blockSize, 0.0f);
        sample_rate.store((int)audioDevice->getCurrentSampleRate());
        poll_samples = 0;
    }

    void audioDeviceStopped() override
    {
    }

    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData, int numInputChannels,
        float* const* outputChannelData, int numOutputChannels, int numSamples,
        const juce::AudioIODeviceCallbackContext&) override
    {
        for (int i = 0; i < numOutputChannels; ++i)
            if (outputChannelData[i] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);

        device.beat();

        const juce::SpinLock::ScopedTryLockType sl(lock);
        if (!sl.isLocked() || ndi_send == nullptr || lib == nullptr)
            return;

        if (numInputChannels > 0 && numSamples > 0 &&
            (size_t)numInputChannels * (size_t)numSamples <= buf.size())
        {
            float blockPeak = 0.0f;

            for (int ch = 0; ch < numInputChannels; ++ch)
            {
                float* dst = buf.data() + (size_t)ch * (size_t)numSamples;
                const float* src = inputChannelData[ch];

                if (src == nullptr)
                {
                    juce::FloatVectorOperations::clear(dst, numSamples);
                    continue;
                }

                for (int j = 0; j < numSamples; ++j)
                {
                    dst[j] = src[j];
                    blockPeak = juce::jmax(blockPeak, std::abs(src[j]));
                }
            }

            if (blockPeak > peak.load(std::memory_order_relaxed))
                peak.store(blockPeak, std::memory_order_relaxed);

            NDIlib_audio_frame_v2_t frame{};
            frame.sample_rate = sample_rate.load();
            frame.no_channels = numInputChannels;
            frame.no_samples = numSamples;
            frame.timecode = NDIlib_send_timecode_synthesize;
            frame.p_data = buf.data();
            frame.channel_stride_in_bytes = numSamples * (int)sizeof(float);
            lib->send_send_audio_v2(ndi_send, &frame);
        }

        // receptores conectados, ~10 veces por segundo (timeout 0 = no bloquea)
        poll_samples += numSamples;
        if (poll_samples >= juce::jmax(1, sample_rate.load() / 10))
        {
            poll_samples = 0;
            connections.store(lib->send_get_no_connections(ndi_send, 0));
        }
    }

    //==========================================================================
    SecondaryDevice& device;
    const NDIlib_v5* lib = nullptr;

    juce::String name {"Emisor2"};
    juce::String fullName;
    juce::String lastError;
    bool wanted = false;

    juce::SpinLock lock;
    NDIlib_send_instance_t ndi_send = nullptr;
    std::vector<float> buf;
    int poll_samples = 0;

    std::atomic<float> peak {0.0f};
    std::atomic<int> connections {-1};
    std::atomic<int> sample_rate {48000};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SecondarySender)
};
