// Post-Grape — colours, fonts, images and the look-and-feel (from ui-list.png).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pg::ui
{
namespace col
{
const juce::Colour winTop { 0xfffbf2f8 }, win { 0xfff6eaf3 }, winBottom { 0xffefdcea };
const juce::Colour panelTop { 0xfffbf5f9 }, panelBottom { 0xfff6e9f3 };
const juce::Colour line { 0xffe2c6dc }, line2 { 0xffd6aed0 }, soft { 0xffebcde6 };
const juce::Colour pink { 0xffd36fae }, pink2 { 0xffe79bcf }, hot { 0xffc2458f }, arc { 0xffe58cc6 };
const juce::Colour lav { 0xffb98ac0 }, lav2 { 0xffa56fb0 }, purple { 0xff8a4fb0 };
const juce::Colour plum { 0xff6b3074 }, deep { 0xff3d1849 }, ink { 0xff2f1240 }, tick { 0xff5b2864 };
const juce::Colour muted { 0xff8e6a92 }, faint { 0xffb394b6 }, teal { 0xff3f9b9b };
const juce::Colour ledG { 0xff9ccf86 }, ledY { 0xfff3d27a }, ledO { 0xfff2a35f }, ledR { 0xffef7a8a },
    ledP { 0xffc58ad6 }, ledOff { 0xffe6d6e3 };
const juce::Colour screenTop { 0xfffdf8fc }, screenBottom { 0xfff5e8f2 }, grid { 0xffeddbe8 };
} // namespace col

enum class Weight { Bold, ExtraBold, Black };

juce::Font font (float height, Weight w = Weight::Bold);

// Decoration images (from ui-list.png).
struct Images
{
    juce::Image grapes, leaf, leafSmall, swirl, vine;
    static const Images& get();
};

// Draw an image with its width = w, at the top-left point (x, y).
void drawImageW (juce::Graphics& g, const juce::Image& img, float x, float y, float w, float alpha = 1.0f);

// Small rounded "chip" with text (as in the display legends).
void drawChip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, float fontH);

// Screen background (rounded, inner shadow).
void drawScreen (juce::Graphics& g, juce::Rectangle<float> r, float z);

// Text width for a font.
float textWidth (const juce::Font& f, const juce::String& s);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle,
                           float endAngle, juce::Slider&) override;

    juce::Font getPopupMenuFont() override { return font (15.0f, Weight::Bold); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getAlertWindowTitleFont() override { return font (18.0f, Weight::Black); }
    juce::Font getAlertWindowMessageFont() override { return font (15.0f, Weight::Bold); }
    juce::Font getAlertWindowFont() override { return font (14.0f, Weight::Bold); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (15.0f, h * 0.55f), Weight::ExtraBold); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawAlertBox (juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea, juce::TextLayout&) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool over, bool dragging) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
};
} // namespace pg::ui
