#include "PluginEditor.h"

using namespace BabyGirl;

BabyGirlAudioProcessorEditor::BabyGirlAudioProcessorEditor(BabyGirlAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      rackPage(p), synthPage(p), drumPage(p), corePage(p), scopesPage(p, hub), dreamPage(p, dreamApi), miniSpectrum(hub, true),
      vblank(this, [this] { onFrame(); })
{
    setLookAndFeel(&customLookAndFeel);
    setResizable(true, true);
    setResizeLimits(980, 640, 1900, 1200);
    setSize(1240, 780);

    brandLabel.setText("BABYGIRL.VST", juce::dontSendNotification);
    brandLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    brandLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b));
    addAndMakeVisible(brandLabel);

    tapeDecalLabel.setText("CAUTION: HIGH FLUX BIAS", juce::dontSendNotification);
    tapeDecalLabel.setFont(juce::Font(9.0f, juce::Font::bold));
    tapeDecalLabel.setColour(juce::Label::textColourId, juce::Colour(0xff18181b));
    tapeDecalLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xfffacc15));
    tapeDecalLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(tapeDecalLabel);

    meterLabel.setFont(juce::Font(11.5f));
    meterLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));
    addAndMakeVisible(meterLabel);
    cpuLabel.setFont(juce::Font(10.5f));
    cpuLabel.setColour(juce::Label::textColourId, juce::Colour(0xff71717a));
    addAndMakeVisible(cpuLabel);

    static const char* names[6] = { "RACK", "SYNTH", "DRUMS", "CORE", "SCOPES", "DREAM" };
    for (int i = 0; i < 6; ++i)
    {
        auto& b = tabButtons[(size_t)i];
        b.setButtonText(names[i]);
        b.setRadioGroupId(4242);
        b.setClickingTogglesState(true);
        b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1c1d24));
        b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe4e4e7));
        b.onClick = [this, i] { showTab(i); };
        addAndMakeVisible(b);
    }

    for (auto* c : std::initializer_list<juce::Component*>{ &rackPage, &synthPage, &drumPage, &corePage, &scopesPage, &dreamPage })
        addChildComponent(c);
    addAndMakeVisible(miniSpectrum);

    dreamShareButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    dreamShareButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff59e0b));
    dreamShareButton.onClick = [this] { tabButtons[5].setToggleState(true, juce::dontSendNotification); showTab(5); };
    addAndMakeVisible(dreamShareButton);

    vstBuildButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f2937));
    vstBuildButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
    vstBuildButton.onClick = [] { juce::URL("https://github.com/keganacummings-source/BBGVST").launchInDefaultBrowser(); };
    addAndMakeVisible(vstBuildButton);

    tabButtons[0].setToggleState(true, juce::dontSendNotification);
    showTab(0);
}

BabyGirlAudioProcessorEditor::~BabyGirlAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void BabyGirlAudioProcessorEditor::showTab(int index)
{
    currentTab = index;
    juce::Component* pages[6] = { &rackPage, &synthPage, &drumPage, &corePage, &scopesPage, &dreamPage };
    for (int i = 0; i < 6; ++i) pages[i]->setVisible(i == index);
}

void BabyGirlAudioProcessorEditor::onFrame()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = lastFrameMs > 0.0 ? (float)juce::jlimit(0.001, 0.1, (now - lastFrameMs) * 0.001) : 0.016f;
    lastFrameMs = now;
    ++frameCount;

    hub.update(audioProcessor.scope, audioProcessor.getSampleRateHz(), dt);
    miniSpectrum.repaint();

    switch (currentTab)
    {
        case 0: if ((frameCount % 4) == 0) rackPage.refresh(); break;
        case 2: drumPage.tick(); break;
        case 4: scopesPage.tick(); break;
        default: break;
    }

    if ((frameCount % 6) == 0)
    {
        auto fmt = [](float v) { return v <= -99.0f ? juce::String("-inf") : juce::String(v, 1); };
        meterLabel.setText("LUFS-M " + fmt(audioProcessor.loudness.momentary.load()) + "   TP " + fmt(audioProcessor.loudness.truePeakDb.load()) + " dBTP", juce::dontSendNotification);
        cpuLabel.setText("DSP " + juce::String((int)(audioProcessor.cpuLoad.load() * 100.0f)) + "%   |   " + juce::String(bg::kNumMachines) + " machines", juce::dontSendNotification);
    }
}

void BabyGirlAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0b0c10));
    g.setGradientFill(juce::ColourGradient::vertical(juce::Colour(0xff1f2027), 0.0f, juce::Colour(0xff14151a), 58.0f));
    g.fillRect(0, 0, getWidth(), 58);
    g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.55f));
    g.drawHorizontalLine(58, 0.0f, (float)getWidth());
}

void BabyGirlAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop(58).reduced(10, 8);
    brandLabel.setBounds(head.removeFromLeft(170));
    tapeDecalLabel.setBounds(head.removeFromLeft(150).withSizeKeepingCentre(150, 16));
    head.removeFromLeft(10);
    for (auto& b : tabButtons) { b.setBounds(head.removeFromLeft(78).reduced(2, 4)); }
    auto right = head.removeFromRight(240);
    vstBuildButton.setBounds(right.removeFromRight(100).reduced(0, 6)); right.removeFromRight(4);
    dreamShareButton.setBounds(right.removeFromRight(130).reduced(0, 6));
    auto info = head.removeFromRight(juce::jmin(250, head.getWidth() / 2));
    meterLabel.setBounds(info.removeFromTop(22)); cpuLabel.setBounds(info);
    head.removeFromRight(8);
    miniSpectrum.setBounds(head.reduced(0, 2));

    for (auto* c : std::initializer_list<juce::Component*>{ &rackPage, &synthPage, &drumPage, &corePage, &scopesPage, &dreamPage })
        c->setBounds(r);
}
