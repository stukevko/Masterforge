#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using NR  = juce::NormalisableRange<float>;

// -----------------------------------------------------------------------------
//  Genre-Zielkurven für den Auto Tonal-EQ (relative Bandenergie in dB)
//  Bänder: 60 | 150 | 400 | 1k | 2.5k | 6k | 12k
// -----------------------------------------------------------------------------
static constexpr float kGenreTargets[][mf::TonalEQ::N] {
    {  4.0f, 2.0f, -1.0f, -3.0f, -5.0f, -8.0f, -12.0f },   // Hip-Hop / Trap
    {  1.5f, 1.0f, -1.0f, -2.0f, -4.0f, -6.0f,  -9.0f },   // Pop
    {  3.0f, 2.0f,  0.0f, -2.0f, -5.0f, -8.0f, -12.0f },   // R&B / Soul
    {  0.0f, 1.5f,  0.0f, -1.0f, -3.0f, -6.0f, -10.0f },   // Rock
    {  4.5f, 1.0f, -2.0f, -3.0f, -4.0f, -6.0f,  -9.0f },   // EDM / Club
    {  2.0f, 2.0f,  1.0f, -2.0f, -6.0f,-10.0f, -16.0f },   // Lo-Fi
    { -1.0f, 1.0f,  0.0f, -1.0f, -3.0f, -6.0f,  -9.0f },   // Akustik
};

const juce::StringArray& MasterForgeProcessor::genreNames()
{
    static const juce::StringArray names { "Hip-Hop / Trap", "Pop", "R&B / Soul", "Rock", "EDM / Club", "Lo-Fi", "Akustik" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout MasterForgeProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    auto pct = [] (const char* id, const char* name, float def, float hi = 100.f)
    {
        return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, NR (0.f, hi, 0.1f), def,
                                      juce::AudioParameterFloatAttributes().withLabel ("%"));
    };
    auto db = [] (const char* id, const char* name, float lo, float hi, float def)
    {
        return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, NR (lo, hi, 0.1f), def,
                                      juce::AudioParameterFloatAttributes().withLabel ("dB"));
    };
    auto sw = [] (const char* id, const char* name, bool def)
    {
        return std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def);
    };

    v.push_back (db ("inGain", "Input Gain", -12.f, 12.f, 0.f));
    v.push_back (std::make_unique<APC> (juce::ParameterID { "genre", 1 }, "Genre", genreNames(), 0));

    v.push_back (sw ("eqOn", "Tonal EQ On", true));
    v.push_back (pct ("eqAmount", "Tonal EQ Amount", 60.f));
    v.push_back (std::make_unique<APF> (juce::ParameterID { "eqTilt", 1 }, "Tone (Dark/Bright)", NR (-1.f, 1.f, 0.01f), 0.f));
    v.push_back (db ("eqLow", "Bass", -4.f, 4.f, 0.f));
    v.push_back (db ("eqMid", "Presence", -4.f, 4.f, 0.f));
    v.push_back (db ("eqAir", "Air", -4.f, 4.f, 0.f));

    v.push_back (sw ("mbOn", "Multiband On", true));
    v.push_back (pct ("mbAmount", "Multiband Amount", 45.f));
    v.push_back (pct ("mbSpeed", "Multiband Speed", 50.f));

    v.push_back (sw ("glueOn", "Glue On", true));
    v.push_back (pct ("glueAmount", "Glue Amount", 40.f));
    v.push_back (pct ("glueSpeed", "Glue Speed", 40.f));
    v.push_back (pct ("glueMix", "Glue Mix", 100.f));

    v.push_back (sw ("satOn", "Saturation On", true));
    v.push_back (pct ("satDrive", "Saturation Drive", 15.f));

    v.push_back (sw ("stOn", "Stereo On", true));
    v.push_back (pct ("stWidth", "Stereo Width", 100.f, 200.f));
    v.push_back (std::make_unique<APF> (juce::ParameterID { "stMono", 1 }, "Bass Mono", NR (20.f, 300.f, 1.f, 0.6f), 120.f,
                                        juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    v.push_back (pct ("stFocus", "Vocal Focus", 20.f));

    v.push_back (sw ("loudAuto", "Auto Loudness", true));
    v.push_back (std::make_unique<APF> (juce::ParameterID { "lufsTarget", 1 }, "Target Loudness", NR (-16.f, -5.f, 0.1f), -9.f,
                                        juce::AudioParameterFloatAttributes().withLabel ("LUFS")));
    v.push_back (pct ("clip", "Clipper", 30.f));
    v.push_back (db ("push", "Manual Push", 0.f, 18.f, 6.f));
    v.push_back (db ("ceiling", "Ceiling", -3.f, -0.1f, -1.f));
    v.push_back (db ("outTrim", "Output Trim", -12.f, 0.f, 0.f));
    v.push_back (sw ("compare", "A/B Compare", false));

    return { v.begin(), v.end() };
}

// -----------------------------------------------------------------------------
//  Werks-Presets
// -----------------------------------------------------------------------------
struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };

static const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        { "Hip-Hop / Trap – laut & fett",   { { "genre", 0 }, { "lufsTarget", -8.f }, { "mbAmount", 55.f }, { "glueAmount", 45.f }, { "satDrive", 25.f },
                                              { "stMono", 140.f }, { "stWidth", 110.f }, { "stFocus", 30.f }, { "eqLow", 1.f }, { "clip", 55.f } } },
        { "Pop – modern & klar",            { { "genre", 1 }, { "lufsTarget", -9.f }, { "satDrive", 15.f }, { "stWidth", 115.f }, { "stFocus", 35.f },
                                              { "eqAir", 1.f }, { "clip", 35.f } } },
        { "R&B / Soul – warm & smooth",     { { "genre", 2 }, { "lufsTarget", -10.f }, { "satDrive", 25.f }, { "eqTilt", -0.15f }, { "glueAmount", 40.f },
                                              { "glueSpeed", 25.f }, { "stFocus", 25.f }, { "clip", 25.f } } },
        { "Rock – dicht & druckvoll",       { { "genre", 3 }, { "lufsTarget", -9.f }, { "glueAmount", 55.f }, { "mbAmount", 60.f }, { "satDrive", 35.f },
                                              { "stWidth", 110.f }, { "clip", 45.f } } },
        { "EDM / Club – maximal",           { { "genre", 4 }, { "lufsTarget", -7.f }, { "mbAmount", 60.f }, { "mbSpeed", 65.f }, { "satDrive", 30.f },
                                              { "stMono", 150.f }, { "stWidth", 125.f }, { "clip", 70.f } } },
        { "Lo-Fi – soft & vintage",         { { "genre", 5 }, { "lufsTarget", -12.f }, { "eqTilt", -0.4f }, { "satDrive", 40.f }, { "stWidth", 90.f },
                                              { "mbAmount", 35.f }, { "clip", 15.f } } },
        { "Akustik / Ballade – dynamisch",  { { "genre", 6 }, { "lufsTarget", -12.f }, { "mbAmount", 30.f }, { "glueAmount", 30.f }, { "satDrive", 10.f },
                                              { "stFocus", 30.f }, { "clip", 0.f } } },
        { "Vocal nach vorne (Beat + Rap)",  { { "genre", 0 }, { "lufsTarget", -9.f }, { "stFocus", 70.f }, { "eqMid", 1.5f }, { "mbAmount", 50.f }, { "clip", 45.f } } },
        { "Streaming – Spotify (-14 LUFS)", { { "genre", 1 }, { "lufsTarget", -14.f }, { "mbAmount", 40.f }, { "clip", 15.f } } },
        { "Sanft – nur polieren",           { { "eqAmount", 30.f }, { "mbAmount", 25.f }, { "glueAmount", 25.f }, { "satDrive", 0.f }, { "stFocus", 0.f },
                                              { "lufsTarget", -11.f }, { "clip", 0.f } } },
    };
    return list;
}

int MasterForgeProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String MasterForgeProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String::fromUTF8 (presets()[(size_t) i].name) : juce::String();
}

void MasterForgeProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (rp->getParameterID() != "compare")
                rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, value] : presets()[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

// -----------------------------------------------------------------------------
MasterForgeProcessor::MasterForgeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    setCurrentProgram (0);   // startet mit dem ersten Preset (Hip-Hop / Trap)
}

bool MasterForgeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void MasterForgeProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    tonal.prepare (fs); multiband.prepare (fs); glue.prepare (fs); stereo.prepare (fs); limiter.prepare (fs);
    inMeter.prepare (fs); preMeter.prepare (fs); outMeter.prepare (fs);
    dryDelay.prepare (limiter.latency());
    setLatencySamples (limiter.latency());

    for (auto* b : { &sL, &sR, &rawL, &rawR }) b->assign (mf::kMaxChunk, 0.0f);
    inGain.reset (fs, 0.05);     inGain.setCurrentAndTargetValue (mf::dbToGain (p ("inGain")));
    outTrim.reset (fs, 0.05);    outTrim.setCurrentAndTargetValue (mf::dbToGain (p ("outTrim")));
    compareMix.reset (fs, 0.03); compareMix.setCurrentAndTargetValue (on ("compare") ? 1.f : 0.f);
    loudGainDb = on ("loudAuto") ? 0.f : p ("push");
    limiterLoss = 0; matchDb = 0;
}

void MasterForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch) buffer.clear (ch, 0, n);
    if (numOut == 0 || n == 0) return;

    if (resetIntegrated.exchange (false)) outMeter.resetIntegrated();

    inGain.setTargetValue (mf::dbToGain (p ("inGain")));
    outTrim.setTargetValue (mf::dbToGain (p ("outTrim")));
    compareMix.setTargetValue (on ("compare") ? 1.f : 0.f);

    const int genre = std::clamp ((int) p ("genre"), 0, (int) std::size (kGenreTargets) - 1);
    const bool eqOn = on ("eqOn"), mbOn = on ("mbOn"), glueOn = on ("glueOn"), satOn = on ("satOn"),
               stOn = on ("stOn"), autoLoud = on ("loudAuto");
    const float eqAmt = p ("eqAmount") * 0.01f, tilt = p ("eqTilt"), eqLow = p ("eqLow"), eqMid = p ("eqMid"), eqAir = p ("eqAir");
    const float mbAmt = p ("mbAmount") * 0.01f, mbSpd = p ("mbSpeed") * 0.01f;
    const float gAmt = p ("glueAmount") * 0.01f, gSpd = p ("glueSpeed") * 0.01f, gMix = p ("glueMix") * 0.01f;
    const float drive = p ("satDrive") * 0.01f;
    const float width = p ("stWidth") * 0.01f, monoF = p ("stMono"), focus = p ("stFocus") * 0.01f;
    const float target = p ("lufsTarget"), push = p ("push"), ceil = p ("ceiling"), clipAmt = p ("clip") * 0.01f;

    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    float peak = 0.f;

    for (int start = 0; start < n; start += mf::kMaxChunk)
    {
        const int len = std::min (mf::kMaxChunk, n - start);
        float* L = sL.data(); float* R = sR.data();
        for (int i = 0; i < len; ++i)
        {
            rawL[(size_t) i] = inL[start + i]; rawR[(size_t) i] = inR[start + i];
            const float g = inGain.getNextValue();
            L[i] = rawL[(size_t) i] * g; R[i] = rawR[(size_t) i] * g;
        }
        inMeter.process (rawL.data(), rawR.data(), len);

        tonal.process (L, R, len, kGenreTargets[genre], eqAmt, tilt, eqLow, eqMid, eqAir, eqOn);
        multiband.process (L, R, len, mbAmt, mbSpd, mbOn);
        glue.process (L, R, len, gAmt, gSpd, gMix, glueOn);
        mf::Saturator::process (L, R, len, drive, satOn);
        stereo.process (L, R, len, width, monoF, focus, stOn);
        preMeter.process (L, R, len);

        // ---- Auto Loudness: Gain so wählen, dass das Ergebnis den Ziel-LUFS trifft ----
        const float dt = (float) len / (float) fs;
        float desired = push;
        if (autoLoud)
            desired = preMeter.hasLongTerm() ? std::clamp (target - preMeter.longTermLufs() + limiterLoss, -12.f, 30.f) : loudGainDb;
        const float a = std::exp (-dt / 0.8f);
        const float newGain = a * loudGainDb + (1 - a) * desired;
        const float g0 = mf::dbToGain (loudGainDb), g1 = mf::dbToGain (newGain);
        for (int i = 0; i < len; ++i)
        {
            const float g = g0 + (g1 - g0) * (float) (i + 1) / (float) len;
            L[i] *= g; R[i] *= g;
        }
        loudGainDb = newGain;

        mf::SoftClipper::process (L, R, len, clipAmt, ceil);
        limiter.process (L, R, len, ceil);
        outMeter.process (L, R, len);

        // Was der Limiter an Lautheit "frisst", wird langsam nachgelernt
        if (autoLoud && preMeter.hasLongTerm() && outMeter.hasLongTerm())
        {
            const float loss = std::clamp (preMeter.longTermLufs() + loudGainDb - outMeter.longTermLufs(), 0.f, 14.f);
            limiterLoss += (loss - limiterLoss) * std::min (1.f, dt / 4.0f);
        }
        if (inMeter.hasLongTerm() && outMeter.hasLongTerm())
            matchDb += (std::clamp (outMeter.longTermLufs() - inMeter.longTermLufs(), -24.f, 24.f) - matchDb) * std::min (1.f, dt / 1.0f);

        // ---- Ausgabe (inkl. A/B-Vergleich mit gleicher Lautheit) ----
        const float match = mf::dbToGain (matchDb);
        for (int i = 0; i < len; ++i)
        {
            float dL = rawL[(size_t) i], dR = rawR[(size_t) i];
            dryDelay.process (dL, dR);
            const float t = outTrim.getNextValue(), c = compareMix.getNextValue();
            const float oL = L[i] * t * (1 - c) + dL * match * c;
            const float oR = R[i] * t * (1 - c) + dR * match * c;
            if (outR) { outL[start + i] = oL; outR[start + i] = oR; }
            else      { outL[start + i] = 0.5f * (oL + oR); }
            peak = std::max ({ peak, std::abs (oL), std::abs (oR) });
        }
    }

    lufsShort.store (outMeter.shortTerm());
    lufsInteg.store (outMeter.integrated());
    autoGain.store (loudGainDb);
    peakOut.store (mf::gainToDb (peak));
    grLow.store (multiband.grLow()); grMid.store (multiband.grMid()); grHigh.store (multiband.grHigh());
    grGlue.store (glue.getGr()); grLim.store (limiter.getGr());
    correlation.store (stereo.correlation());
    for (int b = 0; b < mf::TonalEQ::N; ++b) eqGains[(size_t) b].store (tonal.getGain (b));
}

// -----------------------------------------------------------------------------
void MasterForgeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void MasterForgeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* MasterForgeProcessor::createEditor() { return new MasterForgeEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MasterForgeProcessor(); }
