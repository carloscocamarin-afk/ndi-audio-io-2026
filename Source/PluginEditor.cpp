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

//[Headers] You can add your own extra header files here...
//[/Headers]

#include "PluginEditor.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...

// Ventana con el panel de ajustes de audio (tipo de driver, tarjeta, canales,
// frecuencia y buffer) del emisor 2, usando SU gestor de audio.
class Sender2SettingsWindow : public juce::DocumentWindow
{
  public:
    Sender2SettingsWindow(juce::AudioDeviceManager& manager,
                          juce::Colour background,
                          std::function<void()> onCloseRequested)
        : juce::DocumentWindow("Ajustes de audio - Emisor 2", background,
                               juce::DocumentWindow::closeButton),
          closeCallback(std::move(onCloseRequested))
    {
        auto* selector = new juce::AudioDeviceSelectorComponent(
            manager, 0, 8, 0, 0, false, false, false, false);
        selector->setSize(500, 340);

        setUsingNativeTitleBar(true);
        setContentOwned(selector, true);
        setResizable(false, false);
        setAlwaysOnTop(true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        if (closeCallback)
            closeCallback();
    }

  private:
    std::function<void()> closeCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Sender2SettingsWindow)
};

//[/MiscUserDefs]

//==============================================================================
NdiAudioProcessorEditor::NdiAudioProcessorEditor (NdiAudioProcessor& p)
    : AudioProcessorEditor (&p),
      ap (p)
{
    //[Constructor_pre] You can add your own custom stuff here..
    LookAndFeel::setDefaultLookAndFeel(&lf);
    lf.setColourScheme(LookAndFeel_V4::getMidnightColourScheme());
    //[/Constructor_pre]

    combobox_sources.reset (new juce::ComboBox ("ndi_sources"));
    addAndMakeVisible (combobox_sources.get());
    combobox_sources->setEditableText (true);
    combobox_sources->setJustificationType (juce::Justification::centredLeft);
    combobox_sources->setTextWhenNothingSelected (TRANS("no source"));
    combobox_sources->setTextWhenNoChoicesAvailable (TRANS("no sources"));
    combobox_sources->addListener (this);

    juce__textEditor.reset (new juce::TextEditor ("new text editor"));
    addAndMakeVisible (juce__textEditor.get());
    juce__textEditor->setMultiLine (false);
    juce__textEditor->setReturnKeyStartsNewLine (false);
    juce__textEditor->setReadOnly (false);
    juce__textEditor->setScrollbarsShown (true);
    juce__textEditor->setCaretVisible (true);
    juce__textEditor->setPopupMenuEnabled (true);
    juce__textEditor->setText (juce::String());

    juce__sendButton.reset (new juce::TextButton ("send"));
    addAndMakeVisible (juce__sendButton.get());
    juce__sendButton->addListener (this);
    juce__sendButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e0e0e));
    juce__sendButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffc93c3c));

    juce__recvButton.reset (new juce::TextButton ("recv"));
    addAndMakeVisible (juce__recvButton.get());
    juce__recvButton->addListener (this);
    juce__recvButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e0e0e));
    juce__recvButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2a9d62));

    drawable1 = juce::Drawable::createFromImageData (ndiLogoMasterLight_svg, ndiLogoMasterLight_svgSize);

    //[UserPreSize]
    juce__textEditor->setText(ap.getNDISendTextInput());
    juce__textEditor->addListener(this);
    juce__textEditor->applyFontToAllText(lf.getPopupMenuFont());
    juce__textEditor->setJustification(Justification::verticallyCentred);

    juce__sendButton->setToggleable(true);
    juce__sendButton->setClickingTogglesState(true);

    juce__recvButton->setToggleable(true);
    juce__recvButton->setClickingTogglesState(true);

    // botones: texto blanco y textos claros segun el estado
    for (auto* b : {juce__sendButton.get(), juce__recvButton.get()})
    {
        b->setColour(juce::TextButton::textColourOffId,
                     juce::Colours::white.withAlpha(0.85f));
        b->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }
    juce__sendButton->setButtonText("ENVIAR");
    juce__recvButton->setButtonText("RECIBIR");

    // etiquetas sobre los campos
    send_label.setText("Emisor 1 - nombre NDI (Enter para aplicar)",
                       juce::dontSendNotification);
    recv_label.setText("Fuente NDI a recibir", juce::dontSendNotification);
    for (auto* l : {&send_label, &recv_label})
    {
        l->setColour(juce::Label::textColourId,
                     juce::Colours::white.withAlpha(0.6f));
        l->setJustificationType(juce::Justification::bottomLeft);
        l->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(l);
    }

    addAndMakeVisible(send_meter);
    addAndMakeVisible(recv_meter);
    addAndMakeVisible(send_status);
    addAndMakeVisible(recv_status);

    // ---------------- emisor 2 ----------------
    send2_label.setText("Emisor 2 - nombre NDI (Enter para aplicar)",
                        juce::dontSendNotification);
    send2_label.setColour(juce::Label::textColourId,
                          juce::Colours::white.withAlpha(0.6f));
    send2_label.setJustificationType(juce::Justification::bottomLeft);
    send2_label.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(send2_label);

    send2_name.setMultiLine(false);
    send2_name.setReturnKeyStartsNewLine(false);
    send2_name.setScrollbarsShown(false);
    send2_name.applyFontToAllText(lf.getPopupMenuFont()); // misma tipografia que el emisor 1
    send2_name.setFont(lf.getPopupMenuFont());
    send2_name.setText(ap.getSender2().getName(), juce::dontSendNotification);
    send2_name.addListener(this);
    addAndMakeVisible(send2_name);

    send2_device.setJustificationType(juce::Justification::centredLeft);
    send2_device.setTextWhenNothingSelected("Elige la tarjeta de sonido del emisor 2");
    send2_device.setTextWhenNoChoicesAvailable("No hay dispositivos de entrada");
    send2_device.addListener(this);
    addAndMakeVisible(send2_device);

    send2_button.setClickingTogglesState(true);
    send2_button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0e0e0e));
    send2_button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffc93c3c));
    send2_button.setColour(juce::TextButton::textColourOffId,
                           juce::Colours::white.withAlpha(0.85f));
    send2_button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    send2_button.addListener(this);
    addAndMakeVisible(send2_button);

    send2_settings.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0e0e0e));
    send2_settings.setColour(juce::TextButton::textColourOffId,
                             juce::Colours::white.withAlpha(0.85f));
    send2_settings.addListener(this);
    addAndMakeVisible(send2_settings);

    addAndMakeVisible(send2_meter);
    addAndMakeVisible(send2_status);

    // el emisor 2 solo existe en la app independiente
    if (!juce::JUCEApplicationBase::isStandaloneApp())
        for (auto* c : std::initializer_list<juce::Component*>{
                 &send2_label, &send2_name, &send2_device, &send2_button,
                 &send2_settings, &send2_meter, &send2_status})
            c->setVisible(false);

    refreshDevices(true);

    rxAttachment.reset(
        new ButtonAttachment(ap.getAPVTS(), "recv", *juce__recvButton));
    txAttachment.reset(
        new ButtonAttachment(ap.getAPVTS(), "send", *juce__sendButton));
    comboboxAttachment.reset(
        new ComboBoxAttachment(ap.getAPVTS(), "ndi_recv", *combobox_sources));

    //[/UserPreSize]

    setSize (640, 550);


    //[Constructor] You can add your own custom stuff here..
    refreshSources();
    updateIndicators();
    startTimer(33); // ~30 fps para los medidores

    setResizable(true, false);
    setResizeLimits(480, 413, INT16_MAX, INT16_MAX);

    // if (juce::JUCEApplicationBase::isStandaloneApp())
    // {
    //     auto pluginHolder = juce::StandalonePluginHolder::getInstance();

    //     if (pluginHolder)
    //     {
    // pluginHolder->getMuteInputValue().setValue(false);
    // pluginHolder->processorHasPotentialFeedbackLoop = false;
    // pluginHolder->muteInput.store(false);
    //     }
    // }
    //[/Constructor]
}

