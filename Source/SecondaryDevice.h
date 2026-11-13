#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

//==============================================================================
// Dispositivo de audio del SEGUNDO PAR (Emisor 2 + Receptor 2).
//
// Igual que el dispositivo principal (Emisor 1 + Receptor 1), un solo ajuste
// define la tarjeta de ENTRADA (la usa el Emisor 2) y la de SALIDA (la usa el
// Receptor 2), junto con tipo de driver, canales, frecuencia y buffer.
//
// - Es dueno de un AudioDeviceManager propio, distinto del principal.
// - El emisor y el receptor se "enganchan" (attach) cuando arrancan y se
//   sueltan (detach) cuando paran. La tarjeta solo se abre en las direcciones
//   que alguien necesita, y se cierra cuando nadie la usa.
// - Vigila la tarjeta: si deja de entregar audio, intenta reabrirla cada 5 s.
//   No cambia en silencio a otra tarjeta.
//==============================================================================
class SecondaryDevice : private juce::Timer
{
  public:
    SecondaryDevice() = default;

    ~SecondaryDevice() override
    {
        stopTimer();
        manager.closeAudioDevice();
    }

    juce::AudioDeviceManager& getManager()
    {
        return manager;
    }

    //==========================================================================
    // Estado visible
    juce::String getInputName() const
    {
        return inputName;
    }

    juce::String getOutputName() const
    {
        return outputName;
    }

    juce::String getError() const
    {
        return lastError;
    }

    int getRetries() const
    {
        return retries;
    }

    // true si algun cliente esta recibiendo bloques de audio
    bool isRunning() const
    {
        return (juce::Time::getMillisecondCounter() - last_block_ms.load()) <
               1000u;
    }

    // Lo llaman los clientes desde el hilo de audio en cada bloque.
    void beat()
    {
        last_block_ms.store(juce::Time::getMillisecondCounter());
    }

    //==========================================================================
    // Clientes (hilo de mensajes)

    // Devuelve false si falta elegir la tarjeta en AJUSTES. Si la tarjeta esta
    // elegida pero no se puede abrir, devuelve true y se reintenta sola.
    bool attach(juce::AudioIODeviceCallback* client, bool needsInput,
                bool needsOutput)
    {
        removeClient(client);
        lastError.clear();

        if (needsInput && inputName.isEmpty())
        {
            lastError = "Elige la entrada en AJUSTES";
            return false;
        }
        if (needsOutput && outputName.isEmpty())
        {
            lastError = "Elige la salida en AJUSTES";
            return false;
        }

        clients.push_back({client, needsInput, needsOutput});
        manager.addAudioCallback(client);

        retries = 0;
        stale_seconds = 0;

        const bool covered = manager.getCurrentAudioDevice() != nullptr &&
                             (!needsInput || openedInput) &&
                             (!needsOutput || openedOutput);
        if (!covered)
            openDevice();

        startTimerHz(1);
        return true;
    }

    void detach(juce::AudioIODeviceCallback* client)
    {
        removeClient(client);

        if (clients.empty() && !settingsOpen)
        {
            stopTimer();
            closeDevice();
            lastError.clear();
            retries = 0;
        }
    }

    //==========================================================================
    // Dialogo de ajustes (entrada + salida)

    // Llamar antes de mostrar el dialogo.
    void beginSettings()
    {
        settingsOpen = true;
        fillDefaultNames();
        openDevice();
    }

    // Llamar al cerrar el dialogo: guarda lo elegido y reabre segun los clientes.
    void endSettings()
    {
        captureSetup();
        settingsOpen = false;
        stale_seconds = 0;
        retries = 0;

        if (clients.empty())
        {
            stopTimer();
            closeDevice();
            lastError.clear();
        }
        else
        {
            openDevice();
        }
    }

    //==========================================================================
    // Persistencia
    juce::ValueTree getState() const
    {
        juce::ValueTree v("pair2");
        v.setProperty("input", inputName, nullptr);
        v.setProperty("output", outputName, nullptr);
        v.setProperty("type", typeName, nullptr);
        v.setProperty("rate", rateSetting, nullptr);
        v.setProperty("buffer", bufferSetting, nullptr);
        v.setProperty("inChannels", inputMask, nullptr);
        v.setProperty("outChannels", outputMask, nullptr);
        return v;
    }

    void restore(const juce::ValueTree& v)
    {
        if (!juce::JUCEApplicationBase::isStandaloneApp())
            return;

        inputName = v["input"].toString();
        outputName = v["output"].toString();
        typeName = v["type"].toString();
        rateSetting = v.hasProperty("rate") ? (double)v["rate"] : 48000.0;
        bufferSetting = (int)v["buffer"];
        inputMask = (int)v["inChannels"];
        outputMask = (int)v["outChannels"];
    }

  private:
    struct Client
    {
        juce::AudioIODeviceCallback* callback;
        bool input;
        bool output;
    };

    void removeClient(juce::AudioIODeviceCallback* client)
    {
        manager.removeAudioCallback(client);

        for (auto it = clients.begin(); it != clients.end(); ++it)
        {
            if (it->callback == client)
            {
                clients.erase(it);
                break;
            }
        }
    }

    // Cambia al tipo de driver guardado, solo si no hay tarjeta abierta.
    void applySavedType()
    {
        if (typeName.isEmpty() || manager.getCurrentAudioDevice() != nullptr)
            return;

        if (manager.getCurrentAudioDeviceType() != typeName)
            manager.setCurrentAudioDeviceType(typeName, false);
    }

