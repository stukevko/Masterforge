#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MasterDSP.h"

class MasterForgeProcessor : public juce::AudioProcessor
{
public:
    MasterForgeProcessor();
    ~MasterForgeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MasterForge"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const juce::StringArray& genreNames();
    juce::AudioProcessorValueTreeState apvts;

    // Anzeige-Werte
    std::atomic<float> lufsShort { -100.f }, lufsInteg { -100.f }, autoGain { 0.f }, peakOut { -100.f },
                       grLow { 0.f }, grMid { 0.f }, grHigh { 0.f }, grGlue { 0.f }, grLim { 0.f }, correlation { 1.f };
    std::array<std::atomic<float>, mf::TonalEQ::N> eqGains {};
    std::atomic<bool> resetIntegrated { false };

private:
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool  on (const char* id) const { return p (id) > 0.5f; }

    mf::TonalEQ tonal;
    mf::Multiband multiband;
    mf::Glue glue;
    mf::Stereo stereo;
    mf::Limiter limiter;
    mf::LoudnessMeter inMeter, preMeter, outMeter;
    mf::DelayLine dryDelay;

    juce::SmoothedValue<float> inGain, outTrim, compareMix;
    std::vector<float> sL, sR, rawL, rawR;
    double fs = 44100.0;
    float loudGainDb = 0.f, limiterLoss = 0.f, matchDb = 0.f;
    int ctrlCounter = 0;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterForgeProcessor)
};
