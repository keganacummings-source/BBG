#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "DreamApi.h"
#include "Visualizers.h"
#include "EditorPages.h"

class BabyGirlAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit BabyGirlAudioProcessorEditor(BabyGirlAudioProcessor&);
    ~BabyGirlAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void onFrame();
    void showTab(int index);

    BabyGirlAudioProcessor& audioProcessor;
    BabyGirl::BabyGirlLookAndFeel customLookAndFeel;
    BabyGirl::DreamApi dreamApi;
    BabyGirl::AnalyzerHub hub;

    BabyGirl::RackPage rackPage;
    BabyGirl::SynthPage synthPage;
    BabyGirl::DrumPage drumPage;
    BabyGirl::CorePage corePage;
    BabyGirl::ScopesPage scopesPage;
    BabyGirl::DreamPage dreamPage;
    BabyGirl::SpectrumView miniSpectrum;

    std::array<juce::TextButton, 6> tabButtons;
    juce::Label brandLabel, meterLabel, tapeDecalLabel, cpuLabel;
    juce::TextButton dreamShareButton{ "DREAMSHARE PRESETS" };
    juce::TextButton vstBuildButton{ "VST3 & GITHUB CI" };
    int currentTab = 0, frameCount = 0;
    double lastFrameMs = 0.0;

    juce::VBlankAttachment vblank;   // last: destroyed first, stops frame callbacks before anything else goes away

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BabyGirlAudioProcessorEditor)
};