NdiAudioProcessorEditor::~NdiAudioProcessorEditor()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    settings2_window = nullptr; // su selector usa el gestor del procesador
    rxAttachment = nullptr;
    txAttachment = nullptr;
    comboboxAttachment = nullptr;
    //[/Destructor_pre]

    combobox_sources = nullptr;
    juce__textEditor = nullptr;
    juce__sendButton = nullptr;
    juce__recvButton = nullptr;
    drawable1 = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    // LookAndFeel::setDefaultLookAndFeel(nullptr);
    //[/Destructor]
}

//==============================================================================
void NdiAudioProcessorEditor::paint (juce::Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (juce::Colour (0xff0e0e0e));

    {
        int x = proportionOfWidth (0.2703f), y = proportionOfHeight (0.1727f), width = proportionOfWidth (0.2000f), height = proportionOfHeight (0.1018f);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (juce::Colours::black);
        jassert (drawable1 != nullptr);
        if (drawable1 != nullptr)
            drawable1->drawWithin (g, juce::Rectangle<int> (x, y, width, height).toFloat(),
                                   juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.000f);
    }

    {
        int x = proportionOfWidth (0.6078f), y = proportionOfHeight (0.1436f), width = proportionOfWidth (0.3422f), height = proportionOfHeight (0.1145f);
        juce::String text (TRANS("Audio IO"));
        juce::Colour fillColour = juce::Colour (0xff6257ff);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        g.setFont(
            juce::Font("Arial",
                       static_cast<float>(jmin(proportionOfWidth(0.09375f),
                                               proportionOfHeight(0.1091f))),
                       juce::Font::plain)
                .withTypefaceStyle("Black"));
        g.setColour(fillColour);
        g.drawText(text, x, y, width, height, juce::Justification::centred,
                   true);
    #if 0
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (juce::Font ("Arial", 57.40f, juce::Font::plain).withTypefaceStyle ("Black"));
        g.drawText (text, x, y, width, height,
                    juce::Justification::centred, true);
    }

    //[UserPaint] Add your own custom painting code here..
#endif
    }
    //[/UserPaint]
}