    // Si aun no hay nada elegido, parte de las tarjetas por defecto del sistema.
    void fillDefaultNames()
    {
        manager.getAvailableDeviceTypes();
        applySavedType();

        auto* type = manager.getCurrentDeviceTypeObject();
        if (type == nullptr)
            return;

        type->scanForDevices();

        if (inputName.isEmpty())
        {
            const auto names = type->getDeviceNames(true);
            const int i = type->getDefaultDeviceIndex(true);
            if (juce::isPositiveAndBelow(i, names.size()))
                inputName = names[i];
        }

        if (outputName.isEmpty())
        {
            const auto names = type->getDeviceNames(false);
            const int i = type->getDefaultDeviceIndex(false);
            if (juce::isPositiveAndBelow(i, names.size()))
                outputName = names[i];
        }
    }

    // Lee del gestor lo que se eligio en el dialogo.
    void captureSetup()
    {
        auto* device = manager.getCurrentAudioDevice();
        if (device == nullptr)
            return;

        const auto setup = manager.getAudioDeviceSetup();

        // el dialogo abre ambas direcciones: vacio significa "ninguna"
        inputName = setup.inputDeviceName;
        outputName = setup.outputDeviceName;

        const auto type = manager.getCurrentAudioDeviceType();
        if (type.isNotEmpty())
            typeName = type;

        rateSetting = device->getCurrentSampleRate();
        bufferSetting = device->getCurrentBufferSizeSamples();
        inputMask = device->getActiveInputChannels().toInteger();
        outputMask = device->getActiveOutputChannels().toInteger();
    }

    static juce::BigInteger makeChannels(int mask, int numChannels)
    {
        juce::BigInteger channels((juce::int32)mask);
        juce::BigInteger valid;
        valid.setRange(0, numChannels, true);
        channels &= valid;

        // 0 = automatico: los dos primeros
        if (channels.isZero())
            channels.setRange(0, juce::jmin(2, numChannels), true);

        return channels;
    }

    // Abre la tarjeta con lo que necesitan los clientes (y el dialogo).
    void openDevice()
    {
        closeDevice();
        lastError.clear();

        bool needIn = settingsOpen && inputName.isNotEmpty();
        bool needOut = settingsOpen && outputName.isNotEmpty();
        for (const auto& c : clients)
        {
            needIn = needIn || c.input;
            needOut = needOut || c.output;
        }

        if (!needIn && !needOut)
            return;

        if (needIn && inputName.isEmpty())
        {
            lastError = "Elige la entrada en AJUSTES";
            return;
        }
        if (needOut && outputName.isEmpty())
        {
            lastError = "Elige la salida en AJUSTES";
            return;
        }

        manager.getAvailableDeviceTypes();
        applySavedType();
        auto* type = manager.getCurrentDeviceTypeObject();
        if (type == nullptr)
        {
            lastError = "No hay drivers de audio";
            return;
        }

        type->scanForDevices();

        if (needIn && !type->getDeviceNames(true).contains(inputName))
        {
            lastError = "Entrada no disponible: " + inputName;
            return;
        }
        if (needOut && !type->getDeviceNames(false).contains(outputName))
        {
            lastError = "Salida no disponible: " + outputName;
            return;
        }

        // cuantos canales tiene cada direccion
        int numIn = 0, numOut = 0;
        {
            std::unique_ptr<juce::AudioIODevice> probe(type->createDevice(
                needOut ? outputName : juce::String(),
                needIn ? inputName : juce::String()));
            if (probe != nullptr)
            {
                numIn = probe->getInputChannelNames().size();
                numOut = probe->getOutputChannelNames().size();
            }
        }

        if ((needIn && numIn <= 0) || (needOut && numOut <= 0))
        {
            lastError = "No se pudo abrir la tarjeta";
            return;
        }

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.inputDeviceName = needIn ? inputName : juce::String();
        setup.outputDeviceName = needOut ? outputName : juce::String();
        setup.useDefaultInputChannels = false;
        setup.inputChannels = needIn ? makeChannels(inputMask, numIn)
                                     : juce::BigInteger();
        setup.useDefaultOutputChannels = false;
        setup.outputChannels = needOut ? makeChannels(outputMask, numOut)
                                       : juce::BigInteger();
        setup.sampleRate = rateSetting > 0.0 ? rateSetting : 48000.0;
        setup.bufferSize = bufferSetting;

        const auto error = manager.setAudioDeviceSetup(setup, false);
        if (error.isNotEmpty())
        {
            lastError = error.upToFirstOccurrenceOf("\n", false, false);
            closeDevice();
            return;
        }

        openedInput = needIn;
        openedOutput = needOut;
    }

    void closeDevice()
    {
        manager.closeAudioDevice();
        last_block_ms.store(0);
        openedInput = false;
        openedOutput = false;
    }

    // Cada segundo: si no llegan bloques de audio, reintenta abrir la tarjeta.
    void timerCallback() override
    {
        if (clients.empty() || settingsOpen)
            return;

        if (isRunning())
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
    juce::AudioDeviceManager manager;
    std::vector<Client> clients;

    juce::String inputName;
    juce::String outputName;
    juce::String typeName;
    double rateSetting = 48000.0;
    int bufferSetting = 0;
    int inputMask = 0;
    int outputMask = 0;

    bool settingsOpen = false;
    bool openedInput = false;
    bool openedOutput = false;
    juce::String lastError;
    int retries = 0;
    int stale_seconds = 0;

    std::atomic<uint32_t> last_block_ms {0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SecondaryDevice)
};
