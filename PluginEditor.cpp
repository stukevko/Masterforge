#include "PluginEditor.h"

using namespace mfui;

// =============================================================================
//  LookAndFeel
// =============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Label::textColourId, col::dim);
    setColour (juce::ComboBox::backgroundColourId, col::panel);
    setColour (juce::ComboBox::outlineColourId, col::edge);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::arrowColourId, col::accent);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.3f));
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::TextButton::buttonColourId, col::panel);
    setColour (juce::TextButton::buttonOnColourId, col::accent2);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, col::bg);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);
    const bool enabled = s.isEnabled();

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, a1, true);
    g.setColour (col::edge);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? a0 + (float) ((0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (a1 - a0) : a0;
    juce::Path val;
    val.addCentredArc (c.x, c.y, r - 2, r - 2, 0, std::min (from, ang), std::max (from, ang), true);
    g.setColour (enabled ? col::accent : col::dim);
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (col::bg);
    g.fillEllipse (juce::Rectangle<float> (r * 1.3f, r * 1.3f).withCentre (c));
    g.setColour (col::text.withAlpha (enabled ? 1.0f : 0.4f));
    const juce::Point<float> tip (c.x + std::sin (ang) * r * 0.55f, c.y - std::cos (ang) * r * 0.55f);
    g.drawLine ({ c.getPointOnCircumference (r * 0.2f, ang), tip }, 2.0f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (2);
    const bool onState = b.getToggleState();
    g.setColour (onState ? col::accent : col::edge);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    const float d = r.getHeight() - 4;
    g.setColour (col::text);
    g.fillEllipse (onState ? r.getRight() - d - 2 : r.getX() + 2, r.getY() + 2, d, d);
}

// =============================================================================
//  Panel
// =============================================================================
Panel::Panel (juce::AudioProcessorValueTreeState& s, const char* t, const juce::String& onId,
              const std::vector<std::pair<const char*, const char*>>& knobs, const juce::String& comboId,
              bool powerDisablesKnobs)
    : title (juce::String::fromUTF8 (t))
{
    if (onId.isNotEmpty())
    {
        addAndMakeVisible (power);
        powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, onId, power);
        if (powerDisablesKnobs)
            power.onStateChange = [this]
            {
                for (auto* sl : sliders) sl->setEnabled (power.getToggleState());
                if (combo) combo->setEnabled (power.getToggleState());
            };
    }
    if (comboId.isNotEmpty())
    {
        combo = std::make_unique<juce::ComboBox>();
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (comboId)))
            combo->addItemList (cp->choices, 1);
        addAndMakeVisible (*combo);
        comboAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, comboId, *combo);
    }
    for (auto& [id, name] : knobs)
    {
        auto* sl = sliders.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow));
        sl->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        addAndMakeVisible (sl);
        sliderAtts.add (new juce::AudioProcessorValueTreeState::SliderAttachment (s, id, *sl));
        sl->setNumDecimalPlacesToDisplay (1);
        sl->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        sl->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        sl->setColour (juce::Slider::textBoxTextColourId, col::text);

        auto* lb = labels.add (new juce::Label ({}, juce::String::fromUTF8 (name)));
        lb->setJustificationType (juce::Justification::centred);
        lb->setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (lb);
    }
    if (power.onStateChange) power.onStateChange();
}

void Panel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3);
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 8);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r, 8, 1);
    g.setColour (col::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (title, getLocalBounds().reduced (12, 8).removeFromTop (18), juce::Justification::centredLeft);
}

void Panel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto head = r.removeFromTop (20);
    power.setBounds (head.removeFromRight (36).withSizeKeepingCentre (34, 18));
    if (combo) { r.removeFromTop (6); combo->setBounds (r.removeFromTop (22)); }
    r.removeFromTop (4);

    const int n = sliders.size();
    if (n == 0) return;
    const int w = r.getWidth() / n;
    for (int i = 0; i < n; ++i)
    {
        auto cell = r.removeFromLeft (w);
        auto knob = cell.withSizeKeepingCentre (std::min (cell.getWidth(), 86), std::min (cell.getHeight() - 16, 104));
        sliders[i]->setBounds (knob);
        labels[i]->setBounds (cell.getX(), knob.getBottom(), cell.getWidth(), 16);
    }
}

// =============================================================================
//  Analyse-Anzeige
// =============================================================================
void AnalysisView::update()
{
    st = proc.lufsShort.load(); integ = proc.lufsInteg.load(); gain = proc.autoGain.load(); peak = proc.peakOut.load();
    corr = proc.correlation.load(); lo = proc.grLow.load(); mi = proc.grMid.load(); hi = proc.grHigh.load();
    gl = proc.grGlue.load(); lim = proc.grLim.load();
    for (size_t b = 0; b < eq.size(); ++b) eq[b] = proc.eqGains[b].load();
    repaint();
}