void NdiAudioProcessorEditor::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    combobox_sources->setBounds (proportionOfWidth (0.1375f), proportionOfHeight (0.7063f), proportionOfWidth (0.4500f), proportionOfHeight (0.0917f));
    juce__textEditor->setBounds (proportionOfWidth (0.1375f), proportionOfHeight (0.4354f), proportionOfWidth (0.4500f), proportionOfHeight (0.0896f));
    juce__sendButton->setBounds (proportionOfWidth (0.6750f), proportionOfHeight (0.4354f), proportionOfWidth (0.2141f), proportionOfHeight (0.0917f));
    juce__recvButton->setBounds (proportionOfWidth (0.6750f), proportionOfHeight (0.7063f), proportionOfWidth (0.2141f), proportionOfHeight (0.0917f));
    //[UserResized] Add your own custom resize handling here..
    {
        // Distribucion en pixeles sobre un diseno de 640 x 550; todo escala
        // con el tamano de la ventana.
        auto Y = [this](float px)
        { return juce::roundToInt(px * (float) getHeight() / 550.0f); };

        const auto x  = proportionOfWidth (0.1375f);
        const auto w  = proportionOfWidth (0.4500f);
        const auto bx = proportionOfWidth (0.6750f);
        const auto bw = proportionOfWidth (0.2141f);
        const auto fs = juce::jlimit (10.0f, 13.0f, (float) getHeight() * 0.0235f);

        for (auto* l : {&send_label, &send2_label, &recv_label})
            l->setFont (juce::Font (fs));

        // ----- emisor 1 (dispositivo principal) -----
        send_label.setBounds (x, Y (158), w, Y (20));
        juce__textEditor->setBounds (x, Y (180), w, Y (38));
        juce__sendButton->setBounds (bx, Y (180), bw, Y (38));
        send_meter.setBounds (x, Y (222), w, Y (12));
        send_status.setBounds (x, Y (237), w, Y (22));

        // ----- emisor 2 (otro dispositivo) -----
        send2_label.setBounds (x, Y (272), w, Y (20));
        send2_name.setBounds (x, Y (294), w, Y (38));
        send2_button.setBounds (bx, Y (294), bw, Y (38));
        send2_device.setBounds (x, Y (336), w, Y (34));
        send2_settings.setBounds (bx, Y (336), bw, Y (34));
        send2_meter.setBounds (x, Y (374), w, Y (12));
        send2_status.setBounds (x, Y (389), w, Y (22));

        // ----- recepcion -----
        recv_label.setBounds (x, Y (424), w, Y (20));
        combobox_sources->setBounds (x, Y (446), w, Y (38));
        juce__recvButton->setBounds (bx, Y (446), bw, Y (38));
        recv_meter.setBounds (x, Y (488), w, Y (12));
        recv_status.setBounds (x, Y (503), w, Y (22));
    }
    //[/UserResized]
}

void NdiAudioProcessorEditor::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    //[UsercomboBoxChanged_Pre]
    std::scoped_lock lk(editor_mutex);
    //[/UsercomboBoxChanged_Pre]

    if (comboBoxThatHasChanged == combobox_sources.get())
    {
        //[UserComboBoxCode_combobox_sources] -- add your combo box handling code here..
        //code here..
        // code here..
        if (timer_update == true)
        {
            return;
        }

        auto text = combobox_sources->getText();

        if (text.isEmpty() || text == prev_text)
            return;

        prev_text = text;

        ap.parseRecvTextInput(text);
        //[/UserComboBoxCode_combobox_sources]
    }

    else if (comboBoxThatHasChanged == &send2_device)
    {
        auto& s2 = ap.getSender2();
        s2.setDeviceName(send2_device.getText());
        if (s2.isWanted())
            s2.reopenDevice();
    }

    //[UsercomboBoxChanged_Post]
    //[/UsercomboBoxChanged_Post]
}

