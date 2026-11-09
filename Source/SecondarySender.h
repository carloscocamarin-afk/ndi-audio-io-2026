#pragma once
#include <JuceHeader.h>
#include <Processing.NDI.Lib.h>
#include <atomic>
#include <vector>

//==============================================================================
// Segundo emisor NDI con su PROPIO dispositivo de audio.
//
// El emisor 1 usa el dispositivo que se elige en Menu > Settings. Este emisor
// abre otra tarjeta de sonido por su cuenta (su propio AudioDeviceManager) y
// envia su audio con un nombre NDI distinto.
//
// - Los metodos de configuracion se llaman desde el hilo de mensajes (UI).
// - El audio corre en el hilo del dispositivo; se comunica con la UI solo con
//   atomicos.
// - Vigila el dispositivo: si se desconecta o deja de entregar audio, intenta
//   reabrirlo cada pocos segundos (el emisor NDI se mantiene vivo, asi los
//   receptores no pierden la conexion).
//==============================================================================
class SecondarySender : private juce::AudioIODeviceCallback,
                        private juce::Timer
{
  public:
    SecondarySender() = default;

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

    void setDeviceName(const juce::String& newDevice)
    {
        if (newDevice != deviceName)
        {
            // otra tarjeta: canales y buffer vuelven a automatico
            inputMask = 0;
            bufferSetting = 0;
        }
        deviceName = newDevice;
    }

    juce::String getDeviceName() const
    {
        return deviceName;
    }

    // Nombres de los dispositivos de entrada (del tipo de driver por defecto:
    // "Windows Audio" en Windows). No llamar mientras el dispositivo esta
    // abierto: reescanea los dispositivos.
    juce::StringArray scanInputDevices()
    {
        deviceManager.getAvailableDeviceTypes(); // crea los tipos de driver
        applySavedType();
        if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        {
            type->scanForDevices();
            return type->getDeviceNames(true);
        }
        return {};
    }

    //==========================================================================
    // Ajustes de audio (dialogo con tipo de driver, tarjeta, canales,
    // frecuencia y buffer). Usan el AudioDeviceManager propio de este emisor.
    juce::AudioDeviceManager& getDeviceManager()
    {
        return deviceManager;
    }

    // Llamar antes de mostrar el dialogo: si el emisor esta apagado, abre la
    // tarjeta para que el dialogo parta de un estado valido.
    void beginSettings()
    {
        settingsOpen = true;
        if (!wanted)
            openDevice();
    }

    // Llamar al cerrar el dialogo: guarda lo que se eligio y, si el emisor
    // esta apagado, vuelve a soltar la tarjeta.
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

    // Arranca el emisor. Devuelve false si la configuracion no es valida; si el
    // dispositivo no se puede abrir, sigue "deseado" y se reintenta solo.
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
        if (deviceName.isEmpty())
        {
            lastError = "Elige un dispositivo";
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
        }

        wanted = true;
        retries = 0;
        stale_seconds = 0;
        connections.store(0);
        startTimerHz(1); // vigilancia del dispositivo

        openDevice();
        return true;
    }

    void stop()
    {
        wanted = false;
        stopTimer();
        closeDevice();

        {
            const juce::SpinLock::ScopedLockType sl(lock);
            if (ndi_send != nullptr && lib != nullptr)
                lib->send_destroy(ndi_send);
            ndi_send = nullptr;
        }

        connections.store(-1);
        retries = 0;
    }

    // Reabre solo el dispositivo (cambio de dispositivo con el envio activo).
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

    int getRetries() const
    {
        return retries;
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
        return (juce::Time::getMillisecondCounter() - last_block_ms.load()) <
               1000u;
    }

    //==========================================================================
    // Persistencia
    juce::ValueTree getState() const
    {
        juce::ValueTree v("sender2");
        v.setProperty("name", name, nullptr);
        v.setProperty("device", deviceName, nullptr);
        v.setProperty("active", wanted, nullptr);
        v.setProperty("type", typeName, nullptr);
        v.setProperty("rate", rateSetting, nullptr);
        v.setProperty("buffer", bufferSetting, nullptr);
        v.setProperty("channels", inputMask, nullptr);
        return v;
    }

    void restore(const juce::ValueTree& v)
    {
        // solo tiene sentido en la app independiente
        if (!juce::JUCEApplicationBase::isStandaloneApp())
            return;

        const auto savedName = v["name"].toString();
        name = savedName.isNotEmpty() ? savedName : juce::String("Emisor2");
        deviceName = v["device"].toString();
        typeName = v["type"].toString();
        rateSetting = v.hasProperty("rate") ? (double)v["rate"] : 48000.0;
        bufferSetting = (int)v["buffer"];
        inputMask = (int)v["channels"];

        if ((bool)v["active"])
            start();
        else
            stop();
    }

  private:
    //==========================================================================
    // Cambia al tipo de driver guardado (p. ej. "Windows Audio"), solo si no
    // hay un dispositivo abierto.
    void applySavedType()
    {
        if (typeName.isEmpty() || deviceManager.getCurrentAudioDevice() != nullptr)
            return;

        if (deviceManager.getCurrentAudioDeviceType() != typeName)
            deviceManager.setCurrentAudioDeviceType(typeName, false);
    }

    // Lee del gestor lo que el usuario eligio en el dialogo de ajustes.
    void captureSetup()
    {
        const auto setup = deviceManager.getAudioDeviceSetup();
        if (setup.inputDeviceName.isNotEmpty())
            deviceName = setup.inputDeviceName;

        const auto type = deviceManager.getCurrentAudioDeviceType();
        if (type.isNotEmpty())
            typeName = type;

        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            rateSetting = device->getCurrentSampleRate();
            bufferSetting = device->getCurrentBufferSizeSamples();
            inputMask = device->getActiveInputChannels().toInteger();
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

        // cuantos canales de entrada tiene la tarjeta
        int numChannels = 0;
        {
            std::unique_ptr<juce::AudioIODevice> probe(
                type->createDevice({}, deviceName));
            if (probe != nullptr)
                numChannels = probe->getInputChannelNames().size();
        }

        if (numChannels <= 0)
        {
            lastError = "Dispositivo no disponible: " + deviceName;
            return;
        }

        // canales elegidos (0 = automatico: los dos primeros)
        juce::BigInteger channels((juce::int32)inputMask);
        juce::BigInteger valid;
        valid.setRange(0, numChannels, true);
        channels &= valid;
        if (channels.isZero())
            channels.setRange(0, juce::jmin(2, numChannels), true);

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.inputDeviceName = deviceName;
        setup.outputDeviceName = {};
        setup.useDefaultInputChannels = false;
        setup.inputChannels = channels;
        setup.useDefaultOutputChannels = false;
        setup.outputChannels.clear();
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

    // Cada segundo: si el audio no fluye, reintenta abrir el dispositivo.
    void timerCallback() override
    {
        if (!wanted || settingsOpen)
            return;

        if (isAudioRunning())
        {
            stale_seconds = 0;
            retries = 0;
            return;
        }

        if (++stale_seconds >= 5)
        {
            stale_seconds = 0;
            ++retries;
            openDevice();
        }
    }

    //==========================================================================
    // Hilo de audio
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        const int numChannels =
            juce::jmax(1, device->getActiveInputChannels().countNumberOfSetBits());
        const int blockSize = juce::jmax(8192, device->getCurrentBufferSizeSamples() * 2);

        buf.assign((size_t)numChannels * (size_t)blockSize, 0.0f);
        sample_rate.store((int)device->getCurrentSampleRate());
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

        last_block_ms.store(juce::Time::getMillisecondCounter());

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
    const NDIlib_v5* lib = nullptr;
    juce::AudioDeviceManager deviceManager;

    juce::String name {"Emisor2"};
    juce::String deviceName;
    juce::String typeName;       // tipo de driver (p. ej. "Windows Audio")
    double rateSetting = 48000.0; // frecuencia de muestreo
    int bufferSetting = 0;        // 0 = automatico
    int inputMask = 0;            // canales de entrada activos (0 = automatico)
    bool settingsOpen = false;
    juce::String lastError;
    bool wanted = false;
    int retries = 0;
    int stale_seconds = 0;

    juce::SpinLock lock;
    NDIlib_send_instance_t ndi_send = nullptr;
    std::vector<float> buf;
    int poll_samples = 0;

    std::atomic<float> peak {0.0f};
    std::atomic<int> connections {-1};
    std::atomic<int> sample_rate {48000};
    std::atomic<uint32_t> last_block_ms {0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SecondarySender)
};
