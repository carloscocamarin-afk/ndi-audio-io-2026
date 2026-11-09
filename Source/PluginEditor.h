/*
  ==============================================================================

  This is an automatically generated GUI class created by the Projucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Projucer version: 7.0.7

  ------------------------------------------------------------------------------

  The Projucer is part of the JUCE library.
  Copyright (c) 2020 - Raw Material Software Limited.

  ==============================================================================
*/

#pragma once

//[Headers]     -- You can add your own extra header files here --
#include "CustomLookAndFeel.h"
#include "StatusWidgets.h"
#include "PluginProcessor.h"
#include <JuceHeader.h>

//[/Headers]

//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Projucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class Sender2SettingsWindow; // ventana de ajustes de audio del emisor 2

class NdiAudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer,
                                public juce::TextEditor::Listener,
                                public juce::ComboBox::Listener,
                                public juce::Button::Listener
{
  public:
    //==============================================================================
    explicit NdiAudioProcessorEditor(NdiAudioProcessor& p);
    ~NdiAudioProcessorEditor() override;

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    typedef juce::AudioProcessorValueTreeState::ButtonAttachment
        ButtonAttachment;
    typedef juce::AudioProcessorValueTreeState::ComboBoxAttachment
        ComboBoxAttachment;

    void textEditorReturnKeyPressed(TextEditor& editor) override;

    void timerCallback() override;
    //[/UserMethods]

    void paint(juce::Graphics& g) override;
    void resized() override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void buttonClicked(juce::Button* buttonThatWasClicked) override;

    // Binary resources:
    static const char* ndiLogoMasterLight_svg;
    static const int ndiLogoMasterLight_svgSize;

  private:
    //[UserVariables]   -- You can add your own custom variables in this
    //section.
    // section.
    CustomLookAndFeel lf;

    NdiAudioProcessor& ap;

    std::unique_ptr<ButtonAttachment> rxAttachment;
    std::unique_ptr<ButtonAttachment> txAttachment;
    std::unique_ptr<ComboBoxAttachment> comboboxAttachment;

    std::mutex editor_mutex;
    bool timer_update {false};
    String prev_text {};

    // indicadores de conexion, medidores y etiquetas
    LevelMeter send_meter, recv_meter;
    StatusIndicator send_status, recv_status;
    juce::Label send_label, recv_label;
    int source_refresh_ticks {0};
    int recv_silent_ticks {0};
    int recv_search_ticks {0};
    int recv_retries {0};

    // emisor 2 (segundo dispositivo)
    juce::Label send2_label, device2_label;
    juce::TextEditor send2_name;
    juce::ComboBox send2_device;
    juce::TextButton send2_button {"ENVIAR"};
    LevelMeter send2_meter;
    StatusIndicator send2_status;
    juce::TextButton send2_settings {"AJUSTES"};
    std::unique_ptr<Sender2SettingsWindow> settings2_window;
    int device_scan_ticks {0};

    void openSender2Settings();
    void closeSender2Settings();

    void refreshSources();
    void refreshDevices(bool force);
    void updateIndicators();

    //[/UserVariables]

    //==============================================================================
    std::unique_ptr<juce::ComboBox> combobox_sources;
    std::unique_ptr<juce::TextEditor> juce__textEditor;
    std::unique_ptr<juce::TextButton> juce__sendButton;
    std::unique_ptr<juce::TextButton> juce__recvButton;
    std::unique_ptr<juce::Drawable> drawable1;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NdiAudioProcessorEditor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]
