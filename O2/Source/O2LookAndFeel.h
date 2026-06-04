#pragma once
#include <JuceHeader.h>

//==============================================================================
// Global colours
//==============================================================================
static const juce::Colour o2BgDark    = juce::Colour(0xFF080808);
static const juce::Colour o2BgMid     = juce::Colour(0xFF141414);
static const juce::Colour o2BgPanel   = juce::Colour(0xFF1E1E1E);
static const juce::Colour o2Text      = juce::Colour(0xFFCCCCCC);
static const juce::Colour o2Dim       = juce::Colour(0xFF888888);  // brightened from 0xFF555555

static const juce::Colour o2BandColours[3] = {
    juce::Colour(0xFF00CCFF),   // Low  — ice blue
    juce::Colour(0xFF00FFAA),   // Mid  — mint green
    juce::Colour(0xFFFF6680),   // High — coral rose
};

inline juce::Colour o2MidiColour(juce::Colour base)
{
    return base.interpolatedWith(juce::Colours::white, 0.55f);
}

//==============================================================================
class O2LookAndFeel : public juce::LookAndFeel_V4
{
public:
    O2LookAndFeel()
    {
        setColour(juce::PopupMenu::backgroundColourId,            o2BgPanel);
        setColour(juce::PopupMenu::textColourId,                  o2Text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xFF333333));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
        setColour(juce::AlertWindow::backgroundColourId,          o2BgPanel);
        setColour(juce::AlertWindow::textColourId,                o2Text);
        setColour(juce::AlertWindow::outlineColourId,             juce::Colour(0xFF444444));
        setColour(juce::TextEditor::backgroundColourId,           o2BgMid);
        setColour(juce::TextEditor::textColourId,                 o2Text);
        setColour(juce::TextEditor::outlineColourId,              juce::Colour(0xFF444444));
        setColour(juce::TextEditor::focusedOutlineColourId,       juce::Colour(0xFF888888));
        setColour(juce::TextButton::buttonColourId,               o2BgPanel);
        setColour(juce::TextButton::textColourOffId,              o2Text);
        setColour(juce::ComboBox::backgroundColourId,             o2BgMid);
        setColour(juce::ComboBox::textColourId,                   o2Text);
        setColour(juce::ComboBox::outlineColourId,                juce::Colour(0xFF333333));
        setColour(juce::ScrollBar::thumbColourId,                 juce::Colour(0xFF444444));
    }

    //==========================================================================
    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        juce::Colour accent = juce::Colour(0xFF00CCFF);
        if (auto* v = slider.getProperties().getVarPointer("accentColour"))
            accent = juce::Colour((juce::uint32)(int)*v);

        int  midiCC     = (int)slider.getProperties().getWithDefault("midiCC",       -1);
        bool isLearning = (bool)slider.getProperties().getWithDefault("midiLearning", false);
        juce::Colour arcColour = (midiCC >= 0) ? o2MidiColour(accent) : accent;

        float learnAlpha = 0.f;
        if (isLearning)
        {
            double ms = (double)(juce::Time::currentTimeMillis() % 1000);
            learnAlpha = 0.5f + 0.5f * (float)std::sin(ms / 1000.0 * juce::MathConstants<double>::twoPi);
        }

        auto  bounds  = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
        float cx      = bounds.getCentreX();
        float cy      = bounds.getCentreY();
        float radius  = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.42f;
        float toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float trackW = 3.5f;

        { juce::Path p; p.addCentredArc(cx, cy, radius, radius, 0.f, rotaryStartAngle, rotaryEndAngle, true);
          g.setColour(juce::Colour(0xFF2A2A2A));
          g.strokePath(p, juce::PathStrokeType(trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }

        if (sliderPos > 0.001f)
        { juce::Path p; p.addCentredArc(cx, cy, radius, radius, 0.f, rotaryStartAngle, toAngle, true);
          g.setColour(arcColour);
          g.strokePath(p, juce::PathStrokeType(trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }

        if (isLearning)
        { juce::Path p; p.addCentredArc(cx, cy, radius + 5.f, radius + 5.f, 0.f, 0.f, juce::MathConstants<float>::twoPi, true);
          g.setColour(arcColour.withAlpha(learnAlpha * 0.7f));
          g.strokePath(p, juce::PathStrokeType(2.5f)); }

        float kr = radius * 0.60f;
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillEllipse(cx - kr, cy - kr, kr * 2.f, kr * 2.f);
        g.setColour(juce::Colour(0xFF303030));
        g.drawEllipse(cx - kr, cy - kr, kr * 2.f, kr * 2.f, 0.8f);

        float px = cx + (kr * 0.62f) * std::sin(toAngle);
        float py = cy - (kr * 0.62f) * std::cos(toAngle);
        g.setColour(arcColour);
        g.drawLine(cx, cy, px, py, 1.8f);
        g.fillEllipse(px - 2.f, py - 2.f, 4.f, 4.f);

        if (midiCC >= 0)
        {
            float gapCY = cy + radius * 1.05f;
            g.setFont(juce::Font(juce::FontOptions("Courier New", 7.f, juce::Font::bold)));
            g.setColour(arcColour);
            g.drawText(juce::String(midiCC),
                       juce::Rectangle<int>((int)(cx - 14.f), (int)(gapCY - 5.f), 28, 10),
                       juce::Justification::centred, false);
        }
    }

    //==========================================================================
    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos, float, float,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        { LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0.f, 0.f, style, slider); return; }

        juce::Colour accent = juce::Colour(0xFF00CCFF);
        if (auto* v = slider.getProperties().getVarPointer("accentColour"))
            accent = juce::Colour((juce::uint32)(int)*v);
        int midiCC = (int)slider.getProperties().getWithDefault("midiCC", -1);
        juce::Colour fill = (midiCC >= 0) ? o2MidiColour(accent) : accent;

        const float th = 3.f;
        float ty = y + height * 0.5f - th * 0.5f;
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRoundedRectangle((float)x, ty, (float)width, th, 1.5f);
        float fw = sliderPos - (float)x;
        if (fw > 0.f) { g.setColour(fill); g.fillRoundedRectangle((float)x, ty, fw, th, 1.5f); }
        const float tw = 8.f, twh = 14.f;
        g.setColour(fill);
        g.fillRoundedRectangle(sliderPos - tw * 0.5f, y + height * 0.5f - twh * 0.5f, tw, twh, 2.f);
    }

    //==========================================================================
    void drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                               const juce::Colour& bg, bool isHighlighted, bool isDown) override
    {
        auto b = btn.getLocalBounds().toFloat().reduced(0.5f);
        juce::Colour c = bg;
        if (isHighlighted) c = c.brighter(0.15f);
        if (isDown)        c = c.darker(0.15f);
        g.setColour(c); g.fillRoundedRectangle(b, 3.f);
        g.setColour(c.brighter(0.25f)); g.drawRoundedRectangle(b, 3.f, 0.7f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool, bool) override
    {
        juce::Colour tc = btn.findColour(btn.getToggleState()
            ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
        g.setColour(tc);
        g.setFont(juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::bold)));
        g.drawFittedText(btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred, 1);
    }

    void drawComboBox(juce::Graphics& g, int w, int h, bool,
                      int, int, int, int, juce::ComboBox& box) override
    {
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(0.f, 0.f, (float)w, (float)h, 3.f);
        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(0.5f, 0.5f, (float)w - 1.f, (float)h - 1.f, 3.f, 0.7f);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    { return juce::Font(juce::FontOptions("Courier New", 10.f, juce::Font::plain)); }

    juce::Font getLabelFont(juce::Label&) override
    { return juce::Font(juce::FontOptions("Courier New", 10.f, juce::Font::plain)); }

    juce::Font getTextButtonFont(juce::TextButton&, int) override
    { return juce::Font(juce::FontOptions("Courier New", 9.f, juce::Font::bold)); }
};
