#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Visualizers.h"
#include "DreamApi.h"
#include "Presets.h"

namespace BabyGirl
{
    using APVTS = juce::AudioProcessorValueTreeState;

    // ----------------------------------------------------------------- shared widgets
    class Knob : public juce::Component
    {
    public:
        Knob(APVTS& apvts, const juce::String& paramId, const juce::String& text, juce::Colour colour = Theme::amber, bool showValue = true)
        {
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(showValue ? juce::Slider::TextBoxBelow : juce::Slider::NoTextBox, false, 58, 13);
            slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
            slider.setNumDecimalPlacesToDisplay(2);
            label.setText(text, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setFont(juce::Font(10.0f, juce::Font::bold));
            label.setColour(juce::Label::textColourId, Theme::dim);
            label.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(slider); addAndMakeVisible(label);
            attachment = std::make_unique<APVTS::SliderAttachment>(apvts, paramId, slider);
        }
        void setText(const juce::String& t) { label.setText(t, juce::dontSendNotification); }
        void resized() override { auto r = getLocalBounds(); label.setBounds(r.removeFromTop(13)); slider.setBounds(r); }
        juce::Slider slider; juce::Label label;
    private:
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    inline void layoutGrid(juce::Rectangle<int> area, const std::vector<juce::Component*>& items, int cols, int maxCell = 96)
    {
        if (items.empty() || cols <= 0) return;
        const int rows = ((int)items.size() + cols - 1) / cols;
        const int cw = juce::jmin(maxCell, area.getWidth() / cols), ch = juce::jmin((int)(maxCell * 1.15f), area.getHeight() / rows);
        const int totalW = cw * cols, x0 = area.getX() + (area.getWidth() - totalW) / 2;
        for (size_t i = 0; i < items.size(); ++i)
            items[i]->setBounds(x0 + ((int)i % cols) * cw, area.getY() + ((int)i / cols) * ch, cw, ch);
    }

    inline void setParamReal(APVTS& a, const juce::String& id, float real)
    {
        if (auto* p = a.getParameter(id)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(real)); p->endChangeGesture(); }
    }

    inline void setSlotMachine(BabyGirlAudioProcessor& proc, int slot, int machine)
    {
        const auto& info = bg::kMachines[machine];
        setParamReal(proc.apvts, rackId(slot, "MACH"), (float)machine);
        setParamReal(proc.apvts, rackId(slot, "ON"), 1.0f);
        setParamReal(proc.apvts, rackId(slot, "MIX"), defaultMixForFamily(info.fam));
        for (int k = 0; k < 6; ++k) setParamReal(proc.apvts, rackId(slot, "P" + juce::String(k + 1)), info.def[k]);
    }

    // ----------------------------------------------------------------- rack
    class SlotCard : public juce::Component
    {
    public:
        SlotCard(BabyGirlAudioProcessor& p, int s) : proc(p), slot(s), mix(p.apvts, rackId(s, "MIX"), "MIX", Theme::cyan)
        {
            machineBtn.setColour(juce::TextButton::buttonColourId, Theme::panel2);
            machineBtn.setColour(juce::TextButton::textColourOffId, Theme::text);
            machineBtn.onClick = [this] { showMenu(); };
            addAndMakeVisible(machineBtn);
            onBtn.setButtonText("ON"); addAndMakeVisible(onBtn);
            onAtt = std::make_unique<APVTS::ButtonAttachment>(p.apvts, rackId(s, "ON"), onBtn);
            for (int k = 0; k < 6; ++k) { knobs[(size_t)k] = std::make_unique<Knob>(p.apvts, rackId(s, "P" + juce::String(k + 1)), "-"); addAndMakeVisible(*knobs[(size_t)k]); }
            addAndMakeVisible(mix);
            resetBtn.setButtonText("RESET"); resetBtn.setColour(juce::TextButton::buttonColourId, Theme::panel2); resetBtn.setColour(juce::TextButton::textColourOffId, Theme::dim);
            diceBtn.setButtonText("DICE"); diceBtn.setColour(juce::TextButton::buttonColourId, Theme::panel2); diceBtn.setColour(juce::TextButton::textColourOffId, Theme::pink);
            resetBtn.onClick = [this] { const int m = current(); if (m > 0) setSlotMachine(proc, slot, m); };
            diceBtn.onClick = [this]
            {
                if (current() == 0) return;
                for (int k = 0; k < 6; ++k) setParamReal(proc.apvts, rackId(slot, "P" + juce::String(k + 1)), rng.nextFloat());
            };
            addAndMakeVisible(resetBtn); addAndMakeVisible(diceBtn);
            refresh(true);
        }

        int current() const { return (int)proc.apvts.getRawParameterValue(rackId(slot, "MACH"))->load(); }

        void refresh(bool force = false)
        {
            const int m = juce::jlimit(0, bg::kNumMachines - 1, current());
            if (m != shown || force)
            {
                shown = m; const auto& info = bg::kMachines[m];
                machineBtn.setButtonText(juce::String(slot + 1) + "  " + info.name);
                for (int k = 0; k < 6; ++k) { knobs[(size_t)k]->setText(info.lab[k]); knobs[(size_t)k]->setVisible(m != 0 && juce::String(info.lab[k]) != "-"); }
                mix.setVisible(m != 0); onBtn.setVisible(m != 0); resetBtn.setVisible(m != 0); diceBtn.setVisible(m != 0);
                repaint();
            }
            const float mt = proc.slotMeter[slot].load();
            if (std::abs(mt - lastMeter) > 0.002f || shown != 0) { lastMeter = mt; repaint(getLocalBounds().removeFromBottom(10)); }
        }