void NdiAudioProcessorEditor::buttonClicked (juce::Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    std::scoped_lock lk(editor_mutex);
    //[/UserbuttonClicked_Pre]

    if (buttonThatWasClicked == juce__sendButton.get())
    {
        //[UserButtonCode_juce__sendButton] -- add your button handler code here..
        //here..
        // here..
        //[/UserButtonCode_juce__sendButton]
    }
    else if (buttonThatWasClicked == juce__recvButton.get())
    {
        //[UserButtonCode_juce__recvButton] -- add your button handler code here..
        //here..
        // here..
        //[/UserButtonCode_juce__recvButton]
    }

    else if (buttonThatWasClicked == &send2_settings)
    {
        openSender2Settings();
    }
    else if (buttonThatWasClicked == &send2_button)
    {
        auto& s2 = ap.getSender2();
        if (send2_button.getToggleState())
        {
            s2.setName(send2_name.getText());
            s2.setDeviceName(send2_device.getText());
            s2.start(); // si la configuracion no es valida, el boton vuelve a "ENVIAR"
        }
        else
        {
            s2.stop();
        }
    }

    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
//other code here...
// other code here...
// any
void NdiAudioProcessorEditor::textEditorReturnKeyPressed(TextEditor& editor)
{
    std::scoped_lock lk(editor_mutex);

    if (&editor == &send2_name)
    {
        auto& s2 = ap.getSender2();
        s2.setName(send2_name.getText());
        if (s2.isWanted())
            s2.start(); // reinicia con el nombre nuevo
        send2_name.setText(s2.getName(), juce::dontSendNotification);
        return;
    }

    if (&editor != this->juce__textEditor.get())
        return;

    if (editor.getText().isNotEmpty() &&
        editor.getText() == ap.getNDISendTextInput())
        return;

    ap.parseSendTextInput(editor.getText());
    ap.parameterChanged("send",
                        ap.getAPVTS().getRawParameterValue("send")->load());
    editor.setText(ap.getNDISendTextInput());
}

void NdiAudioProcessorEditor::timerCallback()
{
    updateIndicators();

    // la lista de fuentes se refresca cada ~1 s
    if (++source_refresh_ticks >= 30)
    {
        source_refresh_ticks = 0;
        refreshSources();
    }

    // la lista de tarjetas de sonido, cada ~5 s (solo si el emisor 2 esta
    // apagado, para no tocar un dispositivo en uso)
    if (++device_scan_ticks >= 150)
    {
        device_scan_ticks = 0;
        refreshDevices(false);
    }
}

void NdiAudioProcessorEditor::openSender2Settings()
{
    if (settings2_window != nullptr)
    {
        settings2_window->toFront(true);
        return;
    }

    auto& s2 = ap.getSender2();

    // si aun no hay tarjeta elegida, se parte de la primera de la lista
    if (s2.getDeviceName().isEmpty() && send2_device.getNumItems() > 0)
        s2.setDeviceName(send2_device.getItemText(0));

    if (s2.getDeviceName().isEmpty())
        return; // no hay dispositivos de entrada

    s2.beginSettings();

    settings2_window = std::make_unique<Sender2SettingsWindow>(
        s2.getDeviceManager(),
        getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
        [this]() { closeSender2Settings(); });
}

void NdiAudioProcessorEditor::closeSender2Settings()
{
    // se cierra en diferido: la ventana no puede borrarse a si misma
    juce::MessageManager::callAsync(
        [safe = juce::Component::SafePointer<NdiAudioProcessorEditor>(this)]()
        {
            if (auto* editor = safe.getComponent())
            {
                editor->ap.getSender2().endSettings();
                editor->settings2_window = nullptr;
                editor->refreshDevices(false);
            }
        });
}

void NdiAudioProcessorEditor::refreshDevices(bool force)
{
    if (!juce::JUCEApplicationBase::isStandaloneApp())
        return;

    auto& s2 = ap.getSender2();

    // con el emisor 2 activo no se reescanean las tarjetas (hay una en uso):
    // solo se actualiza la seleccion
    juce::StringArray names;
    if (s2.isWanted() && !force)
    {
        for (int i = 0; i < send2_device.getNumItems(); ++i)
            names.add(send2_device.getItemText(i));
    }
    else
    {
        names = s2.scanInputDevices();
    }

    // si el dispositivo guardado no esta conectado, se deja en la lista igual
    const auto saved = s2.getDeviceName();
    if (saved.isNotEmpty() && !names.contains(saved))
        names.add(saved);

    // solo se reconstruye si cambio algo (no cierra el desplegable abierto)
    juce::StringArray current;
    for (int i = 0; i < send2_device.getNumItems(); ++i)
        current.add(send2_device.getItemText(i));

    if (current != names)
    {
        send2_device.clear(juce::dontSendNotification);
        for (int i = 0; i < names.size(); ++i)
            send2_device.addItem(names[i], i + 1);
    }

    const auto idx = names.indexOf(saved);
    send2_device.setSelectedId(idx >= 0 ? idx + 1 : 0, juce::dontSendNotification);
}

void NdiAudioProcessorEditor::updateIndicators()
{
    using State = StatusIndicator::State;

    const auto send_peak = ap.takeSendPeak();
    const auto recv_peak = ap.takeRecvPeak();

    // el texto de los botones sigue su estado (activo = detener)
    juce__sendButton->setButtonText(juce__sendButton->getToggleState() ? "DETENER" : "ENVIAR");
    juce__recvButton->setButtonText(juce__recvButton->getToggleState() ? "DETENER" : "RECIBIR");

    // ---------------- emisor 2 ----------------
    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        auto& s2 = ap.getSender2();
        const bool wanted = s2.isWanted();
        const bool running = s2.isAudioRunning();
        const int conns = s2.getConnections();
        const float peak2 = s2.takePeak();

        send2_button.setToggleState(wanted, juce::dontSendNotification);
        send2_button.setButtonText(wanted ? "DETENER" : "ENVIAR");

        send2_meter.setActive(wanted && running);
        if (wanted && running)
            send2_meter.pushLevel(peak2);

        if (!wanted)
        {
            send2_status.setStatus(s2.getError().isNotEmpty() ? State::error : State::off,
                                   s2.getError().isNotEmpty() ? s2.getError()
                                                              : String("Emisor 2 detenido"));
        }
        else if (!running)
        {
            const auto err = s2.getError();
            const auto retry = s2.getRetries() > 0
                                   ? " (reintento " + String(s2.getRetries()) + ")"
                                   : String();
            send2_status.setStatus(err.isNotEmpty() ? State::error : State::waiting,
                                   (err.isNotEmpty() ? err : String("Reconectando dispositivo")) + retry);
        }
        else if (conns <= 0)
            send2_status.setStatus(State::waiting, "Emitiendo - sin receptores conectados");
        else
            send2_status.setStatus(State::ok, "Emitiendo - " + String(conns) +
                                                  (conns == 1 ? " receptor conectado"
                                                              : " receptores conectados"));
    }

    if (!ap.getNDILib())
    {
        send_meter.setActive(false);
        recv_meter.setActive(false);
        send_status.setStatus(State::error, "NDI Runtime no encontrado");
        recv_status.setStatus(State::error, "NDI Runtime no encontrado");
        return;
    }

    const bool audio_running = ap.isAudioRunning();
    const int tx = ap.getSendConnections();
    const int rx = ap.getRecvConnections();

    // ---------------- envio ----------------
    send_meter.setActive(tx >= 0 && audio_running);
    if (tx >= 0 && audio_running)
        send_meter.pushLevel(send_peak);

    if (tx < 0)
        send_status.setStatus(State::off, "Emisor detenido");
    else if (!audio_running)
        send_status.setStatus(State::waiting, "Sin flujo de audio");
    else if (tx == 0)
        send_status.setStatus(State::waiting, "Emitiendo - sin receptores conectados");
    else
        send_status.setStatus(State::ok, "Emitiendo - " + String(tx) +
                                             (tx == 1 ? " receptor conectado"
                                                      : " receptores conectados"));

    // ---------------- recepcion ----------------
    recv_meter.setActive(rx > 0 && audio_running);
    if (rx > 0 && audio_running)
        recv_meter.pushLevel(recv_peak);

    // cuenta ~5 s de silencio continuo mientras hay conexion
    if (rx > 0 && audio_running && recv_peak < 0.00003f)
        ++recv_silent_ticks;
    else
        recv_silent_ticks = 0;

    // Reintento de conexion: NDI ya reconecta solo cuando la fuente vuelve,
    // pero si pasan ~15 s buscando se recrea el receptor para asegurar.
    if (rx == 0 && audio_running && ap.getNDIRecvName().isNotEmpty())
    {
        if (++recv_search_ticks >= 450)
        {
            recv_search_ticks = 0;
            ++recv_retries;
            ap.parameterChanged("recv", 1.0f);
        }
    }
    else
    {
        recv_search_ticks = 0;
        if (rx != 0)
            recv_retries = 0;
    }

    if (rx < 0)
    {
        recv_status.setStatus(State::off, "Receptor detenido");
    }
    else
    {
        const auto name = ap.getNDIRecvName();

        if (name.isEmpty())
            recv_status.setStatus(State::waiting, "Elige una fuente");
        else if (!audio_running)
            recv_status.setStatus(State::waiting, "Sin flujo de audio");
        else if (rx == 0)
            recv_status.setStatus(State::waiting,
                                  "Buscando: " + name +
                                      (recv_retries > 0
                                           ? " (reintento " + String(recv_retries) + ")"
                                           : String()));
        else if (recv_silent_ticks > 150)
            recv_status.setStatus(State::ok, "Conectado (sin audio entrante): " + name);
        else
            recv_status.setStatus(State::ok, "Conectado: " + name);
    }
}

