#pragma once
#include <JuceHeader.h>
#include <Processing.NDI.Lib.h>
#include <atomic>
#include <string>

//==============================================================================
// Segundo receptor NDI con su PROPIA tarjeta de sonido de salida.
//
// El receptor 1 saca el audio por el dispositivo principal (Menu > Settings).
// Este receptor se conecta a otra fuente NDI y la reproduce por otra tarjeta,
// con su propio AudioDeviceManager.
//
// - Usa el "frame-sync" de NDI: adapta el audio recibido al reloj de la tarjeta
//   (aunque las dos tarjetas no esten sincronizadas entre si).
// - Vigila el dispositivo (reintenta abrirlo si se desconecta) y la fuente
//   (si pasan ~15 s sin conexion, recrea el receptor).
//==============================================================================
class SecondaryReceiver : private juce::AudioIODeviceCallback,
                          private juce::Timer
{
  public:
    SecondaryReceiver() = default;

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

    void setDeviceName(const juce::String& newDevice)
    {
        if (newDevice != deviceName)
        {
            outputMask = 0;
            bufferSetting = 0;
        }
        deviceName = newDevice;
    }

    juce::String getDeviceName() const
    {
        return deviceName;
    }

    // Nombres de los dispositivos de SALIDA. No llamar con el dispositivo en uso.
    juce::StringArray scanOutputDevices()
    {
        deviceManager.getAvailableDeviceTypes();
        applySavedType();
        if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        {
            type->scanForDevices();
            return type->getDeviceNames(false);
        }
        return {};
    }

    //==========================================================================
    // Ajustes de audio (dialogo con tipo de driver, tarjeta, canales, etc.)
    juce::AudioDeviceManager& getDeviceManager()
    {
        return deviceManager;
    }

    void beginSettings()
    {
        settingsOpen = true;
        if (!wanted)
            openDevice();
    }

    void endSettings()
    {
        captureSetup();
        settingsOpen = false;
        stale_seconds = 0;

        if (!wanted)
        {
            closeDevice();
            lastError.clear();
        }
    }

    //==========================================================================
    // Control
    bool isWanted() const
    {
        return wanted;
    }

    // Arranca el receptor. Devuelve false si la configuracion no es valida; si
    // la tarjeta no se puede abrir, sigue "deseado" y se reintenta solo.
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
        if (deviceName.isEmpty())
        {
            lastError = "Elige un dispositivo";
            return false;
        }
        if (!createReceiver())
        {
            lastError = "No se pudo crear el receptor NDI";
            return false;
        }

        wanted = true;
        retries = 0;
        search_retries = 0;
        stale_seconds = 0;
        search_seconds = 0;
        connections.store(0);
        startTimerHz(1);

        openDevice();
        return true;
    }

    void stop()
    {
        wanted = false;
        stopTimer();
        closeDevice();
        destroyReceiver();
        connections.store(-1);
        retries = 0;
        search_retries = 0;
    }

    void reopenDevice()
    {
        if (!wanted)
            return;

        retries = 0;
        stale_seconds = 0;
        openDevice();
    }

    //==========================================================================
    // Estado para la interfaz
    juce::String getError() const
    {
        return lastError;
    }

    int getRetries() const // reintentos de la tarjeta
    {
        return retries;
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
        return (juce::Time::getMillisecondCounter() - last_block_ms.load()) <
               1000u;
    }

    //==========================================================================
    // Persistencia
    juce::ValueTree getState() const
    {
        juce::ValueTree v("recv2");
        v.setProperty("source", sourceName, nullptr);
        v.setProperty("device", deviceName, nullptr);
        v.setProperty("active", wanted, nullptr);
        v.setProperty("type", typeName, nullptr);
        v.setProperty("rate", rateSetting, nullptr);
        v.setProperty("buffer", bufferSetting, nullptr);
        v.setProperty("channels", outputMask, nullptr);
        return v;
    }

    void restore(const juce::ValueTree& v)
    {
        if (!juce::JUCEApplicationBase::isStandaloneApp())
            return;

        sourceName = v["source"].toString();
        deviceName = v["device"].toString();
        typeName = v["type"].toString();
        rateSetting = v.hasProperty("rate") ? (double)v["rate"] : 48000.0;
        bufferSetting = (int)v["buffer"];
        outputMask = (int)v["channels"];

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

    //==========================================================================
    // Dispositivo de salida
    void applySavedType()
    {
        if (typeName.isEmpty() || deviceManager.getCurrentAudioDevice() != nullptr)
            return;

        if (deviceManager.getCurrentAudioDeviceType() != typeName)
            deviceManager.setCurrentAudioDeviceType(typeName, false);
    }

    void captureSetup()
    {
        const auto setup = deviceManager.getAudioDeviceSetup();
        if (setup.outputDeviceName.isNotEmpty())
            deviceName = setup.outputDeviceName;

        const auto type = deviceManager.getCurrentAudioDeviceType();
        if (type.isNotEmpty())
            typeName = type;

        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            rateSetting = device->getCurrentSampleRate();
            bufferSetting = device->getCurrentBufferSizeSamples();
            outputMask = device->getActiveOutputChannels().toInteger();
        }
    }

    void openDevice()
    {
        closeDevice();
        lastError.clear();

        deviceManager.getAvailableDeviceTypes();
        applySavedType();
        auto* type = deviceManager.getCurrentDeviceTypeObject();
        if (type == nullptr)
        {
            lastError = "No hay drivers de audio";
            return;
        }

        type->scanForDevices();

        int numChannels = 0;
        {
            std::unique_ptr<juce::AudioIODevice> probe(
                type->createDevice(deviceName, {}));
            if (probe != nullptr)
                numChannels = probe->getOutputChannelNames().size();
        }

        if (numChannels <= 0)
        {
            lastError = "Dispositivo no disponible: " + deviceName;
            return;
        }

        // canales de salida elegidos (0 = automatico: los dos primeros)
        juce::BigInteger channels((juce::int32)outputMask);
        juce::BigInteger valid;
        valid.setRange(0, numChannels, true);
        channels &= valid;
        if (channels.isZero())
            channels.setRange(0, juce::jmin(2, numChannels), true);

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.inputDeviceName = {};
        setup.outputDeviceName = deviceName;
        setup.useDefaultInputChannels = false;
        setup.inputChannels.clear();
        setup.useDefaultOutputChannels = false;
        setup.outputChannels = channels;
        setup.sampleRate = rateSetting > 0.0 ? rateSetting : 48000.0;
        setup.bufferSize = bufferSetting;

        const auto error = deviceManager.setAudioDeviceSetup(setup, false);
        if (error.isNotEmpty())
        {
            lastError = error.upToFirstOccurrenceOf("\n", false, false);
            closeDevice();
            return;
        }

        deviceManager.addAudioCallback(this);
    }

    void closeDevice()
    {
        deviceManager.removeAudioCallback(this);
        deviceManager.closeAudioDevice();
        last_block_ms.store(0);
    }

    // Cada segundo: vigila la tarjeta y la conexion con la fuente.
    void timerCallback() override
    {
        if (!wanted || settingsOpen)
            return;

        if (!isAudioRunning())
        {
            search_seconds = 0;
            if (++stale_seconds >= 5)
            {
                stale_seconds = 0;
                ++retries;
                openDevice();
            }
            return;
        }

        stale_seconds = 0;
        retries = 0;

        if (connections.load() == 0)
        {
            // NDI ya reconecta solo; si pasan ~15 s buscando, se recrea el receptor
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
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        sample_rate.store((int)device->getCurrentSampleRate());
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

        last_block_ms.store(juce::Time::getMillisecondCounter());

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
    const NDIlib_v5* lib = nullptr;
    juce::AudioDeviceManager deviceManager;

    juce::String sourceName;
    juce::String deviceName;
    juce::String typeName;
    double rateSetting = 48000.0;
    int bufferSetting = 0;
    int outputMask = 0;
    bool settingsOpen = false;
    juce::String lastError;
    bool wanted = false;
    int retries = 0;
    int search_retries = 0;
    int stale_seconds = 0;
    int search_seconds = 0;

    juce::SpinLock lock;
    NDIlib_recv_instance_t ndi_recv = nullptr;
    NDIlib_framesync_instance_t framesync = nullptr;
    int poll_samples = 0;

    std::atomic<float> peak {0.0f};
    std::atomic<int> connections {-1};
    std::atomic<int> sample_rate {48000};
    std::atomic<uint32_t> last_block_ms {0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SecondaryReceiver)
};