        void paint(juce::Graphics& g) override
        {
            const auto& info = bg::kMachines[juce::jlimit(0, bg::kNumMachines - 1, shown)];
            const auto col = Theme::family(info.fam);
            auto r = getLocalBounds().toFloat().reduced(2.0f);
            g.setGradientFill(juce::ColourGradient::vertical(Theme::panel2, r.getY(), Theme::panel, r.getBottom()));
            g.fillRoundedRectangle(r, 6.0f);
            g.setColour(shown == 0 ? Theme::edge : col.withAlpha(0.65f)); g.drawRoundedRectangle(r, 6.0f, shown == 0 ? 1.0f : 1.5f);
            if (shown != 0)
            {
                g.setColour(col.withAlpha(0.85f)); g.fillRoundedRectangle(r.getX(), r.getY() + 6.0f, 3.0f, 18.0f, 1.5f);
                g.setColour(Theme::dim); g.setFont(juce::Font(9.5f)); g.drawText(juce::String(info.maker) + "  |  " + bg::kFamNames[info.fam], (int)r.getX() + 8, (int)r.getBottom() - 21, (int)r.getWidth() - 16, 11, juce::Justification::left);
                // activity / gain-reduction meter along the bottom edge
                const float mt = proc.slotMeter[slot].load();
                float t = info.fam == bg::F_DYN ? mt / 18.0f : mt;
                if (info.fam == bg::F_FORGE)
                {
                    const int seed = (int)(proc.apvts.getRawParameterValue(rackId(slot, "P1"))->load() * (float)bg::kForgeSeeds);
                    g.setColour(col); g.setFont(juce::Font(10.0f, juce::Font::bold)); g.drawText("SEED #" + juce::String(juce::jmin(seed, bg::kForgeSeeds - 1)), (int)r.getRight() - 92, (int)r.getBottom() - 21, 86, 11, juce::Justification::right);
                }
                else
                {
                    t = juce::jlimit(0.0f, 1.0f, t);
                    g.setColour(Theme::back); g.fillRoundedRectangle(r.getX() + 6.0f, r.getBottom() - 8.0f, r.getWidth() - 12.0f, 4.0f, 2.0f);
                    g.setColour(info.fam == bg::F_DYN ? Theme::pink : col); g.fillRoundedRectangle(r.getX() + 6.0f, r.getBottom() - 8.0f, (r.getWidth() - 12.0f) * t, 4.0f, 2.0f);
                }
            }
            else
            {
                g.setColour(Theme::dim.withAlpha(0.6f)); g.setFont(juce::Font(12.0f));
                g.drawText("click to load a machine  -  " + juce::String(bg::kNumMachines) + " available", getLocalBounds().withTrimmedTop(40), juce::Justification::centred);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced(6);
            auto head = r.removeFromTop(26);
            onBtn.setBounds(head.removeFromRight(48)); head.removeFromRight(4);
            machineBtn.setBounds(head);
            r.removeFromTop(4); r.removeFromBottom(22);
            const int cols = 4, rows = 2, cw = r.getWidth() / cols, ch = r.getHeight() / rows;
            std::vector<juce::Component*> cells;
            for (int k = 0; k < 6; ++k) cells.push_back(knobs[(size_t)k].get());
            cells.push_back(&mix);
            for (size_t i = 0; i < cells.size(); ++i) cells[i]->setBounds(r.getX() + ((int)i % cols) * cw, r.getY() + ((int)i / cols) * ch, cw, ch);
            auto last = juce::Rectangle<int>(r.getX() + 3 * cw, r.getY() + ch, cw, ch).reduced(6, ch / 4);
            resetBtn.setBounds(last.removeFromTop(last.getHeight() / 2 - 2)); last.removeFromTop(4); diceBtn.setBounds(last);
        }

    private:
        void showMenu()
        {
            juce::PopupMenu root;
            const int cur = current();
            for (int f = 0; f < bg::F_COUNT; ++f)
            {
                juce::PopupMenu sub;
                for (int i = 0; i < bg::kNumMachines; ++i)
                    if (bg::kMachines[i].fam == f) sub.addItem(i + 1, juce::String(bg::kMachines[i].name), true, i == cur);
                root.addSubMenu(juce::String(bg::kFamNames[f]) + "  (" + juce::String([&] { int c = 0; for (auto& m : bg::kMachines) c += m.fam == f; return c; }()) + ")", sub);
            }
            juce::Component::SafePointer<SlotCard> safe(this);
            root.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&machineBtn).withMinimumWidth(260), [safe](int result)
            {
                if (safe != nullptr && result > 0) setSlotMachine(safe->proc, safe->slot, result - 1);
            });
        }

        BabyGirlAudioProcessor& proc; int slot; int shown = -1; float lastMeter = -1.0f;
        juce::TextButton machineBtn, resetBtn, diceBtn; juce::ToggleButton onBtn;
        std::unique_ptr<APVTS::ButtonAttachment> onAtt;
        std::array<std::unique_ptr<Knob>, 6> knobs; Knob mix; juce::Random rng;
    };

    class RackPage : public juce::Component
    {
    public:
        explicit RackPage(BabyGirlAudioProcessor& p) : proc(p)
        {
            for (int s = 0; s < kRackSlots; ++s) { cards[(size_t)s] = std::make_unique<SlotCard>(p, s); addChildComponent(*cards[(size_t)s]); }
            auto styleBtn = [&](juce::TextButton& b, const juce::String& t, juce::Colour c) { b.setButtonText(t); b.setColour(juce::TextButton::buttonColourId, Theme::panel2); b.setColour(juce::TextButton::textColourOffId, c); addAndMakeVisible(b); };
            styleBtn(pageA, "SLOTS 1-8", Theme::amber); styleBtn(pageB, "SLOTS 9-16", Theme::amber);
            pageA.setClickingTogglesState(false); pageB.setClickingTogglesState(false);
            pageA.onClick = [this] { setPage(0); }; pageB.onClick = [this] { setPage(1); };
            styleBtn(randomBtn, "RANDOMIZE RACK", Theme::pink); styleBtn(clearBtn, "CLEAR RACK", Theme::dim);
            randomBtn.onClick = [this] { proc.randomizeRack(false, rnd); };
            clearBtn.onClick = [this] { proc.clearRack(); };
            posBox.addItem("Rack BEFORE Hush", 1); posBox.addItem("Rack AFTER Kyoto", 2); addAndMakeVisible(posBox);
            posAtt = std::make_unique<APVTS::ComboBoxAttachment>(p.apvts, "RACK_POS", posBox);
            info.setText(juce::String(bg::kNumMachines) + " MACHINES  -  16 SERIAL SLOTS  -  FORGE: 10,000 SEEDED MACHINES", juce::dontSendNotification);
            info.setFont(juce::Font(11.0f, juce::Font::bold)); info.setColour(juce::Label::textColourId, Theme::dim); addAndMakeVisible(info);
            setPage(0);
        }
        void setPage(int pg)
        {
            page = pg;
            for (int s = 0; s < kRackSlots; ++s) cards[(size_t)s]->setVisible((s / 8) == pg);
            pageA.setToggleState(pg == 0, juce::dontSendNotification); pageB.setToggleState(pg == 1, juce::dontSendNotification);
            resized();
        }
        void refresh() { for (int s = page * 8; s < page * 8 + 8; ++s) cards[(size_t)s]->refresh(); }
        void paint(juce::Graphics& g) override { g.fillAll(Theme::back); }
        void resized() override
        {
            auto r = getLocalBounds().reduced(8);
            auto head = r.removeFromTop(30);
            pageA.setBounds(head.removeFromLeft(96)); head.removeFromLeft(4); pageB.setBounds(head.removeFromLeft(96)); head.removeFromLeft(12);
            randomBtn.setBounds(head.removeFromLeft(130)); head.removeFromLeft(4); clearBtn.setBounds(head.removeFromLeft(100)); head.removeFromLeft(12);
            posBox.setBounds(head.removeFromLeft(150)); head.removeFromLeft(12); info.setBounds(head);
            r.removeFromTop(6);
            const int cw = r.getWidth() / 4, ch = r.getHeight() / 2;
            for (int i = 0; i < 8; ++i) cards[(size_t)(page * 8 + i)]->setBounds(r.getX() + (i % 4) * cw, r.getY() + (i / 4) * ch, cw, ch);
        }
    private:
        BabyGirlAudioProcessor& proc; int page = 0; juce::Random rnd;
        std::array<std::unique_ptr<SlotCard>, kRackSlots> cards;
        juce::TextButton pageA, pageB, randomBtn, clearBtn; juce::ComboBox posBox; juce::Label info;
        std::unique_ptr<APVTS::ComboBoxAttachment> posAtt;
    };

