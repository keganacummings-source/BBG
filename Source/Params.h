#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/bg_machine.h"
#include "Core/bg_synth.h"
#include "Core/bg_drums.h"

namespace BabyGirl
{
    constexpr int kRackSlots = 16;

    inline juce::String rackId(int slot, const juce::String& what) { return "R" + juce::String(slot + 1) + "_" + what; }
    inline juce::String padId(int pad, const juce::String& what) { return "D" + juce::String(pad + 1) + "_" + what; }

    inline juce::StringArray machineNames()
    {
        juce::StringArray a;
        for (int i = 0; i < bg::kNumMachines; ++i) a.add(bg::kMachines[i].name);
        return a;
    }

    // Suggested wet mix when a machine is chosen: time-based effects default to a send-like mix.
    inline float defaultMixForFamily(int fam)
    {
        switch (fam)
        {
            case bg::F_DLY: return 0.32f;
            case bg::F_REV: return 0.30f;
            case bg::F_PIT: return 0.5f;
            case bg::F_MOD: return 0.6f;
            case bg::F_GIM: return 0.6f;
            case bg::F_FORGE: return 0.6f;
            default: return 1.0f;
        }
    }

    inline void addParams(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params)
    {
        using PID = juce::ParameterID;
        auto F = [&](const juce::String& id, const juce::String& name, float lo, float hi, float def)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(PID{ id, 1 }, name, juce::NormalisableRange<float>(lo, hi), def));
        };
        auto FC = [&](const juce::String& id, const juce::String& name, float lo, float hi, float def, float centre)
        {
            juce::NormalisableRange<float> r(lo, hi); r.setSkewForCentre(centre);
            params.push_back(std::make_unique<juce::AudioParameterFloat>(PID{ id, 1 }, name, r, def));
        };
        auto B = [&](const juce::String& id, const juce::String& name, bool def) { params.push_back(std::make_unique<juce::AudioParameterBool>(PID{ id, 1 }, name, def)); };
        auto C = [&](const juce::String& id, const juce::String& name, const juce::StringArray& ch, int def) { params.push_back(std::make_unique<juce::AudioParameterChoice>(PID{ id, 1 }, name, ch, def)); };
        auto I = [&](const juce::String& id, const juce::String& name, int lo, int hi, int def) { params.push_back(std::make_unique<juce::AudioParameterInt>(PID{ id, 1 }, name, lo, hi, def)); };

        // ---- global ----
        B("HUSH_ON", "Hush Stage On", false);
        B("KYOTO_ON", "Kyoto Stage On", false);
        B("MASTER_ON", "Master Stage On", true);
        C("RACK_POS", "Rack Position", { "Pre (before Hush)", "Post (after Kyoto)" }, 1);
        F("IN_TRIM", "Input Trim dB", -24.0f, 24.0f, 0.0f);
        F("OUT_TRIM", "Output Trim dB", -24.0f, 12.0f, 0.0f);

        // ---- machine rack: 16 slots ----
        const auto names = machineNames();
        for (int s = 0; s < kRackSlots; ++s)
        {
            const juce::String n = "Rack " + juce::String(s + 1);
            C(rackId(s, "MACH"), n + " Machine", names, 0);
            B(rackId(s, "ON"), n + " On", true);
            F(rackId(s, "MIX"), n + " Mix", 0.0f, 1.0f, 1.0f);
            for (int k = 0; k < 6; ++k) F(rackId(s, "P" + juce::String(k + 1)), n + " Knob " + juce::String(k + 1), 0.0f, 1.0f, 0.5f);
        }

        // ---- synth ----
        juce::StringArray osc, flt, lfo;
        for (auto* o : bg::kOscNames) osc.add(o);
        for (auto* o : bg::kFilterNames) flt.add(o);
        for (auto* o : bg::kLfoNames) lfo.add(o);
        B("SYN_ON", "Synth On", true);
        C("SYN_OSC1", "Osc 1 Wave", osc, 2); C("SYN_OSC2", "Osc 2 Wave", osc, 2);
        I("SYN_O2SEMI", "Osc 2 Semitones", -24, 24, 0); F("SYN_O2FINE", "Osc 2 Fine cents", -50.0f, 50.0f, 7.0f);
        F("SYN_MIX", "Osc Mix", 0.0f, 1.0f, 0.5f); F("SYN_SUB", "Sub Level", 0.0f, 1.0f, 0.0f); F("SYN_NOISE", "Noise Level", 0.0f, 1.0f, 0.0f);
        I("SYN_UNI", "Unison Voices", 1, 7, 1); F("SYN_UDET", "Unison Detune", 0.0f, 1.0f, 0.2f);
        F("SYN_MORPH", "Wave Morph", 0.0f, 1.0f, 0.3f); F("SYN_FM", "FM / Drawbar", 0.0f, 1.0f, 0.3f); F("SYN_PWM", "Pulse Width", 0.05f, 0.95f, 0.5f);
        C("SYN_FTYPE", "Filter Type", flt, 1);
        FC("SYN_CUT", "Filter Cutoff", 20.0f, 20000.0f, 3000.0f, 1000.0f);
        F("SYN_RES", "Filter Resonance", 0.0f, 1.0f, 0.2f); F("SYN_DRIVE", "Filter Drive", 0.0f, 1.0f, 0.0f);
        F("SYN_FENV", "Filter Env Amount", -1.0f, 1.0f, 0.4f); F("SYN_KEY", "Key Tracking", 0.0f, 1.0f, 0.5f);
        FC("SYN_A", "Amp Attack", 0.001f, 8.0f, 0.005f, 0.2f); FC("SYN_D", "Amp Decay", 0.005f, 8.0f, 0.3f, 0.5f);
        F("SYN_S", "Amp Sustain", 0.0f, 1.0f, 0.7f); FC("SYN_R", "Amp Release", 0.005f, 10.0f, 0.3f, 0.6f);
        FC("SYN_FA", "Filt Attack", 0.001f, 8.0f, 0.005f, 0.2f); FC("SYN_FD", "Filt Decay", 0.005f, 8.0f, 0.3f, 0.5f);
        F("SYN_FS", "Filt Sustain", 0.0f, 1.0f, 0.3f); FC("SYN_FR", "Filt Release", 0.005f, 10.0f, 0.3f, 0.6f);
        FC("SYN_LFORATE", "LFO Rate", 0.05f, 30.0f, 4.0f, 3.0f); C("SYN_LFOSHAPE", "LFO Shape", lfo, 0);
        F("SYN_LFOPITCH", "LFO to Pitch", 0.0f, 1.0f, 0.0f); F("SYN_LFOFILT", "LFO to Filter", 0.0f, 1.0f, 0.0f); F("SYN_LFOAMP", "LFO to Amp", 0.0f, 1.0f, 0.0f);
        FC("SYN_GLIDE", "Glide", 0.0f, 2.0f, 0.0f, 0.3f); F("SYN_LEVEL", "Synth Level", 0.0f, 1.0f, 0.7f); F("SYN_VEL", "Velocity Sens", 0.0f, 1.0f, 0.7f);

        // ---- drums ----
        B("DRM_ON", "Drums On", true);
        F("DRM_LEVEL", "Drum Master", 0.0f, 1.5f, 0.8f);
        C("DRM_SEQ", "Sequencer", { "Off", "Host Sync", "Free Run" }, 1);
        C("DRM_MAP", "Drum MIDI Map", { "Channel 10 only", "Notes 36-51 (all ch)", "Both" }, 0);
        F("DRM_BPM", "Free-Run BPM", 40.0f, 240.0f, 120.0f); F("DRM_SWING", "Swing", 0.0f, 1.0f, 0.0f);
        for (int p = 0; p < bg::kPads; ++p)
        {
            const juce::String n = bg::kPadNames[p];
            F(padId(p, "TUNE"), n + " Tune", 0.0f, 1.0f, 0.5f); F(padId(p, "DEC"), n + " Decay", 0.0f, 1.0f, 0.5f);
            F(padId(p, "LVL"), n + " Level", 0.0f, 1.0f, 0.8f); F(padId(p, "PAN"), n + " Pan", 0.0f, 1.0f, 0.5f);
        }
    }
}
