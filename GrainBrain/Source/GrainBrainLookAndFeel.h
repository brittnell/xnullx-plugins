#pragma once
#include <JuceHeader.h>

//==============================================================================
// Global colour constants  (used throughout editor + processor)
//==============================================================================
static const juce::Colour bgDark = juce::Colour(0xFF080808);
static const juce::Colour bgMid = juce::Colour(0xFF141414);
static const juce::Colour bgPanel = juce::Colour(0xFF1E1E1E);
static const juce::Colour textColour = juce::Colour(0xFFCCCCCC);
static const juce::Colour dimText = juce::Colour(0xFF555555);

static const juce::Colour bandColours[4] = {
    juce::Colour(0xFFFFCC00),   // Low    – gold
    juce::Colour(0xFFFF44CC),   // LowMid – pink
    juce::Colour(0xFF00DDFF),   // HiMid  – cyan
    juce::Colour(0xFFAA55FF)    // High   – violet
};

//==============================================================================
// Helper — +60 Photoshop Lightness ≈ 55% blend toward white
//==============================================================================
inline juce::Colour midiMappedColour(juce::Colour base)
{
    return base.interpolatedWith(juce::Colours::white, 0.55f);
}

//==============================================================================
// GrainBrainLookAndFeel
//==============================================================================
class GrainBrainLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GrainBrainLookAndFeel()
    {
        setColour(juce::PopupMenu::backgroundColourId, bgPanel);
        setColour(juce::PopupMenu::textColourId, textColour);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xFF333333));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::AlertWindow::backgroundColourId, bgPanel);
        setColour(juce::AlertWindow::textColourId, textColour);
        setColour(juce::AlertWindow::outlineColourId, juce::Colour(0xFF444444));
        setColour(juce::TextEditor::backgroundColourId, bgMid);
        setColour(juce::TextEditor::textColourId, textColour);
        setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF444444));
        setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xFF888888));
        setColour(juce::TextButton::buttonColourId, bgPanel);
        setColour(juce::TextButton::textColourOffId, textColour);
        setColour(juce::ComboBox::backgroundColourId, bgMid);
        setColour(juce::ComboBox::textColourId, textColour);
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF333333));
        setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xFF444444));
    }

    //==========================================================================
    // Rotary slider
    //==========================================================================
    void drawRotarySlider(juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPosProportional,
        float rotaryStartAngle, float rotaryEndAngle,
        juce::Slider& slider) override
    {
        // ── Accent colour ────────────────────────────────────────────────────
        juce::Colour accent = juce::Colour(0xFF00AAFF);
        if (auto* v = slider.getProperties().getVarPointer("accentColour"))
            accent = juce::Colour((juce::uint32)(int)*v);

        // ── MIDI state ───────────────────────────────────────────────────────
        int  midiCC = (int)slider.getProperties().getWithDefault("midiCC", -1);
        bool isLearning = (bool)slider.getProperties().getWithDefault("midiLearning", false);

        juce::Colour arcColour = (midiCC >= 0) ? midiMappedColour(accent) : accent;

        // Learning pulse (sine wave over 1-second period)
        float learnAlpha = 0.f;
        if (isLearning)
        {
            double ms = (double)(juce::Time::currentTimeMillis() % 1000);
            learnAlpha = 0.5f + 0.5f * (float)std::sin(ms / 1000.0 * juce::MathConstants<double>::twoPi);
        }

        // ── Geometry ─────────────────────────────────────────────────────────
        auto   bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
        float  cx = bounds.getCentreX();
        float  cy = bounds.getCentreY();
        float  radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.42f;
        float  toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const  float trackW = 3.5f;

        // ── Track ring ───────────────────────────────────────────────────────
        {
            juce::Path p;
            p.addCentredArc(cx, cy, radius, radius, 0.f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(juce::Colour(0xFF2A2A2A));
            g.strokePath(p, juce::PathStrokeType(trackW, juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
        }

        // ── Value arc ────────────────────────────────────────────────────────
        if (sliderPosProportional > 0.001f)
        {
            juce::Path p;
            p.addCentredArc(cx, cy, radius, radius, 0.f, rotaryStartAngle, toAngle, true);
            g.setColour(arcColour);
            g.strokePath(p, juce::PathStrokeType(trackW, juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
        }

        // ── Learning glow ring ───────────────────────────────────────────────
        if (isLearning)
        {
            juce::Path p;
            p.addCentredArc(cx, cy, radius + 5.f, radius + 5.f,
                0.f, 0.f, juce::MathConstants<float>::twoPi, true);
            g.setColour(arcColour.withAlpha(learnAlpha * 0.7f));
            g.strokePath(p, juce::PathStrokeType(2.5f));
        }

        // ── Knob body ────────────────────────────────────────────────────────
        float knobR = radius * 0.60f;
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillEllipse(cx - knobR, cy - knobR, knobR * 2.f, knobR * 2.f);
        g.setColour(juce::Colour(0xFF303030));
        g.drawEllipse(cx - knobR, cy - knobR, knobR * 2.f, knobR * 2.f, 0.8f);

        // ── Pointer ──────────────────────────────────────────────────────────
        float px = cx + (knobR * 0.62f) * std::sin(toAngle);
        float py = cy - (knobR * 0.62f) * std::cos(toAngle);
        g.setColour(arcColour);
        g.drawLine(cx, cy, px, py, 1.8f);
        g.fillEllipse(px - 2.f, py - 2.f, 4.f, 4.f);

        // ── CC number in arc gap (bottom centre, between arc endpoints) ───────
        if (midiCC >= 0)
        {
            // The arc gap is at the bottom — position just below the arc endpoint
            float gapCY = cy + radius * 1.05f;
            g.setFont(juce::Font(juce::FontOptions("Courier New", 7.f, juce::Font::bold)));
            g.setColour(arcColour);
            g.drawText(juce::String(midiCC),
                juce::Rectangle<int>((int)(cx - 14.f), (int)(gapCY - 5.f), 28, 10),
                juce::Justification::centred, false);
        }
    }

    //==========================================================================
    // Horizontal linear slider
    //==========================================================================
    void drawLinearSlider(juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPos, float /*minPos*/, float /*maxPos*/,
        juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                sliderPos, 0.f, 0.f, style, slider);
            return;
        }

        juce::Colour accent = juce::Colour(0xFF00AAFF);
        if (auto* v = slider.getProperties().getVarPointer("accentColour"))
            accent = juce::Colour((juce::uint32)(int)*v);

        int midiCC = (int)slider.getProperties().getWithDefault("midiCC", -1);
        juce::Colour fill = (midiCC >= 0) ? midiMappedColour(accent) : accent;

        const float trackH = 3.f;
        float       trackY = y + height * 0.5f - trackH * 0.5f;

        // Track bg
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRoundedRectangle((float)x, trackY, (float)width, trackH, 1.5f);

        // Track fill
        float fillW = sliderPos - (float)x;
        if (fillW > 0.f)
        {
            g.setColour(fill);
            g.fillRoundedRectangle((float)x, trackY, fillW, trackH, 1.5f);
        }

        // Thumb
        const float thumbW = 8.f, thumbH = 14.f;
        g.setColour(fill);
        g.fillRoundedRectangle(sliderPos - thumbW * 0.5f,
            y + height * 0.5f - thumbH * 0.5f,
            thumbW, thumbH, 2.f);
    }

    //==========================================================================
    // Buttons
    //==========================================================================
    void drawButtonBackground(juce::Graphics& g, juce::Button& btn,
        const juce::Colour& bg,
        bool isHighlighted, bool isDown) override
    {
        auto bounds = btn.getLocalBounds().toFloat().reduced(0.5f);
        juce::Colour c = bg;
        if (isHighlighted) c = c.brighter(0.15f);
        if (isDown)        c = c.darker(0.15f);
        g.setColour(c);
        g.fillRoundedRectangle(bounds, 3.f);
        g.setColour(c.brighter(0.25f));
        g.drawRoundedRectangle(bounds, 3.f, 0.7f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& btn,
        bool, bool) override
    {
        juce::Colour tc = btn.findColour(btn.getToggleState()
            ? juce::TextButton::textColourOnId
            : juce::TextButton::textColourOffId);
        g.setColour(tc);
        g.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::bold)));
        g.drawFittedText(btn.getButtonText(), btn.getLocalBounds(),
            juce::Justification::centred, 1);
    }

    //==========================================================================
    // ComboBox
    //==========================================================================
    void drawComboBox(juce::Graphics& g, int w, int h, bool,
        int, int, int, int, juce::ComboBox& box) override
    {
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(0.f, 0.f, (float)w, (float)h, 3.f);
        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(0.5f, 0.5f, (float)w - 1.f, (float)h - 1.f, 3.f, 0.7f);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font(juce::FontOptions("Courier New", 10.f, juce::Font::plain));
    }

    juce::Font getLabelFont(juce::Label&) override
    {
        return juce::Font(juce::FontOptions("Courier New", 10.f, juce::Font::plain));
    }

    juce::Font getTextButtonFont(juce::TextButton&, int) override
    {
        return juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::bold));
    }
};