void AnalysisView::mouseDown (const juce::MouseEvent& e)
{
    if (lufsBox.contains (e.getPosition())) proc.resetIntegrated.store (true);
}

void AnalysisView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3);
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 8);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r, 8, 1);

    auto area = getLocalBounds().reduced (14, 10);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.setColour (col::text);
    g.drawText ("WAS DIE AUTOMATIK GERADE MACHT", area.removeFromTop (18), juce::Justification::centredLeft);
    area.removeFromTop (6);

    auto fmt = [] (float v, int dec = 1) { return v <= -99.f ? juce::String ("--") : juce::String (v, dec); };

    // --- LUFS-Block ---
    lufsBox = area.removeFromLeft (230);
    {
        auto b = lufsBox.reduced (0, 2);
        g.setColour (col::bg);
        g.fillRoundedRectangle (b.toFloat(), 6);
        b.reduce (12, 8);
        const float target = proc.apvts.getRawParameterValue ("lufsTarget")->load();
        const bool autoOn = proc.apvts.getRawParameterValue ("loudAuto")->load() > 0.5f;
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (11.5f));
        g.drawText ("LAUTHEIT (Short-Term)", b.removeFromTop (14), juce::Justification::centredLeft);
        g.setColour (std::abs (st - target) < 1.0f || ! autoOn ? col::accent : col::accent2);
        g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
        g.drawText (fmt (st) + " LUFS", b.removeFromTop (42), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (12.0f));
        g.setColour (col::text);
        g.drawText ("Integriert: " + fmt (integ) + " LUFS", b.removeFromTop (17), juce::Justification::centredLeft);
        g.drawText (autoOn ? "Ziel: " + juce::String (target, 1) + " LUFS" : juce::String ("Ziel: manuell"), b.removeFromTop (17), juce::Justification::centredLeft);
        g.drawText ("Auto-Gain: " + juce::String (gain >= 0 ? "+" : "") + juce::String (gain, 1) + " dB", b.removeFromTop (17), juce::Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (juce::String::fromUTF8 ("Klick = Integriert zurücksetzen"), b.removeFromBottom (14), juce::Justification::centredLeft);
    }
    area.removeFromLeft (18);

    // --- Tonal-EQ-Balken ---
    auto eqArea = area.removeFromLeft ((area.getWidth() - 18) / 2);
    static const char* names[] { "60", "150", "400", "1k", "2.5k", "6k", "12k" };
    const int bw = eqArea.getWidth() / (int) eq.size();
    const float mid = (float) eqArea.getY() + (eqArea.getHeight() - 16) * 0.5f;
    const float scale = (eqArea.getHeight() - 16) * 0.5f / 6.0f;
    g.setColour (col::edge);
    g.drawHorizontalLine ((int) mid, (float) eqArea.getX(), (float) eqArea.getRight());
    for (size_t b = 0; b < eq.size(); ++b)
    {
        auto cell = eqArea.withX (eqArea.getX() + (int) b * bw).withWidth (bw);
        const float v = std::clamp (eq[b], -6.0f, 6.0f) * scale;
        g.setColour (v >= 0 ? col::accent : col::accent2);
        g.fillRoundedRectangle ((float) cell.getX() + bw * 0.2f, v >= 0 ? mid - v : mid, bw * 0.6f, std::max (1.0f, std::abs (v)), 2);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (names[b], cell.removeFromBottom (14), juce::Justification::centred);
    }
    area.removeFromLeft (18);

    // --- Gain Reduction & Stereo ---
    const int rowH = area.getHeight() / 7;
    auto row = [&] (const juce::String& name, float t, juce::Colour c, const juce::String& txt)
    {
        auto line = area.removeFromTop (rowH);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (name, line.removeFromLeft (84), juce::Justification::centredLeft);
        g.drawText (txt, line.removeFromRight (58), juce::Justification::centredRight);
        auto bar = line.reduced (4, std::max (0, line.getHeight() / 2 - 3)).toFloat();
        g.setColour (col::edge);
        g.fillRoundedRectangle (bar, 3);
        g.setColour (c);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, t)), 3);
    };
    auto gr = [&] (const char* n, float v, float maxDb) { row (juce::String::fromUTF8 (n), v / maxDb, col::accent2, "-" + juce::String (v, 1) + " dB"); };
    gr ("Comp Bass",  lo, 8);
    gr ("Comp Mitten", mi, 8);
    gr ("Comp Höhen", hi, 8);
    gr ("Glue",       gl, 8);
    gr ("Limiter",    lim, 10);
    row ("Stereo-Korr.", (corr + 1) * 0.5f, corr < 0 ? col::warn : col::accent, juce::String (corr, 2));
    row ("Peak Out", (peak + 30) / 30, peak > -0.05f ? col::warn : col::accent, fmt (peak) + " dB");
}