void NdiAudioProcessorEditor::refreshSources()
{
    if (!ap.getNDILib())
        return;

    // actual NDI send name
    auto this_tx_name = ap.getNDISendName2();

    uint32_t no_sources;
    const NDIlib_source_t* p_sources =
        ap.getNDILib()->find_get_current_sources(ap.getNDIFind(), &no_sources);

    combobox_sources->clear();
    for (uint32_t i = 0; i < no_sources; i++)
    {
        String ndi_name = p_sources[i].p_ndi_name;
        if (ndi_name != this_tx_name)
            combobox_sources->addItem(p_sources[i].p_ndi_name, (int)i + 1);
    }

    for (auto i = 0; i < combobox_sources->getNumItems(); i++)
    {
        auto combobox_item_text = combobox_sources->getItemText(i);
        auto ndi_recv_name = ap.getNDIRecvName();
        if (combobox_item_text == ndi_recv_name)
        {
            timer_update = true;
            combobox_sources->setText(
                ap.getNDIRecvTextInput(),
                juce::NotificationType::sendNotificationSync);
            timer_update = false;
            break;
        }
    }
    combobox_sources->setTextWhenNothingSelected(
        ap.getNDIRecvName().isNotEmpty() ? ap.getNDIRecvTextInput()
                                         : "no source");
}
//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Projucer information section --

    This is where the Projucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="NdiAudioProcessorEditor"
                 componentName="" parentClasses="public juce::AudioProcessorEditor, private juce::Timer, public juce::TextEditor::Listener"
                 constructorParams="NdiAudioProcessor&amp; p" variableInitialisers="AudioProcessorEditor (&amp;p)&#10;ap (p)&#10;"
                 snapPixels="8" snapActive="1" snapShown="1" overlayOpacity="0.330"
                 fixedSize="1" initialWidth="640" initialHeight="480">
  <BACKGROUND backgroundColour="ff0e0e0e">
    <IMAGE pos="27.031% 19.792% 20% 11.667%" resource="ndiLogoMasterLight_svg"
           opacity="1.0" mode="2"/>
    <TEXT pos="60.781% 16.458% 34.219% 13.125%" fill="solid: ff6257ff"
          hasStroke="0" text="Audio IO" fontname="Arial" fontsize="57.4"
          kerning="0.0" bold="0" italic="0" justification="36" typefaceStyle="Black"/>
  </BACKGROUND>
  <COMBOBOX name="ndi_sources" id="8a9f4410678088ea" memberName="combobox_sources"
            virtualName="" explicitFocusOrder="0" pos="13.75% 70.625% 45% 9.167%"
            editable="1" layout="33" items="" textWhenNonSelected="no source"
            textWhenNoItems="no sources"/>
  <TEXTEDITOR name="new text editor" id="c6b28ec6d037d807" memberName="juce__textEditor"
              virtualName="" explicitFocusOrder="0" pos="13.75% 43.542% 45% 8.958%"
              initialText="" multiline="0" retKeyStartsLine="0" readonly="0"
              scrollbars="1" caret="1" popupmenu="1"/>
  <TEXTBUTTON name="send" id="8ea332196abf1fb" memberName="juce__sendButton"
              virtualName="" explicitFocusOrder="0" pos="67.5% 43.542% 21.406% 9.167%"
              bgColOff="ff0e0e0e" bgColOn="ff6257ff" buttonText="send" connectedEdges="0"
              needsCallback="1" radioGroupId="0"/>
  <TEXTBUTTON name="recv" id="1fc1e6a5a8d1dd8f" memberName="juce__recvButton"
              virtualName="" explicitFocusOrder="0" pos="67.5% 70.625% 21.406% 9.167%"
              bgColOff="ff0e0e0e" bgColOn="ff6257ff" buttonText="recv" connectedEdges="0"
              needsCallback="1" radioGroupId="0"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif

