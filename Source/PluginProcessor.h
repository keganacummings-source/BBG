#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include "HushDsp.h"
#include "KyotoDsp.h"
#include "MasterDsp.h"
#include "ModulesDsp.h"
#include "DreamShare.h"
#include "Params.h"
#include "Core/bg_analysis.h"

#ifndef BG_PRODUCT_NAME
 #define BG_PRODUCT_NAME "BabyGirl"
#endif

class BabyGirlAudioProcessor : public juce::AudioProcessor
{
public:
    BabyGirlAudioProcessor();
    ~BabyGirlAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return BG_PRODUCT_NAME; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- presets (DreamShare + factory) ----
    void applyPreset(const BabyGirl::DreamPreset& preset);
    BabyGirl::DreamPreset captureCurrent(const juce::String& name, const juce::String& author) const;
    void resetAllToDefault();
    void randomizeRack(bool keepLocked, juce::Random& rng);
    void clearRack();

    // ---- UI-facing state ----
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    float currentLufs = -18.0f;       // kept from the original API (momentary LUFS)
    float currentPeak = 0.0f;         // linear true-peak estimate
    bg::ScopeRing scope;
    bg::LoudnessMeter loudness;
    bg::DrumSequencer sequencer;
    std::atomic<int> currentStep { -1 };
    std::atomic<float> slotMeter[BabyGirl::kRackSlots];
    std::atomic<int> slotMachine[BabyGirl::kRackSlots];
    std::atomic<float> cpuLoad { 0.0f };
    double getSampleRateHz() const { return sampleRateHz; }
    void loadDrumPattern(int idx) { sequencer.loadPattern(idx); }

private:
    BabyGirl::HushDsp hushEngine;
    BabyGirl::KyotoDsp kyotoEngine;
    BabyGirl::MasterDsp masterEngine;
    BabyGirl::ModulesDsp modulesEngine;
    std::unique_ptr<std::array<bg::Machine, BabyGirl::kRackSlots>> machines;
    bg::SynthEngine synth;
    bg::DrumEngine drums;

    float fet1176Env = 0.0f;
    float la2aGlow = 0.0f;
    float tb303State = 0.0f;
    float deesserSibilance = 0.0f;

    double sampleRateHz = 48000.0;
    double freePpq = 0.0, expectedNextPpq = -1.0;
    bool wasPlaying = false;
    juce::Time lastBlockTime;

    // cached raw parameter pointers (audio-thread safe reads)
    std::atomic<float>* rMach[BabyGirl::kRackSlots] {}; std::atomic<float>* rOn[BabyGirl::kRackSlots] {};
    std::atomic<float>* rMix[BabyGirl::kRackSlots] {}; std::atomic<float>* rP[BabyGirl::kRackSlots][6] {};
    std::atomic<float>* dTune[bg::kPads] {}; std::atomic<float>* dDec[bg::kPads] {}; std::atomic<float>* dLvl[bg::kPads] {}; std::atomic<float>* dPan[bg::kPads] {};
    std::atomic<float>* raw(const char* id) { return apvts.getRawParameterValue(id); }
    std::atomic<float>* rawS(const juce::String& id) { return apvts.getRawParameterValue(id); }

    bg::SynthParams readSynthParams();
    void runRack(float* l, float* r, int n, double bpm);

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BabyGirlAudioProcessor)
};