    // ----------------------------------------------------------------- synth
    class SynthPage : public juce::Component
    {
    public:
        explicit SynthPage(BabyGirlAudioProcessor& p)
            : proc(p), keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
        {
            auto K = [&](const char* id, const char* text, std::vector<juce::Component*>& grp, juce::Colour c = Theme::amber)
            {
                knobs.push_back(std::make_unique<Knob>(p.apvts, id, text, c)); addAndMakeVisible(*knobs.back()); grp.push_back(knobs.back().get());
            };
            auto C = [&](juce::ComboBox& box, std::unique_ptr<APVTS::ComboBoxAttachment>& att, const char* id, const juce::StringArray& items)
            {
                box.addItemList(items, 1); addAndMakeVisible(box); att = std::make_unique<APVTS::ComboBoxAttachment>(p.apvts, id, box);
            };
            juce::StringArray osc, flt, lfo;
            for (auto* o : bg::kOscNames) osc.add(o);
            for (auto* o : bg::kFilterNames) flt.add(o);
            for (auto* o : bg::kLfoNames) lfo.add(o);
            C(osc1, a1, "SYN_OSC1", osc); C(osc2, a2, "SYN_OSC2", osc); C(fltBox, a3, "SYN_FTYPE", flt); C(lfoBox, a4, "SYN_LFOSHAPE", lfo);
            K("SYN_O2SEMI", "O2 SEMI", gOsc); K("SYN_O2FINE", "O2 FINE", gOsc); K("SYN_MIX", "OSC MIX", gOsc); K("SYN_SUB", "SUB", gOsc); K("SYN_NOISE", "NOISE", gOsc);
            K("SYN_UNI", "UNISON", gOsc); K("SYN_UDET", "UNI DETUNE", gOsc); K("SYN_MORPH", "MORPH", gOsc); K("SYN_FM", "FM / DRAWBAR", gOsc); K("SYN_PWM", "PULSE W", gOsc);
            K("SYN_CUT", "CUTOFF", gFlt, Theme::cyan); K("SYN_RES", "RESO", gFlt, Theme::cyan); K("SYN_DRIVE", "DRIVE", gFlt, Theme::cyan); K("SYN_FENV", "ENV AMT", gFlt, Theme::cyan); K("SYN_KEY", "KEY TRK", gFlt, Theme::cyan);
            K("SYN_A", "A", gAmp, Theme::green); K("SYN_D", "D", gAmp, Theme::green); K("SYN_S", "S", gAmp, Theme::green); K("SYN_R", "R", gAmp, Theme::green);
            K("SYN_FA", "A", gFe, Theme::violet); K("SYN_FD", "D", gFe, Theme::violet); K("SYN_FS", "S", gFe, Theme::violet); K("SYN_FR", "R", gFe, Theme::violet);
            K("SYN_LFORATE", "RATE", gLfo, Theme::pink); K("SYN_LFOPITCH", "> PITCH", gLfo, Theme::pink); K("SYN_LFOFILT", "> FILTER", gLfo, Theme::pink); K("SYN_LFOAMP", "> AMP", gLfo, Theme::pink);
            K("SYN_GLIDE", "GLIDE", gGlob); K("SYN_VEL", "VEL SENS", gGlob); K("SYN_LEVEL", "LEVEL", gGlob);
            onBtn.setButtonText("SYNTH ON"); addAndMakeVisible(onBtn); onAtt = std::make_unique<APVTS::ButtonAttachment>(p.apvts, "SYN_ON", onBtn);
            keyboard.setAvailableRange(24, 96); keyboard.setKeyWidth(24.0f); keyboard.setMidiChannel(1); addAndMakeVisible(keyboard);
            hint.setText("Oscillators: Sine, Triangle, Saw, Square/PWM, SuperSaw, Wavetable, FM, Organ, Pluck (Osc 1), Hard-Sync.  Use MORPH / FM as the model's extra control.", juce::dontSendNotification);
            hint.setFont(juce::Font(10.5f)); hint.setColour(juce::Label::textColourId, Theme::dim); addAndMakeVisible(hint);
        }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(Theme::back);
            auto box = [&](juce::Rectangle<int> r, const juce::String& t, juce::Colour c)
            {
                g.setColour(Theme::panel); g.fillRoundedRectangle(r.toFloat(), 6.0f); g.setColour(Theme::edge); g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 6.0f, 1.0f);
                g.setColour(c); g.setFont(juce::Font(11.0f, juce::Font::bold)); g.drawText(t, r.getX() + 10, r.getY() + 5, r.getWidth() - 20, 14, juce::Justification::left);
            };
            box(rOsc, "OSCILLATORS", Theme::amber); box(rFlt, "FILTER", Theme::cyan); box(rAmp, "AMP ENVELOPE", Theme::green);
            box(rFe, "FILTER ENVELOPE", Theme::violet); box(rLfo, "LFO", Theme::pink); box(rGlob, "GLOBAL", Theme::amber);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced(8);
            auto kb = r.removeFromBottom(92); keyboard.setBounds(kb); hint.setBounds(r.removeFromBottom(18));
            r.removeFromBottom(4);
            const int topH = r.getHeight() / 2 - 3;
            auto top = r.removeFromTop(topH); r.removeFromTop(6); auto bot = r;
            rOsc = top.removeFromLeft(top.getWidth() * 58 / 100); top.removeFromLeft(6); rFlt = top;
            rAmp = bot.removeFromLeft(bot.getWidth() * 24 / 100); bot.removeFromLeft(6); rFe = bot.removeFromLeft(bot.getWidth() * 32 / 100); bot.removeFromLeft(6);
            rLfo = bot.removeFromLeft(bot.getWidth() * 56 / 100); bot.removeFromLeft(6); rGlob = bot;
            auto in = [](juce::Rectangle<int> a) { return a.withTrimmedTop(24).reduced(6, 2); };
            auto oa = rOsc.withTrimmedTop(22).reduced(10, 0); osc1.setBounds(oa.removeFromTop(24).removeFromLeft(oa.getWidth() / 2 - 4)); 
            osc2.setBounds(rOsc.getX() + 10 + (rOsc.getWidth() - 20) / 2 + 4, rOsc.getY() + 22, (rOsc.getWidth() - 20) / 2 - 4, 24);
            layoutGrid(in(rOsc).withTrimmedTop(26), gOsc, 5, 100);
            fltBox.setBounds(rFlt.getX() + 10, rFlt.getY() + 22, rFlt.getWidth() - 20, 24); layoutGrid(in(rFlt).withTrimmedTop(26), gFlt, 3, 100);
            layoutGrid(in(rAmp), gAmp, 4, 100); layoutGrid(in(rFe), gFe, 4, 100);
            lfoBox.setBounds(rLfo.getX() + 10, rLfo.getY() + 22, rLfo.getWidth() - 20, 24); layoutGrid(in(rLfo).withTrimmedTop(26), gLfo, 4, 100);
            layoutGrid(in(rGlob).withTrimmedTop(0), gGlob, 3, 100);
            onBtn.setBounds(rGlob.getX() + 10, rGlob.getBottom() - 26, rGlob.getWidth() - 20, 22);
        }
    private:
        BabyGirlAudioProcessor& proc;
        std::vector<std::unique_ptr<Knob>> knobs; std::vector<juce::Component*> gOsc, gFlt, gAmp, gFe, gLfo, gGlob;
        juce::ComboBox osc1, osc2, fltBox, lfoBox; juce::ToggleButton onBtn; juce::Label hint;
        std::unique_ptr<APVTS::ComboBoxAttachment> a1, a2, a3, a4; std::unique_ptr<APVTS::ButtonAttachment> onAtt;
        juce::MidiKeyboardComponent keyboard;
        juce::Rectangle<int> rOsc, rFlt, rAmp, rFe, rLfo, rGlob;
    };

    // ----------------------------------------------------------------- drums
    class PadButton : public juce::Component
    {
    public:
        PadButton(BabyGirlAudioProcessor& p, int i, std::function<void(int)> sel) : proc(p), pad(i), onSelect(std::move(sel)) {}
        void mouseDown(const juce::MouseEvent&) override { down = true; proc.keyboardState.noteOn(10, bg::kDrumBaseNote + pad, 0.9f); onSelect(pad); repaint(); }
        void mouseUp(const juce::MouseEvent&) override { down = false; proc.keyboardState.noteOff(10, bg::kDrumBaseNote + pad, 0.0f); repaint(); }
        void setSelected(bool s) { if (s != selected) { selected = s; repaint(); } }
        static juce::Colour colourFor(int pad) { return pad < 3 ? Theme::amber : pad < 7 ? Theme::cyan : pad < 10 ? Theme::green : pad < 13 ? Theme::violet : Theme::pink; }
        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced(3.0f); const auto c = colourFor(pad);
            g.setGradientFill(juce::ColourGradient::vertical(down ? c.withAlpha(0.8f) : Theme::panel2.brighter(0.05f), r.getY(), down ? c.withAlpha(0.45f) : Theme::panel, r.getBottom()));
            g.fillRoundedRectangle(r, 8.0f);
            g.setColour(selected ? c : Theme::edge); g.drawRoundedRectangle(r, 8.0f, selected ? 2.0f : 1.0f);
            g.setColour(down ? juce::Colours::black : Theme::text); g.setFont(juce::Font(12.0f, juce::Font::bold)); g.drawText(bg::kPadNames[pad], r.toNearestInt(), juce::Justification::centred);
            g.setColour(down ? juce::Colours::black : Theme::dim); g.setFont(juce::Font(9.0f)); g.drawText("note " + juce::String(bg::kDrumBaseNote + pad), r.toNearestInt().removeFromBottom(16), juce::Justification::centred);
        }
    private:
        BabyGirlAudioProcessor& proc; int pad; bool down = false, selected = false; std::function<void(int)> onSelect;
    };

    class StepGrid : public juce::Component
    {
    public:
        explicit StepGrid(BabyGirlAudioProcessor& p) : proc(p) {}
        int selectedPad = 0; std::function<void(int)> onPadClicked;
        void paint(juce::Graphics& g) override
        {
            g.fillAll(Theme::panel);
            const int lw = 92; const float cw = (float)(getWidth() - lw) / 16.0f, rh = (float)getHeight() / 16.0f;
            const int cur = proc.currentStep.load();
            for (int row = 0; row < 16; ++row)
            {
                const auto c = PadButton::colourFor(row);
                const float y = rh * (float)row;
                g.setColour(row == selectedPad ? c.withAlpha(0.14f) : juce::Colours::transparentBlack); g.fillRect(0.0f, y, (float)getWidth(), rh);
                g.setColour(row == selectedPad ? c : Theme::dim); g.setFont(juce::Font(10.0f, juce::Font::bold)); g.drawText(bg::kPadNames[row], 6, (int)y, lw - 8, (int)rh, juce::Justification::centredLeft);
                for (int col = 0; col < 16; ++col)
                {
                    const auto cell = juce::Rectangle<float>(lw + cw * (float)col + 1.5f, y + 1.5f, cw - 3.0f, rh - 3.0f);
                    const int v = proc.sequencer.grid[row][col];
                    g.setColour(((col / 4) & 1) ? Theme::panel2.brighter(0.04f) : Theme::panel2); g.fillRoundedRectangle(cell, 3.0f);
                    if (v > 0) { g.setColour(c.withAlpha(0.35f + 0.65f * (float)v / 127.0f)); g.fillRoundedRectangle(cell, 3.0f); if (v > 110) { g.setColour(juce::Colours::white.withAlpha(0.7f)); g.drawRoundedRectangle(cell, 3.0f, 1.2f); } }
                }
            }
            if (cur >= 0) { g.setColour(juce::Colours::white.withAlpha(0.14f)); g.fillRect((float)lw + cw * (float)cur, 0.0f, cw, (float)getHeight()); }
            g.setColour(Theme::edge); g.drawRect(getLocalBounds(), 1);
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            int row, col; if (!cellAt(e.getPosition(), row, col)) { if (e.x < 92) { selectedPad = juce::jlimit(0, 15, (int)(e.y / ((float)getHeight() / 16.0f))); if (onPadClicked) onPadClicked(selectedPad); repaint(); } return; }
            const int accent = (e.mods.isRightButtonDown() || e.mods.isCtrlDown()) ? 127 : 100;
            paintValue = proc.sequencer.grid[row][col] ? 0 : accent; apply(row, col);
            selectedPad = row; if (onPadClicked) onPadClicked(row);
        }
        void mouseDrag(const juce::MouseEvent& e) override { int row, col; if (cellAt(e.getPosition(), row, col)) apply(row, col); }
    private:
        bool cellAt(juce::Point<int> p, int& row, int& col) const
        {
            const int lw = 92; if (p.x < lw || p.x >= getWidth() || p.y < 0 || p.y >= getHeight()) return false;
            col = juce::jlimit(0, 15, (int)((float)(p.x - lw) / ((float)(getWidth() - lw) / 16.0f))); row = juce::jlimit(0, 15, (int)((float)p.y / ((float)getHeight() / 16.0f))); return true;
        }
        void apply(int row, int col) { proc.sequencer.grid[row][col] = (uint8_t)paintValue; repaint(); }
        BabyGirlAudioProcessor& proc; int paintValue = 100;
    };

    class DrumPage : public juce::Component
    {
    public:
        explicit DrumPage(BabyGirlAudioProcessor& p) : proc(p), grid(p)
        {
            for (int i = 0; i < 16; ++i) { pads[(size_t)i] = std::make_unique<PadButton>(p, i, [this](int pd) { selectPad(pd); }); addAndMakeVisible(*pads[(size_t)i]); }
            addAndMakeVisible(grid); grid.onPadClicked = [this](int pd) { selectPad(pd); };
            for (int i = 0; i < bg::DrumSequencer::kPatterns; ++i) patternBox.addItem(bg::DrumSequencer::patternName(i), i + 1);
            patternBox.setTextWhenNothingSelected("Load pattern..."); addAndMakeVisible(patternBox);
            patternBox.onChange = [this] { const int i = patternBox.getSelectedItemIndex(); if (i >= 0) { proc.loadDrumPattern(i); grid.repaint(); } };
            clearBtn.setButtonText("CLEAR GRID"); clearBtn.setColour(juce::TextButton::buttonColourId, Theme::panel2); clearBtn.setColour(juce::TextButton::textColourOffId, Theme::dim);
            clearBtn.onClick = [this] { proc.sequencer.clear(); grid.repaint(); }; addAndMakeVisible(clearBtn);
            seqBox.addItemList({ "Sequencer: OFF", "Sequencer: HOST SYNC", "Sequencer: FREE RUN" }, 1); addAndMakeVisible(seqBox);
            mapBox.addItemList({ "MIDI: Channel 10 only", "MIDI: Notes 36-51 (all ch)", "MIDI: Both" }, 1); addAndMakeVisible(mapBox);
            seqAtt = std::make_unique<APVTS::ComboBoxAttachment>(p.apvts, "DRM_SEQ", seqBox); mapAtt = std::make_unique<APVTS::ComboBoxAttachment>(p.apvts, "DRM_MAP", mapBox);
            swing = std::make_unique<Knob>(p.apvts, "DRM_SWING", "SWING", Theme::cyan); bpm = std::make_unique<Knob>(p.apvts, "DRM_BPM", "FREE BPM", Theme::cyan); master = std::make_unique<Knob>(p.apvts, "DRM_LEVEL", "DRUM LEVEL");
            onBtn.setButtonText("DRUMS ON"); onAtt = std::make_unique<APVTS::ButtonAttachment>(p.apvts, "DRM_ON", onBtn);
            for (auto* c : std::initializer_list<juce::Component*>{ swing.get(), bpm.get(), master.get(), &onBtn }) addAndMakeVisible(c);
            hint.setText("Click / drag cells to program steps  |  right-click or Ctrl = accent  |  click a row name or pad to edit its tune / decay / level / pan", juce::dontSendNotification);
            hint.setFont(juce::Font(10.5f)); hint.setColour(juce::Label::textColourId, Theme::dim); addAndMakeVisible(hint);
            selectPad(0);
        }
        void selectPad(int pd)
        {
            sel = pd; grid.selectedPad = pd; for (int i = 0; i < 16; ++i) pads[(size_t)i]->setSelected(i == pd);
            const auto c = PadButton::colourFor(pd);
            tune = std::make_unique<Knob>(proc.apvts, padId(pd, "TUNE"), "TUNE", c); dec = std::make_unique<Knob>(proc.apvts, padId(pd, "DEC"), "DECAY", c);
            lvl = std::make_unique<Knob>(proc.apvts, padId(pd, "LVL"), "LEVEL", c); pan = std::make_unique<Knob>(proc.apvts, padId(pd, "PAN"), "PAN", c);
            for (auto* k : { tune.get(), dec.get(), lvl.get(), pan.get() }) addAndMakeVisible(k);
            resized(); repaint();
        }
        void tick() { if (isShowing()) grid.repaint(); }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(Theme::back);
            g.setColour(Theme::amber); g.setFont(juce::Font(12.0f, juce::Font::bold)); g.drawText(juce::String("PAD: ") + bg::kPadNames[sel], knobArea.getX(), knobArea.getY() - 18, 200, 14, juce::Justification::left);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced(8);
            hint.setBounds(r.removeFromBottom(16));
            auto left = r.removeFromLeft(juce::jmin(420, r.getWidth() * 36 / 100)); r.removeFromLeft(8);
            auto padArea = left.removeFromTop(left.getHeight() * 52 / 100);
            const int pw = padArea.getWidth() / 4, ph = padArea.getHeight() / 4;
            for (int i = 0; i < 16; ++i) pads[(size_t)i]->setBounds(padArea.getX() + (i % 4) * pw, padArea.getY() + (3 - i / 4) * ph, pw, ph);
            left.removeFromTop(24); knobArea = left.removeFromTop(juce::jmin(120, left.getHeight() / 2));
            layoutGrid(knobArea, { tune.get(), dec.get(), lvl.get(), pan.get() }, 4, 96);
            left.removeFromTop(8);
            seqBox.setBounds(left.removeFromTop(24)); left.removeFromTop(4); mapBox.setBounds(left.removeFromTop(24)); left.removeFromTop(4);
            auto row = left.removeFromTop(24); patternBox.setBounds(row.removeFromLeft(row.getWidth() * 62 / 100)); row.removeFromLeft(4); clearBtn.setBounds(row); left.removeFromTop(4);
            onBtn.setBounds(left.removeFromTop(22)); left.removeFromTop(2);
            layoutGrid(left, { swing.get(), bpm.get(), master.get() }, 3, 96);
            grid.setBounds(r);
        }
    private:
        BabyGirlAudioProcessor& proc; StepGrid grid; int sel = 0;
        std::array<std::unique_ptr<PadButton>, 16> pads;
        std::unique_ptr<Knob> tune, dec, lvl, pan, swing, bpm, master;
        juce::ComboBox patternBox, seqBox, mapBox; juce::TextButton clearBtn; juce::ToggleButton onBtn; juce::Label hint; juce::Rectangle<int> knobArea;
        std::unique_ptr<APVTS::ComboBoxAttachment> seqAtt, mapAtt; std::unique_ptr<APVTS::ButtonAttachment> onAtt;
    };

    // ----------------------------------------------------------------- core (original three bays, preserved)
    class CorePage : public juce::Component
    {
    public:
        explicit CorePage(BabyGirlAudioProcessor& p)
        {
            auto K = [&](const char* id, const char* t, std::vector<juce::Component*>& g, juce::Colour c)
            { knobs.push_back(std::make_unique<Knob>(p.apvts, id, t, c)); addAndMakeVisible(*knobs.back()); g.push_back(knobs.back().get()); };
            auto T = [&](juce::ToggleButton& b, std::unique_ptr<APVTS::ButtonAttachment>& a, const char* id, const char* t) { b.setButtonText(t); addAndMakeVisible(b); a = std::make_unique<APVTS::ButtonAttachment>(p.apvts, id, b); };
            K("TUBE_DRIVE", "DRIVE", gH, Theme::amber); K("TUBE_BIAS", "BIAS", gH, Theme::amber); K("FILTER_CUTOFF", "CUTOFF", gH, Theme::amber); K("FILTER_RESO", "RESO", gH, Theme::amber);
            K("TAPE_TIME", "TAPE TIME", gH, Theme::amber); K("TAPE_FEEDBACK", "FEEDBACK", gH, Theme::amber); K("TAPE_FLUTTER", "FLUTTER", gH, Theme::amber); K("TAPE_MIX", "TAPE MIX", gH, Theme::amber);
            K("VINYL_CRACKLE", "VINYL", gK, Theme::cyan); K("TAPE_WARP", "WARP", gK, Theme::cyan); K("LOFI_BITS", "LOFI BITS", gK, Theme::cyan); K("SHIMMER_MIX", "SHIMMER", gK, Theme::cyan);
            K("STEREO_WIDTH", "WIDTH", gM, Theme::green); K("IRON_DRIVE", "IRON", gM, Theme::green); K("CEILING_DB", "CEILING", gM, Theme::green);
            K("FET_1176_IN", "1176 IN", gM, Theme::green); K("FET_1176_OUT", "1176 OUT", gM, Theme::green); K("PULTEC_BOOST", "PULTEC +", gM, Theme::green); K("PULTEC_ATTEN", "PULTEC -", gM, Theme::green);
            K("IN_TRIM", "IN TRIM", gM, Theme::dim); K("OUT_TRIM", "OUT TRIM", gM, Theme::dim);
            T(hushOn, a1, "HUSH_ON", "HUSH ON"); T(kyotoOn, a2, "KYOTO_ON", "KYOTO ON"); T(masterOn, a3, "MASTER_ON", "MASTER ON");
            T(pent, a4, "TUBE_PENTODE", "PENTODE"); T(f1176, a5, "FET_1176_ON", "1176 ON"); T(pult, a6, "PULTEC_ON", "PULTEC ON");
            modeBox.addItemList({ "Lowpass 24dB", "Bandpass", "Highpass" }, 1); addAndMakeVisible(modeBox); modeAtt = std::make_unique<APVTS::ComboBoxAttachment>(p.apvts, "FILTER_MODE", modeBox);
        }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(Theme::back);
            auto bay = [&](juce::Rectangle<int> r, const juce::String& t, juce::Colour c)
            {
                g.setColour(Theme::panel); g.fillRect(r); g.setColour(juce::Colour(0xff3f3f46)); g.drawRect(r, 1);
                g.setColour(c); g.setFont(juce::Font(12.0f, juce::Font::bold)); g.drawText(t, r.getX() + 12, r.getY() + 8, r.getWidth() - 24, 20, juce::Justification::left);
            };
            bay(b1, "01. HUSH ANALOG ENGINE", Theme::amber); bay(b2, "02. KYOTO SPXRIT LOFI", Theme::cyan); bay(b3, "03. MASTER PRODUCTION SUITE + MODULES", Theme::green);
            g.setColour(Theme::dim); g.setFont(juce::Font(10.0f)); g.drawText("Stages are bypassed until switched on. Rack position (Pre/Post) is set on the RACK page.", 8, getHeight() - 20, getWidth() - 16, 14, juce::Justification::left);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced(8); r.removeFromBottom(18);
            const int w = (r.getWidth() - 16) / 3; b1 = r.removeFromLeft(w); r.removeFromLeft(8); b2 = r.removeFromLeft(w); r.removeFromLeft(8); b3 = r;
            auto in = [](juce::Rectangle<int> a) { return a.withTrimmedTop(32).reduced(10, 0); };
            auto a = in(b1); hushOn.setBounds(a.removeFromTop(22)); pent.setBounds(b1.getX() + b1.getWidth() / 2, b1.getY() + 32, b1.getWidth() / 2 - 10, 22); a.removeFromTop(2);
            modeBox.setBounds(a.removeFromTop(24)); a.removeFromTop(4); layoutGrid(a, gH, 2, 110);
            auto k = in(b2); kyotoOn.setBounds(k.removeFromTop(22)); k.removeFromTop(8); layoutGrid(k, gK, 2, 120);
            auto m = in(b3); masterOn.setBounds(m.removeFromTop(22).removeFromLeft(m.getWidth() / 2)); f1176.setBounds(b3.getX() + b3.getWidth() / 2 - 10, b3.getY() + 32, 90, 22); pult.setBounds(b3.getX() + b3.getWidth() / 2 + 84, b3.getY() + 32, 100, 22);
            m.removeFromTop(2); layoutGrid(m, gM, 3, 100);
        }
    private:
        std::vector<std::unique_ptr<Knob>> knobs; std::vector<juce::Component*> gH, gK, gM;
        juce::ToggleButton hushOn, kyotoOn, masterOn, pent, f1176, pult; juce::ComboBox modeBox;
        std::unique_ptr<APVTS::ButtonAttachment> a1, a2, a3, a4, a5, a6; std::unique_ptr<APVTS::ComboBoxAttachment> modeAtt; juce::Rectangle<int> b1, b2, b3;
    };

    // ----------------------------------------------------------------- scopes
    class ScopesPage : public juce::Component
    {
    public:
        ScopesPage(BabyGirlAudioProcessor& p, AnalyzerHub& h)
            : hub(h), spectrum(h), spectrogram(h), scopeV(h), gonio(h), meters(h, p.loudness)
        {
            for (auto* c : std::initializer_list<juce::Component*>{ &spectrum, &spectrogram, &scopeV, &gonio, &meters }) addAndMakeVisible(c);
            slope.addItem("Slope 0 dB/oct", 1); slope.addItem("Slope 3 dB/oct", 2); slope.addItem("Slope 4.5 dB/oct", 3); slope.addItem("Slope 6 dB/oct", 4); slope.setSelectedId(3, juce::dontSendNotification);
            slope.onChange = [this] { static const float v[] = { 0.0f, 3.0f, 4.5f, 6.0f }; hub.slopeDb = v[juce::jlimit(0, 3, slope.getSelectedItemIndex())]; };
            range.addItem("Range 60 dB", 1); range.addItem("Range 90 dB", 2); range.addItem("Range 120 dB", 3); range.setSelectedId(2, juce::dontSendNotification);
            range.onChange = [this] { static const float v[] = { -60.0f, -90.0f, -120.0f }; hub.floorDb = v[juce::jlimit(0, 2, range.getSelectedItemIndex())]; };
            hold.setButtonText("PEAK HOLD"); hold.setToggleState(true, juce::dontSendNotification); hold.onClick = [this] { hub.holdPeaks = hold.getToggleState(); };
            freeze.setButtonText("FREEZE"); freeze.onClick = [this] { hub.frozen = freeze.getToggleState(); };
            smooth.setSliderStyle(juce::Slider::LinearHorizontal); smooth.setRange(0.0, 0.95, 0.01); smooth.setValue(hub.smoothing, juce::dontSendNotification); smooth.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            smooth.onValueChange = [this] { hub.smoothing = (float)smooth.getValue(); };
            zoom.setSliderStyle(juce::Slider::LinearHorizontal); zoom.setRange(0.0, 1.0, 0.01); zoom.setValue(0.5, juce::dontSendNotification); zoom.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            zoom.onValueChange = [this] { scopeV.zoom = (float)zoom.getValue(); };
            sl.setText("SMOOTH", juce::dontSendNotification); zl.setText("SCOPE TIME", juce::dontSendNotification);
            for (auto* l : { &sl, &zl }) { l->setFont(juce::Font(10.0f, juce::Font::bold)); l->setColour(juce::Label::textColourId, Theme::dim); addAndMakeVisible(l); }
            for (auto* c : std::initializer_list<juce::Component*>{ &slope, &range, &hold, &freeze, &smooth, &zoom }) addAndMakeVisible(c);
        }
        void tick() { if (!isShowing()) return; spectrum.repaint(); spectrogram.tick(); spectrogram.repaint(); scopeV.repaint(); gonio.tick(); gonio.repaint(); meters.repaint(); }
        void paint(juce::Graphics& g) override { g.fillAll(Theme::back); }
        void resized() override
        {
            auto r = getLocalBounds().reduced(8);
            auto strip = r.removeFromTop(26);
            slope.setBounds(strip.removeFromLeft(130)); strip.removeFromLeft(4); range.setBounds(strip.removeFromLeft(110)); strip.removeFromLeft(8);
            hold.setBounds(strip.removeFromLeft(100)); freeze.setBounds(strip.removeFromLeft(80)); strip.removeFromLeft(10);
            sl.setBounds(strip.removeFromLeft(48)); smooth.setBounds(strip.removeFromLeft(110)); strip.removeFromLeft(10); zl.setBounds(strip.removeFromLeft(72)); zoom.setBounds(strip.removeFromLeft(110));
            r.removeFromTop(6);
            auto right = r.removeFromRight(juce::jmax(320, r.getWidth() * 34 / 100)); r.removeFromRight(6);
            spectrum.setBounds(r.removeFromTop(r.getHeight() * 58 / 100)); r.removeFromTop(6); spectrogram.setBounds(r);
            meters.setBounds(right.removeFromBottom(92)); right.removeFromBottom(6); scopeV.setBounds(right.removeFromTop(right.getHeight() * 40 / 100)); right.removeFromTop(6); gonio.setBounds(right);
        }
    private:
        AnalyzerHub& hub; SpectrumView spectrum; SpectrogramView spectrogram; ScopeView scopeV; GonioView gonio; MeterView meters;
        juce::ComboBox slope, range; juce::ToggleButton hold, freeze; juce::Slider smooth, zoom; juce::Label sl, zl;
    };

    // ----------------------------------------------------------------- DreamShare
    class DreamPage : public juce::Component, private juce::ListBoxModel
    {
    public:
        DreamPage(BabyGirlAudioProcessor& p, DreamApi& a) : proc(p), api(a), list("presets", this)
        {
            factory = getFactoryPresets();
            search.setTextToShowWhenEmpty("search name / tag / author", Theme::dim); search.onReturnKey = [this] { fetch(); }; search.onTextChange = [this] { rebuild(); };
            addAndMakeVisible(search);
            tagBox.addItem("All categories", 1); for (auto* t : { "Vocal", "Mastering", "Drums", "Weird", "Ambient", "Lofi", "Bass / Acid", "Pad", "Guitar" }) tagBox.addItem(t, tagBox.getNumItems() + 1);
            tagBox.setSelectedId(1, juce::dontSendNotification); tagBox.onChange = [this] { fetch(); }; addAndMakeVisible(tagBox);
            auto B = [&](juce::TextButton& b, const char* t, juce::Colour c) { b.setButtonText(t); b.setColour(juce::TextButton::buttonColourId, Theme::panel2); b.setColour(juce::TextButton::textColourOffId, c); addAndMakeVisible(b); };
            B(refreshBtn, "REFRESH CLOUD", Theme::cyan); B(loadBtn, "LOAD SELECTED", Theme::amber); B(publishBtn, "PUBLISH CURRENT", Theme::green);
            refreshBtn.onClick = [this] { fetch(); }; loadBtn.onClick = [this] { loadSelected(); }; publishBtn.onClick = [this] { publish(); };
            nameEd.setTextToShowWhenEmpty("preset name", Theme::dim); authorEd.setTextToShowWhenEmpty("your name", Theme::dim); addAndMakeVisible(nameEd); addAndMakeVisible(authorEd);
            list.setRowHeight(46); addAndMakeVisible(list);
            status.setFont(juce::Font(11.0f)); status.setColour(juce::Label::textColourId, Theme::dim); addAndMakeVisible(status);
            endpoint.setText(api.getBaseUrl(), juce::dontSendNotification); endpoint.setFont(juce::Font(10.0f)); endpoint.setColour(juce::Label::textColourId, Theme::dim); addAndMakeVisible(endpoint);
            rebuild(); fetch();
        }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(Theme::back); g.setColour(Theme::amber); g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText("DREAMSHARE  -  community presets (factory + cloud). Presets restore the whole rack, synth and drum pattern.", 8, 6, getWidth() - 16, 18, juce::Justification::left);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced(8); r.removeFromTop(24);
            auto top = r.removeFromTop(28); search.setBounds(top.removeFromLeft(top.getWidth() * 45 / 100)); top.removeFromLeft(6); tagBox.setBounds(top.removeFromLeft(160)); top.removeFromLeft(6); refreshBtn.setBounds(top.removeFromLeft(130)); top.removeFromLeft(6); loadBtn.setBounds(top.removeFromLeft(130));
            r.removeFromTop(6); auto bot = r.removeFromBottom(30); r.removeFromBottom(4);
            nameEd.setBounds(bot.removeFromLeft(240)); bot.removeFromLeft(6); authorEd.setBounds(bot.removeFromLeft(180)); bot.removeFromLeft(6); publishBtn.setBounds(bot.removeFromLeft(150)); bot.removeFromLeft(10); status.setBounds(bot);
            endpoint.setBounds(r.removeFromBottom(14)); list.setBounds(r);
        }
    private:
        int getNumRows() override { return (int)shown.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel) override
        {
            if (row < 0 || row >= (int)shown.size()) return; const auto& p = shown[(size_t)row];
            g.setColour(sel ? Theme::amber.withAlpha(0.2f) : (row & 1 ? Theme::panel : Theme::back)); g.fillRect(0, 0, w, h);
            g.setColour(p.cloud ? Theme::cyan : Theme::amber); g.fillRect(0, 4, 3, h - 8);
            g.setColour(Theme::text); g.setFont(juce::Font(14.0f, juce::Font::bold)); g.drawText(p.name, 12, 4, w / 2, 18, juce::Justification::left);
            g.setColour(Theme::dim); g.setFont(juce::Font(11.0f)); g.drawText("by " + p.author + "   |   " + p.category + "   |   " + p.tags, 12, 24, w - 120, 16, juce::Justification::left);
            g.setColour(p.cloud ? Theme::cyan : Theme::amber); g.setFont(juce::Font(10.0f, juce::Font::bold)); g.drawText(p.cloud ? ("CLOUD  *" + juce::String(p.stars)) : "FACTORY", w - 110, 6, 100, 14, juce::Justification::right);
        }
        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override { list.selectRow(row); loadSelected(); }

        void rebuild()
        {
            shown.clear(); const auto q = search.getText().toLowerCase().trim();
            const juce::String cat = tagBox.getSelectedId() > 1 ? tagBox.getText() : juce::String();
            auto match = [&](const DreamPreset& p) { return (q.isEmpty() || (p.name + " " + p.author + " " + p.tags + " " + p.category).toLowerCase().contains(q)) && (cat.isEmpty() || p.category == cat); };
            for (auto& p : factory) if (match(p)) shown.push_back(p);
            for (auto& p : cloudList) if (match(p)) { bool dup = false; for (auto& s : shown) dup |= (s.id == p.id); if (!dup) shown.push_back(p); }
            list.updateContent(); list.repaint();
        }
        void fetch()
        {
            status.setText("contacting DreamShare...", juce::dontSendNotification);
            juce::Component::SafePointer<DreamPage> safe(this);
            api.fetchCloudPresets(search.getText(), tagBox.getSelectedId() > 1 ? tagBox.getText() : juce::String(), [safe](std::vector<DreamPreset> res, bool ok)
            {
                if (safe == nullptr) return;
                if (ok) { safe->cloudList = std::move(res); safe->status.setText(juce::String((int)safe->cloudList.size()) + " cloud presets", juce::dontSendNotification); }
                else safe->status.setText("cloud offline - showing factory presets (check network / endpoint)", juce::dontSendNotification);
                safe->rebuild();
            });
        }
        void loadSelected()
        {
            const int r = list.getSelectedRow(); if (r < 0 || r >= (int)shown.size()) { status.setText("select a preset first", juce::dontSendNotification); return; }
            proc.applyPreset(shown[(size_t)r]); status.setText("loaded: " + shown[(size_t)r].name, juce::dontSendNotification);
        }
        void publish()
        {
            const auto nm = nameEd.getText().trim(); if (nm.isEmpty()) { status.setText("give your preset a name first", juce::dontSendNotification); return; }
            auto d = proc.captureCurrent(nm, authorEd.getText().trim().isEmpty() ? "Anonymous" : authorEd.getText().trim());
            status.setText("publishing...", juce::dontSendNotification);
            juce::Component::SafePointer<DreamPage> safe(this);
            api.publishPreset(d, [safe](bool ok) { if (safe != nullptr) safe->status.setText(ok ? "published to DreamShare!" : "publish failed (offline or rejected)", juce::dontSendNotification); });
        }

        BabyGirlAudioProcessor& proc; DreamApi& api; juce::ListBox list;
        std::vector<DreamPreset> factory, cloudList, shown;
        juce::TextEditor search, nameEd, authorEd; juce::ComboBox tagBox; juce::TextButton refreshBtn, loadBtn, publishBtn; juce::Label status, endpoint;
    };
}
