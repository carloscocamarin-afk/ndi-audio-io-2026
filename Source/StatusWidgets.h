#pragma once
#include <JuceHeader.h>

//==============================================================================
// Medidor de nivel horizontal (pico, con retencion y lampara de saturacion).
// Se alimenta ~30 veces por segundo desde el hilo de la interfaz.
//==============================================================================
class LevelMeter : public juce::Component
{
  public:
    static constexpr float floorDb = -60.0f;

    LevelMeter()
    {
        setInterceptsMouseClicks(false, false);
    }

    void setActive(bool shouldBeActive)
    {
        if (active == shouldBeActive)
            return;

        active = shouldBeActive;
        if (!active)
        {
            displayDb = holdDb = floorDb;
            holdTicks = clipTicks = 0;
        }
        repaint();
    }

    // peakLinear: pico absoluto (0..1) desde la ultima llamada.
    void pushLevel(float peakLinear)
    {
        const float db = juce::Decibels::gainToDecibels(peakLinear, floorDb);

        // ataque inmediato, caida suave
        if (db >= displayDb)
            displayDb = db;
        else
            displayDb = juce::jmax(db, displayDb - fallPerTick);

        // retencion del pico
        if (db >= holdDb)
        {
            holdDb = db;
            holdTicks = holdDuration;
        }
        else if (holdTicks > 0)
            --holdTicks;
        else
            holdDb = juce::jmax(displayDb, holdDb - fallPerTick);

        // lampara de saturacion (cerca de 0 dBFS)
        if (peakLinear >= 0.99f)
            clipTicks = clipDuration;
        else if (clipTicks > 0)
            --clipTicks;

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour(juce::Colour(0xff1a1a26));
        g.fillRoundedRectangle(bounds, 3.0f);

        auto inner = bounds.reduced(1.5f);
        auto clipArea = inner.removeFromRight(inner.getHeight());
        inner.removeFromRight(2.0f);

        auto toX = [&inner](float db)
        {
            const float p = juce::jlimit(0.0f, 1.0f, (db - floorDb) / (0.0f - floorDb));
            return inner.getX() + inner.getWidth() * p;
        };

        if (active)
        {
            const float x = toX(displayDb);
            if (x > inner.getX() + 0.5f)
            {
                juce::ColourGradient grad(juce::Colour(0xff2fbf71), inner.getX(), 0.0f,
                                          juce::Colour(0xffe5484d), inner.getRight(), 0.0f,
                                          false);
                grad.addColour(0.78, juce::Colour(0xff2fbf71)); // verde hasta -13 dB
                grad.addColour(0.90, juce::Colour(0xffe5a53d)); // ambar hasta -6 dB
                grad.addColour(0.96, juce::Colour(0xffe5484d)); // rojo cerca de 0 dB

                g.saveState();
                g.reduceClipRegion(inner.withRight(x).toNearestInt());
                g.setGradientFill(grad);
                g.fillRoundedRectangle(inner, 2.0f);
                g.restoreState();
            }

            if (holdDb > floorDb + 1.0f)
            {
                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.fillRect(toX(holdDb) - 1.0f, inner.getY(), 2.0f, inner.getHeight());
            }
        }

        // marcas de escala: -48, -36, -24, -12, -6 dB
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        for (float t : {-48.0f, -36.0f, -24.0f, -12.0f, -6.0f})
            g.fillRect(toX(t), inner.getBottom() - 3.0f, 1.0f, 3.0f);

        // lampara de saturacion
        g.setColour(clipTicks > 0 ? juce::Colour(0xffe5484d)
                                  : juce::Colour(0xff3a2226));
        g.fillRoundedRectangle(clipArea, 2.0f);
    }

  private:
    static constexpr float fallPerTick = 0.8f; // ~24 dB/s a 30 fps
    static constexpr int holdDuration = 45;    // ~1.5 s
    static constexpr int clipDuration = 90;    // ~3 s

    bool active {false};
    float displayDb {floorDb};
    float holdDb {floorDb};
    int holdTicks {0};
    int clipTicks {0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};

//==============================================================================
// Indicador de estado: punto de color + texto.
//   off     = gris   (detenido)
//   waiting = ambar  (activo pero sin conexion / sin audio)
//   ok      = verde  (conectado)
//   error   = rojo   (problema)
//==============================================================================
class StatusIndicator : public juce::Component
{
  public:
    enum class State
    {
        off,
        waiting,
        ok,
        error
    };

    StatusIndicator()
    {
        setInterceptsMouseClicks(false, false);
    }

    void setStatus(State newState, const juce::String& newText)
    {
        if (state == newState && text == newText)
            return;

        state = newState;
        text = newText;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        juce::Colour colour;
        switch (state)
        {
        case State::ok:
            colour = juce::Colour(0xff2fbf71);
            break;
        case State::waiting:
            colour = juce::Colour(0xffe5a53d);
            break;
        case State::error:
            colour = juce::Colour(0xffe5484d);
            break;
        case State::off:
        default:
            colour = juce::Colour(0xff6b6b7a);
            break;
        }

        const float d = juce::jmin(bounds.getHeight() * 0.55f, 12.0f);
        juce::Rectangle<float> dot(bounds.getX() + 3.0f,
                                   bounds.getCentreY() - d * 0.5f, d, d);

        g.setColour(colour);
        g.fillEllipse(dot);
        g.setColour(colour.withAlpha(0.35f));
        g.drawEllipse(dot.expanded(2.0f), 1.0f);

        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.setFont(juce::Font(juce::jlimit(10.0f, 14.0f, bounds.getHeight() * 0.7f)));
        g.drawText(text, bounds.withTrimmedLeft(d + 12.0f).toNearestInt(),
                   juce::Justification::centredLeft, true);
    }

  private:
    State state {State::off};
    juce::String text;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusIndicator)
};