//==============================================================================
// Binary resources - be careful not to edit any of these sections!

// JUCER_RESOURCE: ndiLogoMasterLight_svg, 1760, "../Source"
static const unsigned char resource_NdiAudioProcessorEditor_ndiLogoMasterLight_svg[] = { 60,115,118,103,32,119,105,100,116,104,61,34,49,48,52,55,34,32,104,101,105,103,104,116,61,34,53,53,48,34,32,118,
105,101,119,66,111,120,61,34,48,32,48,32,49,48,52,55,32,53,53,48,34,32,102,105,108,108,61,34,110,111,110,101,34,32,120,109,108,110,115,61,34,104,116,116,112,58,47,47,119,119,119,46,119,51,46,111,114,103,
47,50,48,48,48,47,115,118,103,34,62,10,60,112,97,116,104,32,100,61,34,77,50,48,52,46,52,49,52,32,49,51,54,46,49,52,49,86,52,49,51,46,51,50,72,50,55,50,46,52,56,49,86,50,57,54,46,50,48,55,67,50,55,50,46,
52,56,49,32,50,56,56,46,56,54,53,32,50,55,54,46,56,57,54,32,50,56,51,46,55,50,53,32,50,56,51,46,56,56,55,32,50,56,51,46,55,50,53,67,50,56,57,46,55,55,52,32,50,56,51,46,55,50,53,32,50,57,51,46,56,50,49,
32,50,56,55,46,48,50,57,32,51,48,48,46,52,52,52,32,50,57,55,46,51,48,57,76,51,55,51,46,54,54,51,32,52,49,51,46,51,50,72,52,52,55,46,57,56,53,86,49,51,54,46,49,52,49,72,51,55,57,46,57,49,56,86,50,53,52,
46,55,50,50,67,51,55,57,46,57,49,56,32,50,54,50,46,48,54,53,32,51,55,53,46,53,48,50,32,50,54,55,46,50,48,52,32,51,54,56,46,53,49,50,32,50,54,55,46,50,48,52,67,51,54,50,46,54,50,53,32,50,54,55,46,50,48,
52,32,51,53,56,46,53,55,55,32,50,54,51,46,57,32,51,53,49,46,57,53,53,32,50,53,51,46,54,50,49,76,50,55,56,46,55,51,54,32,49,51,54,46,49,52,49,72,50,48,52,46,52,49,52,90,34,32,102,105,108,108,61,34,35,70,
49,70,49,70,49,34,47,62,10,60,112,97,116,104,32,100,61,34,77,55,49,55,46,52,48,51,32,50,55,52,46,53,52,55,67,55,49,55,46,52,48,51,32,49,57,56,46,49,56,53,32,54,53,51,46,48,49,53,32,49,51,54,46,49,52,49,
32,53,55,52,46,50,55,55,32,49,51,54,46,49,52,49,72,52,54,52,46,54,51,51,86,52,49,51,46,51,50,72,53,55,52,46,50,55,55,67,54,53,51,46,51,56,51,32,52,49,51,46,51,50,32,55,49,55,46,52,48,51,32,51,53,48,46,
57,48,57,32,55,49,55,46,52,48,51,32,50,55,52,46,53,52,55,90,77,54,52,55,46,52,57,54,32,50,55,52,46,49,56,67,54,52,55,46,52,57,54,32,51,49,56,46,54,48,50,32,54,49,52,46,55,53,32,51,53,48,46,57,48,57,32,
53,54,57,46,56,54,50,32,51,53,48,46,57,48,57,72,53,52,49,46,53,57,54,67,53,51,54,46,54,56,51,32,51,53,48,46,57,48,57,32,53,51,50,46,55,48,49,32,51,52,54,46,57,51,53,32,53,51,50,46,55,48,49,32,51,52,50,
46,48,51,52,86,50,48,55,46,52,50,55,67,53,51,50,46,55,48,49,32,50,48,50,46,53,50,54,32,53,51,54,46,54,56,51,32,49,57,56,46,53,53,50,32,53,52,49,46,53,57,54,32,49,57,56,46,53,53,50,72,53,54,57,46,56,54,
50,67,54,49,52,46,55,53,32,49,57,56,46,53,53,50,32,54,52,55,46,52,57,54,32,50,51,48,46,56,53,57,32,54,52,55,46,52,57,54,32,50,55,52,46,49,56,90,34,32,102,105,108,108,61,34,35,70,49,70,49,70,49,34,47,62,
10,60,112,97,116,104,32,100,61,34,77,55,51,50,46,49,55,51,32,49,51,54,46,49,52,49,86,52,49,51,46,51,50,72,56,48,48,46,50,52,49,86,49,51,54,46,49,52,49,72,55,51,50,46,49,55,51,90,34,32,102,105,108,108,
61,34,35,70,49,70,49,70,49,34,47,62,10,60,112,97,116,104,32,100,61,34,77,56,50,56,46,50,52,49,32,51,56,54,46,49,57,53,67,56,50,48,46,51,54,54,32,51,56,54,46,49,57,53,32,56,49,52,46,50,52,49,32,51,57,50,
46,49,52,53,32,56,49,52,46,50,52,49,32,51,57,57,46,55,50,57,67,56,49,52,46,50,52,49,32,52,48,55,46,51,55,32,56,50,48,46,51,54,54,32,52,49,51,46,51,50,32,56,50,56,46,50,52,49,32,52,49,51,46,51,50,67,56,
51,54,46,49,49,54,32,52,49,51,46,51,50,32,56,52,50,46,50,52,49,32,52,48,55,46,51,55,32,56,52,50,46,50,52,49,32,51,57,57,46,55,50,57,67,56,52,50,46,50,52,49,32,51,57,50,46,49,52,53,32,56,51,54,46,49,49,
54,32,51,56,54,46,49,57,53,32,56,50,56,46,50,52,49,32,51,56,54,46,49,57,53,90,77,56,50,56,46,50,52,49,32,51,56,56,46,52,55,67,56,51,52,46,54,53,55,32,51,56,56,46,52,55,32,56,51,57,46,54,55,52,32,51,57,
51,46,51,55,32,56,51,57,46,54,55,52,32,51,57,57,46,55,50,57,67,56,51,57,46,54,55,52,32,52,48,54,46,48,56,55,32,56,51,52,46,54,53,55,32,52,49,49,46,48,52,53,32,56,50,56,46,50,52,49,32,52,49,49,46,48,52,
53,67,56,50,49,46,56,50,52,32,52,49,49,46,48,52,53,32,56,49,54,46,56,48,55,32,52,48,54,46,48,56,55,32,56,49,54,46,56,48,55,32,51,57,57,46,55,50,57,67,56,49,54,46,56,48,55,32,51,57,51,46,51,55,32,56,50,
49,46,56,50,52,32,51,56,56,46,52,55,32,56,50,56,46,50,52,49,32,51,56,56,46,52,55,90,77,56,51,51,46,56,57,57,32,51,57,54,46,56,49,50,67,56,51,51,46,56,57,57,32,51,57,52,46,51,48,52,32,56,51,49,46,55,57,
57,32,51,57,50,46,52,57,53,32,56,50,56,46,56,56,50,32,51,57,50,46,52,57,53,72,56,50,51,46,54,51,50,86,52,48,55,46,48,50,72,56,50,54,46,50,53,55,86,52,48,49,46,49,50,57,76,56,51,48,46,57,50,52,32,52,48,
55,46,48,50,72,56,51,51,46,57,53,55,76,56,51,48,46,50,56,50,32,52,48,50,46,52,55,67,56,51,48,46,48,52,57,32,52,48,50,46,49,55,57,32,56,50,57,46,57,51,50,32,52,48,49,46,56,50,57,32,56,50,57,46,57,51,50,
32,52,48,49,46,53,51,55,67,56,50,57,46,57,51,50,32,52,48,49,46,48,55,32,56,51,48,46,49,54,54,32,52,48,48,46,55,50,32,56,51,48,46,54,57,49,32,52,48,48,46,54,48,52,72,56,51,48,46,55,52,57,67,56,51,50,46,
54,55,52,32,52,48,48,46,49,51,55,32,56,51,51,46,56,57,57,32,51,57,56,46,54,55,57,32,56,51,51,46,56,57,57,32,51,57,54,46,56,49,50,90,77,56,51,49,46,49,53,55,32,51,57,54,46,57,50,57,67,56,51,49,46,49,53,
55,32,51,57,56,46,50,49,50,32,56,51,48,46,50,50,52,32,51,57,56,46,57,55,32,56,50,56,46,55,54,54,32,51,57,56,46,57,55,72,56,50,54,46,50,53,55,86,51,57,52,46,56,50,57,72,56,50,56,46,55,54,54,67,56,51,48,
46,50,50,52,32,51,57,52,46,56,50,57,32,56,51,49,46,49,53,55,32,51,57,53,46,54,52,53,32,56,51,49,46,49,53,55,32,51,57,54,46,57,50,57,90,34,32,102,105,108,108,61,34,35,70,49,70,49,70,49,34,47,62,10,60,47,
115,118,103,62,10,0,0};

const char* NdiAudioProcessorEditor::ndiLogoMasterLight_svg = (const char*) resource_NdiAudioProcessorEditor_ndiLogoMasterLight_svg;
const int NdiAudioProcessorEditor::ndiLogoMasterLight_svgSize = 1760;


//[EndFile] You can add extra defines here...
//[/EndFile]