// =============================================================================
//  Editor
// =============================================================================
MasterForgeEditor::MasterForgeEditor (MasterForgeProcessor& p)
    : AudioProcessorEditor (&p), proc (p), analysis (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    panels.add (new Panel (s, "INPUT & GENRE", {}, { { "inGain", "Gain" } }, "genre"));
    panels.add (new Panel (s, "AUTO TONAL-EQ", "eqOn", { { "eqAmount", "Stärke" }, { "eqTilt", "Dunkel/Hell" }, { "eqLow", "Bass" },
                                                         { "eqMid", "Präsenz" }, { "eqAir", "Air" } }));
    panels.add (new Panel (s, "AUTO MULTIBAND", "mbOn", { { "mbAmount", "Stärke" }, { "mbSpeed", "Tempo" } }));
    panels.add (new Panel (s, "GLUE COMP", "glueOn",    { { "glueAmount", "Stärke" }, { "glueSpeed", "Tempo" }, { "glueMix", "Mix" } }));
    panels.add (new Panel (s, "SÄTTIGUNG", "satOn",     { { "satDrive", "Drive" } }));
    panels.add (new Panel (s, "STEREO", "stOn",         { { "stWidth", "Breite" }, { "stMono", "Bass Mono" }, { "stFocus", "Vocal Focus" } }));
    panels.add (new Panel (s, "AUTO LOUDNESS", "loudAuto", { { "lufsTarget", "Ziel LUFS" }, { "clip", "Clipper" }, { "push", "Push (manuell)" }, { "ceiling", "Ceiling" } }, {}, false));
    panels.add (new Panel (s, "OUTPUT", {},              { { "outTrim", "Trim" } }));
    for (auto* pn : panels) addAndMakeVisible (pn);
    addAndMakeVisible (analysis);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0 && id <= proc.getNumPrograms()) proc.setCurrentProgram (id - 1);
    };
    addAndMakeVisible (presetBox);

    compareBtn.setClickingTogglesState (true);
    compareBtn.setTooltip (juce::String::fromUTF8 ("Original mit gleicher Lautheit anhören"));
    compareAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, "compare", compareBtn);
    saveBtn.onClick = [this] { saveUserPreset(); };
    loadBtn.onClick = [this] { loadUserPreset(); };
    for (auto* b : { &compareBtn, &saveBtn, &loadBtn }) addAndMakeVisible (b);

    setResizable (true, true);
    setResizeLimits (960, 600, 1900, 1150);
    setSize (1140, 680);
    startTimerHz (30);
}

MasterForgeEditor::~MasterForgeEditor() { setLookAndFeel (nullptr); }

juce::File MasterForgeEditor::presetFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("MasterForge Presets");
    f.createDirectory();
    return f;
}

void MasterForgeEditor::saveUserPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Preset speichern", presetFolder().getChildFile ("Mein Master.mfpreset"), "*.mfpreset");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;
            if (auto xml = proc.apvts.copyState().createXml())
                xml->writeTo (file.withFileExtension ("mfpreset"));
        });
}

void MasterForgeEditor::loadUserPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Preset laden", presetFolder(), "*.mfpreset");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            if (auto xml = juce::XmlDocument::parse (fc.getResult()))
                if (xml->hasTagName (proc.apvts.state.getType()))
                {
                    proc.apvts.replaceState (juce::ValueTree::fromXml (*xml));
                    presetBox.setText (fc.getResult().getFileNameWithoutExtension(), juce::dontSendNotification);
                }
        });
}

void MasterForgeEditor::timerCallback() { analysis.update(); }

void MasterForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto head = getLocalBounds().reduced (16, 10).removeFromTop (36);
    g.setColour (col::accent);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("MASTERFORGE", head, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String::fromUTF8 ("Auto Mastering für Beat + Vocals"), head.withTrimmedLeft (182), juce::Justification::centredLeft);
}

void MasterForgeEditor::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    auto head = r.removeFromTop (36);
    loadBtn.setBounds (head.removeFromRight (80).reduced (2, 4));
    saveBtn.setBounds (head.removeFromRight (90).reduced (2, 4));
    compareBtn.setBounds (head.removeFromRight (120).reduced (2, 4));
    presetBox.setBounds (head.removeFromRight (300).reduced (6, 4));
    r.removeFromTop (6);

    const int rowH = (r.getHeight() - 190) / 2;
    auto row1 = r.removeFromTop (rowH);
    auto row2 = r.removeFromTop (rowH);
    auto layout = [] (juce::Rectangle<int> row, std::initializer_list<std::pair<Panel*, int>> items)
    {
        int total = 0;
        for (auto& it : items) total += it.second;
        const int unit = row.getWidth() / total;
        for (auto& it : items) it.first->setBounds (row.removeFromLeft (unit * it.second));
    };
    layout (row1, { { panels[0], 2 }, { panels[1], 5 }, { panels[2], 2 }, { panels[3], 3 } });
    layout (row2, { { panels[4], 2 }, { panels[5], 3 }, { panels[6], 4 }, { panels[7], 2 } });
    analysis.setBounds (r);
}
