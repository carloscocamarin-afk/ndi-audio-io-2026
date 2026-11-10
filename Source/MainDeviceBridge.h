#pragma once
#include <JuceHeader.h>
#include <functional>

//==============================================================================
// Puente entre la ventana principal (StandaloneFilterWindow, que es quien tiene
// el dispositivo de audio principal) y el editor. Asi el editor puede abrir el
// dialogo de ajustes del dispositivo principal y mostrar su nombre, sin tocar
// las clases internas del dispositivo.
//
// Lo rellena StandaloneFilterWindow al crearse; en un plugin queda vacio.
//==============================================================================
struct MainDeviceBridge
{
    static std::function<void()> &openSettings()
    {
        static std::function<void()> fn;
        return fn;
    }

    static std::function<juce::String()> &inputName()
    {
        static std::function<juce::String()> fn;
        return fn;
    }

    static std::function<juce::String()> &outputName()
    {
        static std::function<juce::String()> fn;
        return fn;
    }
};
