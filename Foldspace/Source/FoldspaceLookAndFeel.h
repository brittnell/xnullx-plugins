#pragma once
#include <JuceHeader.h>

//==============================================================================
// Foldspace — Dune-flavoured stage palette
//   Family language shared with GrainBrain / O2 / Bunka: dark base, Courier
//   New, arc knobs, dim labels + accent values. Foldspace is single-band but
//   multi-STAGE, so colour codes the signal path instead of frequency bands:
//     violet = MODULATOR   (what drives the phase)
//     blue   = PM          (the modulation itself)
//     spice  = FOLD        (the wavefolder — primary accent, logo S)
//     red    = FEEDBACK    (the dangerous part)
//==============================================================================
static const juce::Colour fsBgDark  = juce::Colour(0xFF080808);
static const juce::Colour fsBgMid   = juce::Colour(0xFF141414);
static const juce::Colour fsBgPanel = juce::Colour(0xFF1E1E1E);
static const juce::Colour fsText    = juce::Colour(0xFFCCCCCC);
static const juce::Colour fsDim     = juce::Colour(0xFF888888);

static const juce::Colour fsViolet  = juce::Colour(0xFF9B6BFF);   // MODULATOR
static const juce::Colour fsBlue    = juce::Colour(0xFF4D7CFF);   // PM
static const juce::Colour fsSpice   = juce::Colour(0xFFFFAA22);   // FOLD / primary
static const juce::Colour fsRed     = juce::Colour(0xFFFF4455);   // FEEDBACK

//==============================================================================
class FoldspaceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FoldspaceLookAndFeel()
    {
        setColour(juce::PopupMenu::backgroundColourId,            fsBgPanel);
        setColour(juce::PopupMenu::textColourId,                  fsText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xFF333333));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
        setColour(juce::AlertWindow::backgroundColourId,          fsBgPanel);
        setColour(juce::AlertWindow::textColourId,                fsText);
        setColour(juce::AlertWindow::outlineColourId,             juce::Colour(0xFF444444));
        setColour(juce::TextEditor::backgroundColourId,           fsBgMid);
        setColour(juce::TextEditor::textColourId,                 fsText);
        setColour(juce::TextEditor::outlineColourId,              juce::Colour(0xFF444444));
        setColour(juce::TextEditor::focusedOutlineColourId,       fsSpice);
        setColour(juce::TextButton::buttonColourId,               fsBgPanel);
        setColour(juce::TextButton::textColourOffId,              fsDim);
        setColour(juce::TextButton::textColourOnId,               juce::Colours::black);
        setColour(juce::ComboBox::backgroundColourId,             fsBgMid);
        setColour(juce::ComboBox::textColourId,                   fsText);
        setColour(juce::ComboBox::outlineColourId,                juce::Colour(0xFF333333));
        setColour(juce::ScrollBar::thumbColourId,                 juce::Colour(0xFF444444));
        setColour(juce::Label::textColourId,                      fsDim);
    }

    //==========================================================================
    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        juce::Colour accent = fsSpice;
        if (auto* v = slider.getProperties().getVarPointer("accentColour"))
            accent = juce::Colour((juce::uint32)(int)*v);

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
          g.setColour(accent);
          g.strokePath(p, juce::PathStrokeType(trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }

        float kr = radius * 0.60f;
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillEllipse(cx - kr, cy - kr, kr * 2.f, kr * 2.f);
        g.setColour(juce::Colour(0xFF303030));
        g.drawEllipse(cx - kr, cy - kr, kr * 2.f, kr * 2.f, 0.8f);

        float px = cx + (kr * 0.62f) * std::sin(toAngle);
        float py = cy - (kr * 0.62f) * std::cos(toAngle);
        g.setColour(accent);
        g.drawLine(cx, cy, px, py, 1.8f);
        g.fillEllipse(px - 2.f, py - 2.f, 4.f, 4.f);
    }

    //==========================================================================
    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos, float, float,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        { LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0.f, 0.f, style, slider); return; }

        juce::Colour accent = fsSpice;
        if (auto* v = slider.getProperties().getVarPointer("accentColour"))
            accent = juce::Colour((juce::uint32)(int)*v);

        const float th = 3.f;
        float ty = y + height * 0.5f - th * 0.5f;
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.fillRoundedRectangle((float)x, ty, (float)width, th, 1.5f);
        float fw = sliderPos - (float)x;
        if (fw > 0.f) { g.setColour(accent); g.fillRoundedRectangle((float)x, ty, fw, th, 1.5f); }
        const float tw = 8.f, twh = 14.f;
        g.setColour(accent);
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
