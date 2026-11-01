#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace mfui
{
namespace col
{
    const juce::Colour bg      { 0xff121419 };
    const juce::Colour panel   { 0xff1c1f27 };
    const juce::Colour edge    { 0xff2b2f3a };
    const juce::Colour text    { 0xffe6e8ee };
    const juce::Colour dim     { 0xff8a90a0 };
    const juce::Colour accent  { 0xff35d6a4 };   // Mint
    const juce::Colour accent2 { 0xffffb547 };   // Gold
    const juce::Colour warn    { 0xffff5a5a };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hl, bool down) override;
};

class Panel : public juce::Component
{
public:
    Panel (juce::AudioProcessorValueTreeState&, const char* title, const juce::String& onId,
           const std::vector<std::pair<const char*, const char*>>& knobs, const juce::String& comboId = {},
           bool powerDisablesKnobs = true);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String title;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtts;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtt;
};

class AnalysisView : public juce::Component
{
public:
    explicit AnalysisView (MasterForgeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void update();

private:
    MasterForgeProcessor& proc;
    juce::Rectangle<int> lufsBox;
    float st = -100, integ = -100, gain = 0, peak = -100, corr = 1, lo = 0, mi = 0, hi = 0, gl = 0, lim = 0;
    std::array<float, mf::TonalEQ::N> eq {};
};
} // namespace mfui

class MasterForgeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MasterForgeEditor (MasterForgeProcessor&);
    ~MasterForgeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void saveUserPreset();
    void loadUserPreset();
    static juce::File presetFolder();

    MasterForgeProcessor& proc;
    mfui::LookAndFeel lnf;
    juce::ComboBox presetBox;
    juce::TextButton saveBtn { "Speichern" }, loadBtn { "Laden" }, compareBtn { "A/B Original" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> compareAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::OwnedArray<mfui::Panel> panels;
    mfui::AnalysisView analysis;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterForgeEditor)
};
