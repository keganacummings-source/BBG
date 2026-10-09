#pragma once
// BabyGirl Machine Rack core.
// One Machine instance hosts any of the registry entries below. Each family has a real
// algorithm; each entry is a different voicing (ranges, topology, nonlinearity) of it.
// Names of hardware are nominative references: the DSP is an original approximation
// of the sonic character, not a circuit-exact clone.
#include "bg_common.h"
#include <atomic>

namespace bg
{
    enum Fam { F_UTIL = 0, F_DYN, F_EQ, F_FLT, F_DST, F_DLY, F_REV, F_MOD, F_PIT, F_SPA, F_GIM, F_FORGE, F_COUNT };

    inline const char* kFamNames[F_COUNT] = {
        "UTILITY", "DYNAMICS", "EQ & TONE", "FILTERS", "DISTORTION & FUZZ", "DELAY & TAPE",
        "REVERB", "MODULATION", "PITCH", "SPATIAL", "GIMMICKS & WEIRD", "FORGE (SEEDED INFINITE)" };

    struct MachineInfo
    {
        const char* name;
        const char* maker;
        int fam;
        int variant;
        const char* lab[6];
        float def[6];
    };

    // Entries MUST stay grouped by family (UI builds section headings from this order).
    // Never reorder or delete: indices are stored in saved sessions. Append inside a family only
    // at the end of that family's block for new builds; old sessions then still map correctly
    // only if appended at the very end of the table (see kExtraMachines in the editor docs).
    inline const MachineInfo kMachines[] = {
        // ---------------- UTILITY ----------------
        { "- Empty Slot -", "BabyGirl", F_UTIL, 0, { "-", "-", "-", "-", "-", "-" }, { 0, 0, 0, 0, 0, 0 } },
        { "Gain / Pan / Width Console", "BabyGirl", F_UTIL, 1, { "Gain", "Pan", "Width", "Tilt", "-", "-" }, { 0.5f, 0.5f, 0.5f, 0.5f, 0, 0 } },
        { "Polarity / DC / Mono Switch", "BabyGirl", F_UTIL, 2, { "Mono", "Invert", "DC Cut", "Swap L/R", "-", "-" }, { 0, 0, 0.3f, 0, 0, 0 } },

        // ---------------- DYNAMICS ----------------
        { "1176-LN FET Limiter", "UREI / Universal Audio", F_DYN, 0, { "Input", "Output", "Attack", "Release", "Ratio", "Grit" }, { 0.55f, 0.5f, 0.7f, 0.6f, 0.34f, 0.3f } },
        { "1176 All-Buttons-In (British)", "UREI / Universal Audio", F_DYN, 1, { "Input", "Output", "Attack", "Release", "Slam", "Grit" }, { 0.65f, 0.45f, 0.9f, 0.7f, 0.6f, 0.6f } },
        { "LA-2A Optical Leveler", "Teletronix", F_DYN, 2, { "Peak Red.", "Gain", "Comp/Limit", "HF Emph.", "Glow", "Drive" }, { 0.45f, 0.5f, 0.0f, 0.0f, 0.5f, 0.2f } },
        { "Fairchild 670 Variable-Mu", "Fairchild", F_DYN, 3, { "Input", "Threshold", "Time Const", "Makeup", "Tube", "Link" }, { 0.5f, 0.45f, 0.3f, 0.5f, 0.35f, 1.0f } },
        { "SSL G-Bus Compressor", "Solid State Logic", F_DYN, 4, { "Threshold", "Ratio", "Attack", "Release", "Makeup", "SC HPF" }, { 0.5f, 0.3f, 0.3f, 0.5f, 0.4f, 0.3f } },
        { "dbx 160 OverEasy", "dbx", F_DYN, 5, { "Threshold", "Ratio", "OverEasy", "Output", "Speed", "Punch" }, { 0.5f, 0.4f, 0.7f, 0.5f, 0.4f, 0.4f } },
        { "Distressor Harmonic Comp", "Empirical Labs", F_DYN, 6, { "Threshold", "Ratio", "Attack", "Release", "Dist 2/3", "Output" }, { 0.45f, 0.5f, 0.3f, 0.4f, 0.35f, 0.5f } },
        { "NY Smash Parallel Crusher", "BabyGirl", F_DYN, 7, { "Squash", "Attack", "Release", "Tone", "Color", "Output" }, { 0.8f, 0.2f, 0.4f, 0.5f, 0.4f, 0.4f } },
        { "Soft-Clip Brickwall Limiter", "BabyGirl", F_DYN, 8, { "Drive", "Ceiling", "Release", "Softness", "Link", "Output" }, { 0.35f, 0.95f, 0.4f, 0.5f, 1.0f, 0.5f } },
        { "Transient Shaper", "SPL-style", F_DYN, 9, { "Attack", "Sustain", "Speed", "Output", "Soft Clip", "Detect" }, { 0.65f, 0.5f, 0.5f, 0.5f, 0.2f, 0.5f } },
        { "Drawmer-Style Gate/Expander", "Drawmer-style", F_DYN, 10, { "Threshold", "Range", "Attack", "Hold", "Release", "Hysteresis" }, { 0.3f, 0.7f, 0.1f, 0.3f, 0.4f, 0.3f } },
        { "Split-Band De-Esser", "BabyGirl", F_DYN, 11, { "Threshold", "Freq", "Range", "Width", "Listen", "Air" }, { 0.45f, 0.6f, 0.5f, 0.4f, 0.0f, 0.2f } },
        { "Vocal Level Rider", "Waves-style", F_DYN, 12, { "Target", "Range", "Speed", "Gate", "Makeup", "Smooth" }, { 0.5f, 0.5f, 0.4f, 0.2f, 0.3f, 0.5f } },
        { "3-Band Dynamics Splicer", "BabyGirl", F_DYN, 13, { "Low", "Mid", "High", "X-over 1", "X-over 2", "Output" }, { 0.5f, 0.45f, 0.5f, 0.35f, 0.6f, 0.5f } },
        { "Upward Maximizer", "Aphex-style", F_DYN, 14, { "Threshold", "Boost", "Attack", "Release", "Ceiling", "Output" }, { 0.4f, 0.5f, 0.3f, 0.4f, 0.8f, 0.5f } },

        // ---------------- EQ & TONE ----------------
        { "Pultec EQP-1A Program EQ", "Pulse Techniques", F_EQ, 0, { "Low Boost", "Low Atten", "High Boost", "High Atten", "Boost Freq", "Tube" }, { 0.45f, 0.38f, 0.4f, 0.1f, 0.5f, 0.35f } },
        { "Neve 1073 Class-A EQ", "Neve", F_EQ, 1, { "Low", "Mid 1", "Mid 2", "High", "Sweep", "Iron" }, { 0.5f, 0.5f, 0.5f, 0.55f, 0.5f, 0.4f } },
        { "SSL 4000 E Channel EQ", "Solid State Logic", F_EQ, 2, { "Low", "LM", "HM", "High", "Sweep", "Color" }, { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.1f } },
        { "API 550A Proportional-Q", "API", F_EQ, 3, { "Low", "LM", "HM", "High", "Sweep", "Punch" }, { 0.5f, 0.5f, 0.5f, 0.55f, 0.5f, 0.3f } },
        { "Baxandall Tone Stack", "Hi-Fi Classic", F_EQ, 4, { "Bass", "-", "-", "Treble", "Pivot", "Warmth" }, { 0.5f, 0, 0, 0.5f, 0.5f, 0.2f } },
        { "Tilt Equalizer", "Tonelux-style", F_EQ, 5, { "Tilt", "-", "-", "-", "Pivot", "Color" }, { 0.5f, 0, 0, 0, 0.5f, 0.0f } },
        { "Aphex-Style Aural Exciter", "Aphex-style", F_EQ, 6, { "Drive", "Freq", "Blend", "Mid Fill", "Air", "Warmth" }, { 0.5f, 0.6f, 0.5f, 0.2f, 0.4f, 0.2f } },
        { "Fletcher-Munson Loudness Smile", "BabyGirl", F_EQ, 7, { "Amount", "Low Pt", "High Pt", "Mid Dip", "-", "Color" }, { 0.5f, 0.5f, 0.5f, 0.5f, 0, 0.1f } },
        { "Vintage AM Radio Speaker", "BabyGirl", F_EQ, 8, { "Low Cut", "High Cut", "Peak", "Resonance", "Drive", "Rattle" }, { 0.4f, 0.4f, 0.6f, 0.5f, 0.3f, 0.1f } },
        { "Telephone Line (300-3.4k)", "Bell-style", F_EQ, 9, { "Low Cut", "High Cut", "Nasal", "Drive", "Crackle", "Ring" }, { 0.5f, 0.5f, 0.5f, 0.35f, 0.1f, 0.0f } },

        // ---------------- FILTERS ----------------
        { "Moog Ladder LP24", "Moog-style", F_FLT, 0, { "Cutoff", "Resonance", "Drive", "Env Amt", "Env Decay", "Comp" }, { 0.55f, 0.4f, 0.3f, 0.3f, 0.4f, 0.5f } },
        { "MS-20 Dual Sallen-Key", "Korg-style", F_FLT, 1, { "LPF Cut", "LPF Peak", "HPF Cut", "HPF Peak", "Drive", "Self-Osc" }, { 0.6f, 0.55f, 0.1f, 0.4f, 0.4f, 0.0f } },
        { "TB-303 Diode Squelch", "Roland-style", F_FLT, 2, { "Cutoff", "Resonance", "Env Mod", "Decay", "Accent", "Dist" }, { 0.4f, 0.75f, 0.7f, 0.4f, 0.6f, 0.4f } },
        { "SEM State-Variable Morph", "Oberheim-style", F_FLT, 3, { "Cutoff", "Resonance", "Drive", "Mod Depth", "Mod Rate", "LP>Notch>HP" }, { 0.55f, 0.35f, 0.2f, 0.0f, 0.3f, 0.0f } },
        { "Auto-Wah Envelope Filter", "Mu-Tron-style", F_FLT, 4, { "Base", "Resonance", "Range", "Sensitivity", "Decay", "Direction" }, { 0.3f, 0.65f, 0.6f, 0.55f, 0.3f, 0.0f } },
        { "5-Vowel Formant Throat", "BabyGirl", F_FLT, 5, { "Vowel A-E-I-O-U", "Sharpness", "Gender", "Drive", "Auto Rate", "Auto Depth" }, { 0.2f, 0.6f, 0.5f, 0.2f, 0.3f, 0.0f } },
        { "Karplus-Strong Comb Resonator", "BabyGirl", F_FLT, 6, { "Pitch", "Feedback", "Damping", "Detune L/R", "Excite", "Polarity" }, { 0.35f, 0.8f, 0.4f, 0.1f, 0.5f, 0.0f } },
        { "Lorenz Chaos Filter", "BabyGirl", F_FLT, 7, { "Center", "Resonance", "Drive", "Chaos Depth", "Chaos Rate", "Bandwidth" }, { 0.5f, 0.55f, 0.2f, 0.6f, 0.35f, 0.5f } },
        { "Music-Box Resonator Bank", "BabyGirl", F_FLT, 8, { "Root Note", "Ring Time", "Detune", "Brightness", "Spread", "Scale" }, { 0.4f, 0.6f, 0.1f, 0.6f, 0.5f, 0.0f } },
        { "8-Step Pattern Filter", "BabyGirl", F_FLT, 9, { "Base", "Resonance", "Drive", "Range", "Rate", "Pattern Seed" }, { 0.35f, 0.5f, 0.2f, 0.6f, 0.4f, 0.3f } },

        // ---------------- DISTORTION & FUZZ ----------------
        { "12AX7 Triode Preamp", "Tube-style", F_DST, 0, { "Drive", "Tone", "Level", "Bias", "Plate V", "Sag" }, { 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.2f } },
        { "EL34 Pentode Crunch", "Tube-style", F_DST, 1, { "Drive", "Tone", "Level", "Bias", "Screen", "Sag" }, { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.3f } },
        { "Germanium Fuzz Face", "Dallas Arbiter-style", F_DST, 2, { "Fuzz", "Tone", "Level", "Bias", "Gate", "Temp Sag" }, { 0.7f, 0.5f, 0.5f, 0.5f, 0.1f, 0.4f } },
        { "Silicon Fuzz (Octave Fuzz)", "Maestro-style", F_DST, 3, { "Fuzz", "Tone", "Level", "Bias", "Octave", "Gate" }, { 0.65f, 0.55f, 0.5f, 0.5f, 0.3f, 0.2f } },
        { "Big Muff Pi Sustainer", "Electro-Harmonix-style", F_DST, 4, { "Sustain", "Tone", "Volume", "Bias", "Wicker", "Scoop" }, { 0.6f, 0.5f, 0.5f, 0.5f, 0.3f, 0.5f } },
        { "RAT Op-Amp Distortion", "ProCo-style", F_DST, 5, { "Distortion", "Filter", "Volume", "Bias", "Slew", "Clip Type" }, { 0.55f, 0.4f, 0.5f, 0.5f, 0.5f, 0.0f } },
        { "Centaur Transparent Overdrive", "Klon-style", F_DST, 6, { "Gain", "Treble", "Output", "Bias", "Mid Hump", "Clean Blend" }, { 0.45f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f } },
        { "Tape Saturation Deck", "BabyGirl", F_DST, 7, { "Drive", "HF Loss", "Level", "Bias", "Head Bump", "Hysteresis" }, { 0.4f, 0.4f, 0.5f, 0.5f, 0.5f, 0.4f } },
        { "Transformer Iron Core", "Neve-style", F_DST, 8, { "Drive", "Tone", "Level", "Asymmetry", "LF Girth", "HF Loss" }, { 0.45f, 0.5f, 0.5f, 0.4f, 0.5f, 0.2f } },
        { "Buchla 259 Wavefolder", "West-Coast", F_DST, 9, { "Fold", "Tone", "Level", "Symmetry", "Stages", "Smooth" }, { 0.45f, 0.6f, 0.5f, 0.5f, 0.4f, 0.5f } },
        { "Super-Fuzz Rectifier Octave", "Univox-style", F_DST, 10, { "Fuzz", "Tone", "Level", "Bias", "Octave Up", "Gate" }, { 0.6f, 0.55f, 0.5f, 0.5f, 0.6f, 0.2f } },
        { "Bit-Crusher / Rate Reducer", "BabyGirl", F_DST, 11, { "Bits", "Sample Rate", "Level", "Dither", "Jitter", "Tone" }, { 0.6f, 0.7f, 0.5f, 0.3f, 0.1f, 0.8f } },
        { "u-Law Telephony Compander", "Bell-style", F_DST, 12, { "Crush", "Tone", "Level", "mu", "Companding", "Noise" }, { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.1f } },
        { "Chebyshev Harmonic Exciter", "BabyGirl", F_DST, 13, { "Amount", "Freq", "Level", "2nd", "3rd", "5th" }, { 0.4f, 0.5f, 0.5f, 0.6f, 0.4f, 0.2f } },
        { "Class-B Crossover Grater", "BabyGirl", F_DST, 14, { "Drive", "Tone", "Level", "Deadband", "Bias", "Bite" }, { 0.5f, 0.5f, 0.5f, 0.4f, 0.5f, 0.4f } },
        { "Rust Gated Noise Fuzz", "BabyGirl", F_DST, 15, { "Fuzz", "Tone", "Level", "Rust", "Gate", "Spit" }, { 0.6f, 0.5f, 0.5f, 0.4f, 0.4f, 0.3f } },
        { "Diode Soft-Clipper", "BabyGirl", F_DST, 16, { "Drive", "Tone", "Level", "Knee", "Asym", "Clean Blend" }, { 0.45f, 0.5f, 0.5f, 0.5f, 0.3f, 0.2f } },

        // ---------------- DELAY & TAPE ----------------
        { "RE-201 Space Echo", "Roland-style", F_DLY, 0, { "Repeat Rate", "Intensity", "Tone", "Wow/Flutter", "Head Mode 1-7", "Tape Sat" }, { 0.35f, 0.45f, 0.5f, 0.3f, 0.5f, 0.4f } },
        { "BBD Analog Delay", "Boss/Panasonic-style", F_DLY, 1, { "Time", "Feedback", "Tone", "Clock Mod", "Companding", "Drive" }, { 0.3f, 0.45f, 0.4f, 0.3f, 0.4f, 0.3f } },
        { "Clean Digital Delay", "BabyGirl", F_DLY, 2, { "Time", "Feedback", "Tone", "Mod", "Spread", "Drive" }, { 0.3f, 0.4f, 0.7f, 0.0f, 0.3f, 0.0f } },
        { "Ping-Pong Cross-Feedback", "BabyGirl", F_DLY, 3, { "Time", "Feedback", "Tone", "Mod", "Offset", "Drive" }, { 0.3f, 0.5f, 0.6f, 0.1f, 0.5f, 0.1f } },
        { "Golden-Ratio Multi-Tap", "BabyGirl", F_DLY, 4, { "Time", "Feedback", "Tone", "Mod", "Tap Decay", "Drive" }, { 0.4f, 0.35f, 0.6f, 0.1f, 0.5f, 0.1f } },
        { "Fibonacci Tap Spacing", "BabyGirl", F_DLY, 5, { "Time", "Feedback", "Tone", "Mod", "Tap Decay", "Drive" }, { 0.45f, 0.4f, 0.55f, 0.1f, 0.5f, 0.1f } },
        { "King-Tubby Deep Sea Dub", "Dub-style", F_DLY, 6, { "Time", "Feedback", "Depth LP", "Sweep", "Throw", "Drive" }, { 0.4f, 0.65f, 0.35f, 0.4f, 0.0f, 0.4f } },
        { "Oil-Can Canister Delay", "Tel-Ray-style", F_DLY, 7, { "Time", "Feedback", "Tone", "Wobble", "Dull", "Drive" }, { 0.25f, 0.5f, 0.3f, 0.5f, 0.5f, 0.3f } },
        { "Echorec 4-Head Drum", "Binson-style", F_DLY, 8, { "Rate", "Sustain", "Tone", "Wow", "Heads", "Swell" }, { 0.4f, 0.55f, 0.45f, 0.3f, 0.7f, 0.5f } },
        { "Reverse Delay", "BabyGirl", F_DLY, 9, { "Time", "Feedback", "Tone", "Smooth", "Mix Fwd", "Drive" }, { 0.35f, 0.4f, 0.6f, 0.5f, 0.3f, 0.0f } },
        { "Infinite Freeze Looper", "Mooer-style", F_DLY, 10, { "Time", "Feedback", "Tone", "Mod", "FREEZE", "Drive" }, { 0.4f, 0.6f, 0.55f, 0.2f, 0.0f, 0.1f } },
        { "Ducking Delay", "BabyGirl", F_DLY, 11, { "Time", "Feedback", "Tone", "Duck Amt", "Duck Rel", "Drive" }, { 0.35f, 0.5f, 0.6f, 0.7f, 0.4f, 0.0f } },
        { "Beat Repeater / Stutter", "BabyGirl", F_DLY, 12, { "Slice", "Repeats", "Tone", "Pitch Drop", "Trigger", "Drive" }, { 0.3f, 0.7f, 0.7f, 0.0f, 0.5f, 0.1f } },
        { "Shimmer Octave Delay", "Strymon-style", F_DLY, 13, { "Time", "Feedback", "Tone", "Shimmer", "Interval", "Drive" }, { 0.4f, 0.55f, 0.6f, 0.6f, 1.0f, 0.1f } },

        // ---------------- REVERB ----------------
        { "Concert Hall FDN", "BabyGirl", F_REV, 0, { "Size", "Decay", "Damping", "Pre-Delay", "Brightness", "Modulation" }, { 0.6f, 0.55f, 0.5f, 0.15f, 0.55f, 0.35f } },
        { "EMT 140 Plate", "EMT-style", F_REV, 1, { "Size", "Decay", "Damping", "Pre-Delay", "Brightness", "Density" }, { 0.5f, 0.5f, 0.35f, 0.1f, 0.7f, 0.7f } },
        { "Accutronics Spring Tank", "Spring-style", F_REV, 2, { "Size", "Decay", "Damping", "Boing", "Brightness", "Drip" }, { 0.45f, 0.5f, 0.5f, 0.4f, 0.6f, 0.5f } },
        { "Studio Live Room", "BabyGirl", F_REV, 3, { "Size", "Decay", "Damping", "Pre-Delay", "Brightness", "Early Refl." }, { 0.35f, 0.3f, 0.5f, 0.05f, 0.6f, 0.6f } },
        { "Cathedral Nave", "BabyGirl", F_REV, 4, { "Size", "Decay", "Damping", "Pre-Delay", "Brightness", "Modulation" }, { 0.9f, 0.8f, 0.55f, 0.25f, 0.45f, 0.4f } },
        { "Cryo Ice-Cavern", "BabyGirl", F_REV, 5, { "Size", "Decay", "Damping", "Pre-Delay", "Frost", "Modulation" }, { 0.85f, 0.85f, 0.35f, 0.3f, 0.7f, 0.5f } },
        { "80s Gated Ambience", "Collins-style", F_REV, 6, { "Size", "Gate Time", "Damping", "Pre-Delay", "Brightness", "Snap" }, { 0.5f, 0.5f, 0.3f, 0.0f, 0.7f, 0.6f } },
        { "Swell Bloom Reverb", "BabyGirl", F_REV, 7, { "Size", "Decay", "Damping", "Bloom Time", "Brightness", "Modulation" }, { 0.7f, 0.7f, 0.5f, 0.6f, 0.5f, 0.5f } },
        { "Shimmer Cloud Reverb", "Eventide-style", F_REV, 8, { "Size", "Decay", "Damping", "Pre-Delay", "Shimmer", "Interval" }, { 0.75f, 0.75f, 0.4f, 0.1f, 0.6f, 1.0f } },
        { "Kyoto Shrine Chimes", "KyotoSpxrit", F_REV, 9, { "Size", "Decay", "Damping", "Chime", "Brightness", "Wind" }, { 0.7f, 0.7f, 0.4f, 0.5f, 0.7f, 0.4f } },
        { "Infinite Freeze Cloud", "BabyGirl", F_REV, 10, { "Size", "Decay", "Damping", "Pre-Delay", "Brightness", "FREEZE" }, { 0.8f, 0.8f, 0.45f, 0.1f, 0.5f, 0.0f } },

        // ---------------- MODULATION ----------------
        { "CE-2 BBD Chorus", "Boss-style", F_MOD, 0, { "Rate", "Depth", "Tone", "Feedback", "Spread", "Noise" }, { 0.3f, 0.5f, 0.5f, 0.0f, 0.7f, 0.2f } },
        { "Juno-106 Ensemble Chorus", "Roland-style", F_MOD, 1, { "Rate", "Depth", "Tone", "Mode I/II/I+II", "Spread", "Noise" }, { 0.3f, 0.5f, 0.6f, 0.9f, 0.8f, 0.2f } },
        { "SDD-320 Dimension Expander", "Roland-style", F_MOD, 2, { "Mode 1-4", "Depth", "Tone", "Width", "-", "Noise" }, { 0.5f, 0.4f, 0.6f, 0.8f, 0, 0.1f } },
        { "Solina String Ensemble", "Eminent-style", F_MOD, 3, { "Rate", "Depth", "Tone", "Vibrato", "Spread", "Hiss" }, { 0.35f, 0.6f, 0.5f, 0.2f, 0.9f, 0.2f } },
        { "Through-Zero Tape Flanger", "BabyGirl", F_MOD, 4, { "Rate", "Depth", "Manual", "Feedback", "Spread", "Neg/Pos" }, { 0.2f, 0.7f, 0.5f, 0.5f, 0.5f, 0.5f } },
        { "Phase 90 Script Phaser", "MXR-style", F_MOD, 5, { "Speed", "Depth", "Feedback", "Stages", "Spread", "Tone" }, { 0.3f, 0.7f, 0.2f, 0.2f, 0.4f, 0.5f } },
        { "Uni-Vibe Throb", "Shin-ei-style", F_MOD, 6, { "Speed", "Intensity", "Chorus/Vib", "Bulb Lag", "Spread", "Tone" }, { 0.3f, 0.7f, 0.0f, 0.5f, 0.4f, 0.5f } },
        { "Ray-Lube Canister Vibrato", "Tel-Ray-style", F_MOD, 7, { "Speed", "Depth", "Tone", "Wobble", "Spread", "Drive" }, { 0.35f, 0.5f, 0.5f, 0.3f, 0.3f, 0.2f } },
        { "Opto Tremolo (Brownface)", "Fender-style", F_MOD, 8, { "Speed", "Depth", "Shape", "Bias", "Spread", "Crossover" }, { 0.35f, 0.6f, 0.4f, 0.5f, 0.0f, 0.0f } },
        { "Vibrolux Crossover Tremolo", "Fender-style", F_MOD, 9, { "Speed", "Depth", "Shape", "X-over", "Spread", "Tube" }, { 0.35f, 0.6f, 0.2f, 0.5f, 0.5f, 0.3f } },
        { "Leslie 122 Rotary Speaker", "Leslie-style", F_MOD, 10, { "Speed", "Horn Depth", "Drum Depth", "Ramp", "Mic Spread", "Drive" }, { 0.4f, 0.7f, 0.5f, 0.4f, 0.7f, 0.3f } },
        { "Pitch-Tracking Ring Mod", "Moog-style", F_MOD, 11, { "Freq", "Depth", "Tone", "Track", "Spread", "Shape" }, { 0.4f, 0.8f, 0.6f, 0.0f, 0.2f, 0.0f } },
        { "Bode Frequency Shifter", "Moog/Bode-style", F_MOD, 12, { "Shift", "Fine", "Feedback", "Up/Down Mix", "Spread", "Tone" }, { 0.5f, 0.5f, 0.0f, 0.5f, 0.2f, 0.7f } },
        { "Auto-Pan Orbit", "BabyGirl", F_MOD, 13, { "Rate", "Depth", "Shape", "Phase", "Haas", "Tone" }, { 0.35f, 0.7f, 0.0f, 0.5f, 0.2f, 0.6f } },
        { "Harmonic Tremolo", "Magnatone-style", F_MOD, 14, { "Speed", "Depth", "Split Freq", "Phase", "Spread", "Tube" }, { 0.35f, 0.6f, 0.5f, 0.5f, 0.3f, 0.2f } },

        // ---------------- PITCH ----------------
        { "Octave Down Divider", "BabyGirl", F_PIT, 0, { "Octave", "Smooth", "Window", "Detune", "Dry Sub", "Tone" }, { 0.0f, 0.5f, 0.5f, 0.0f, 0.2f, 0.5f } },
        { "Harmonizer (Interval)", "Eventide-style", F_PIT, 1, { "Interval", "Fine", "Window", "Feedback", "Spread", "Tone" }, { 0.65f, 0.5f, 0.5f, 0.0f, 0.5f, 0.6f } },
        { "H3000 MicroPitch Thickener", "Eventide-style", F_PIT, 2, { "Detune", "Delay", "Window", "Spread", "Feedback", "Tone" }, { 0.35f, 0.3f, 0.5f, 0.8f, 0.0f, 0.7f } },
        { "Whammy Pitch Sweeper", "DigiTech-style", F_PIT, 3, { "Range", "Position", "Window", "Auto Rate", "Auto Depth", "Tone" }, { 0.5f, 0.5f, 0.5f, 0.3f, 0.0f, 0.7f } },
        { "Pitch Snap Scale Quantizer", "Auto-Tune-style", F_PIT, 4, { "Key", "Scale", "Speed", "Amount", "Window", "Tone" }, { 0.0f, 0.2f, 0.2f, 1.0f, 0.5f, 0.7f } },
        { "Cascade Detune Supersaw", "BabyGirl", F_PIT, 5, { "Spread", "Voices", "Window", "Stereo", "Drift", "Tone" }, { 0.4f, 0.6f, 0.5f, 0.8f, 0.3f, 0.7f } },

        // ---------------- SPATIAL ----------------
        { "M/S Stereo Imager", "BabyGirl", F_SPA, 0, { "Width", "Mid Gain", "Side Gain", "Bass Mono", "Side HPF", "Balance" }, { 0.55f, 0.5f, 0.5f, 0.2f, 0.2f, 0.5f } },
        { "Haas Psychoacoustic Widener", "BabyGirl", F_SPA, 1, { "Delay", "Width", "Tone", "Mix Side", "Mod", "Balance" }, { 0.3f, 0.6f, 0.6f, 0.5f, 0.1f, 0.5f } },
        { "3D HRTF Binaural Rotator", "BabyGirl", F_SPA, 2, { "Azimuth", "Distance", "Head Shadow", "Auto Spin", "Elevation", "Room" }, { 0.5f, 0.3f, 0.5f, 0.0f, 0.5f, 0.2f } },
        { "Bass Mono-Maker", "BabyGirl", F_SPA, 3, { "Freq", "Slope", "Width Above", "Phase Fix", "-", "-" }, { 0.35f, 0.5f, 0.5f, 0.3f, 0, 0 } },
        { "Allpass Decorrelator", "BabyGirl", F_SPA, 4, { "Amount", "Stages", "Spread", "Mod", "Tone", "Mono Safe" }, { 0.6f, 0.5f, 0.6f, 0.2f, 0.6f, 0.5f } },
        { "Headphone Crossfeed", "BabyGirl", F_SPA, 5, { "Feed", "Delay", "Cutoff", "Level", "-", "-" }, { 0.4f, 0.4f, 0.4f, 0.5f, 0, 0 } },
        { "Spherical Orbit Panner", "BabyGirl", F_SPA, 6, { "Rate", "Radius", "Elevation", "Doppler", "Depth", "Tilt" }, { 0.3f, 0.7f, 0.5f, 0.3f, 0.3f, 0.5f } },
        { "Stereo Spread Delay Matrix", "BabyGirl", F_SPA, 7, { "L Time", "R Time", "Feedback", "Cross", "Tone", "Width" }, { 0.2f, 0.28f, 0.2f, 0.3f, 0.6f, 0.8f } },

        // ---------------- GIMMICKS & WEIRD ----------------
        { "Haunted AM Radio", "Weird", F_GIM, 0, { "Tuning", "Static", "Heterodyne", "Drift", "Band", "Level" }, { 0.5f, 0.3f, 0.3f, 0.4f, 0.5f, 0.5f } },
        { "Sentient Toaster", "Weird", F_GIM, 1, { "Heat", "Hum", "Ding Rate", "Ding Pitch", "Crumb Noise", "Level" }, { 0.5f, 0.3f, 0.4f, 0.5f, 0.2f, 0.5f } },
        { "Gravity Well Warp", "Weird", F_GIM, 2, { "Mass", "Pull Speed", "Feedback", "Event Horizon", "Tilt", "Level" }, { 0.5f, 0.4f, 0.4f, 0.5f, 0.5f, 0.5f } },
        { "Dial-Up Modem Mangler", "Weird", F_GIM, 3, { "Carrier", "Handshake", "Mod Depth", "Screech", "Noise", "Level" }, { 0.5f, 0.4f, 0.5f, 0.3f, 0.2f, 0.5f } },
        { "Tape Stop / Vinyl Brake", "Weird", F_GIM, 4, { "Rate", "Brake Time", "Restart", "Wobble", "Random", "Level" }, { 0.3f, 0.5f, 0.4f, 0.2f, 0.4f, 0.5f } },
        { "Bubble Wrap Popper", "Weird", F_GIM, 5, { "Density", "Pop Pitch", "Pop Decay", "Ring Mod", "Random", "Level" }, { 0.4f, 0.5f, 0.3f, 0.4f, 0.5f, 0.5f } },
        { "Cosmic Ray Resonators", "Weird", F_GIM, 6, { "Flux", "Pitch", "Decay", "Spread", "Hiss", "Level" }, { 0.35f, 0.5f, 0.6f, 0.5f, 0.2f, 0.5f } },
        { "Wind Chime Transient Bells", "Weird", F_GIM, 7, { "Sensitivity", "Root", "Decay", "Scale", "Spread", "Level" }, { 0.5f, 0.5f, 0.6f, 0.2f, 0.7f, 0.5f } },
        { "Heartbeat Sidechain Pump", "Weird", F_GIM, 8, { "BPM", "Depth", "Lub-Dub Gap", "Shape", "Tone", "Level" }, { 0.35f, 0.7f, 0.4f, 0.5f, 0.5f, 0.5f } },
        { "Lava Lamp Drift", "Weird", F_GIM, 9, { "Viscosity", "Depth", "Reso", "Detune", "Heat", "Level" }, { 0.3f, 0.6f, 0.4f, 0.3f, 0.4f, 0.5f } },
        { "Washing Machine Drum", "Weird", F_GIM, 10, { "Spin", "Slosh", "Muffle", "Reverse Odds", "Load", "Level" }, { 0.4f, 0.5f, 0.5f, 0.3f, 0.5f, 0.5f } },
        { "Glitch Slice Repeater", "Weird", F_GIM, 11, { "Slice", "Repeat Odds", "Reverse Odds", "Pitch Jitter", "Hold", "Level" }, { 0.3f, 0.5f, 0.3f, 0.2f, 0.0f, 0.5f } },
        { "Random Stutter Gate", "Weird", F_GIM, 12, { "Rate", "Density", "Smooth", "Seed", "Swing", "Level" }, { 0.5f, 0.6f, 0.3f, 0.2f, 0.0f, 0.5f } },
        { "Tape Start/Stop Warble", "Weird", F_GIM, 13, { "Speed", "Wow", "Flutter", "Dropouts", "Age", "Level" }, { 0.5f, 0.4f, 0.4f, 0.2f, 0.5f, 0.5f } },
        { "Robot Voice Comb", "Weird", F_GIM, 14, { "Pitch", "Metal", "Ring", "Formant", "Robo Detune", "Level" }, { 0.4f, 0.6f, 0.4f, 0.5f, 0.2f, 0.5f } },
        { "Submerged Underwater", "Weird", F_GIM, 15, { "Depth", "Bubbles", "Muffle", "Current", "Pressure", "Level" }, { 0.5f, 0.3f, 0.5f, 0.4f, 0.5f, 0.5f } },
        { "Walkie-Talkie Squelch", "Weird", F_GIM, 16, { "Band", "Squelch", "Noise", "Click", "Distort", "Level" }, { 0.5f, 0.4f, 0.3f, 0.5f, 0.4f, 0.5f } },
        { "Time-Warp Reverse Chunks", "Weird", F_GIM, 17, { "Chunk", "Reverse Odds", "Crossfade", "Pitch", "Feedback", "Level" }, { 0.35f, 0.6f, 0.5f, 0.5f, 0.2f, 0.5f } },
        { "Spectral Smear Blur", "Weird", F_GIM, 18, { "Smear", "Stages", "Motion", "Feedback", "Tone", "Level" }, { 0.6f, 0.6f, 0.3f, 0.4f, 0.6f, 0.5f } },
        { "Radio Scanner Static", "Weird", F_GIM, 19, { "Scan Rate", "Band", "Squelch", "Lock", "Noise", "Level" }, { 0.35f, 0.5f, 0.4f, 0.2f, 0.4f, 0.5f } },
        { "Endless Drone Sustainer", "Weird", F_GIM, 20, { "Capture", "Grain", "Blur", "Shift", "Hold", "Level" }, { 0.5f, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f } },
        { "Granular Cloud Machine", "Weird", F_GIM, 21, { "Position", "Size", "Density", "Pitch", "Scatter", "Level" }, { 0.5f, 0.4f, 0.5f, 0.5f, 0.4f, 0.5f } },
        { "Blown Speaker Rattle", "Weird", F_GIM, 22, { "Damage", "Rattle", "Cone Resonance", "Fizz", "Cut", "Level" }, { 0.5f, 0.5f, 0.4f, 0.4f, 0.5f, 0.5f } },
        { "Haunted Music Box", "Weird", F_GIM, 23, { "Pitch", "Comb Decay", "Wind-Down", "Chime", "Detune", "Level" }, { 0.5f, 0.6f, 0.3f, 0.4f, 0.2f, 0.5f } },

        // ---------------- FORGE ----------------
        { "FORGE: Seeded Random Machine", "BabyGirl", F_FORGE, 0, { "Seed (10000 machines)", "Intensity", "Tone", "Motion", "Space", "Level" }, { 0.2f, 0.5f, 0.5f, 0.4f, 0.3f, 0.5f } },
    };

    inline constexpr int kNumMachines = (int)(sizeof(kMachines) / sizeof(kMachines[0]));

    inline constexpr int kForgeSeeds = 10000;

    class Machine
    {
    public:
        Machine() { for (int i = 0; i < 6; ++i) tgt[i] = q[i] = 0.5f; }

        void prepare(double sampleRate)
        {
            sr = (float)sampleRate;
            arenaSize = (int)(sr * 5.0f) + 4096;
            arena.assign((size_t)arenaSize, 0.0f);
            for (auto& o : os) o.prepare(sr);
            select(idx);
        }

        void select(int machineIndex)
        {
            idx = (machineIndex < 0 || machineIndex >= kNumMachines) ? 0 : machineIndex;
            info = &kMachines[idx];
            fam = info->fam; var = info->variant;
            cursor = 0;
            for (auto& d : dl) d = DelayLine();
            for (auto& s : sv) { s.reset(); s.lp = s.bp = s.hp = 0; }
            for (auto& b : bq) b.reset();
            for (auto& o : op) o.reset();
            for (auto& e : en) e.env = 0;
            for (auto& l : lf) l.ph = 0;
            for (auto& s : st) s = 0.0f;
            std::memset(apBuf, 0, sizeof(apBuf)); for (int& p : apPos) p = 0;
            for (int i = 0; i < 4; ++i) ps[i] = PitchShifter();
            hil[0] = Hilbert(); hil[1] = Hilbert();
            for (auto& o : os) o.prepare(sr);
            rng.seed(0x1234567u + (uint32_t)idx * 7919u);
            for (int i = 0; i < 6; ++i) q[i] = tgt[i];
            init();
            meter.store(0.0f);
        }

        int current() const { return idx; }
        void setParams(const float* p) { for (int i = 0; i < 6; ++i) tgt[i] = clampf(p[i], 0.0f, 1.0f); }
        void setBpm(double b) { bpm = (float)(b > 20.0 ? b : 120.0); }

        // Process in place. mix 0..1 (dry/wet). Wet path is fully processed per family.
        void process(float* l, float* r, int n, float mix)
        {
            if (fam == F_UTIL && var == 0) return;
            float dryL[32], dryR[32];
            int pos = 0;
            while (pos < n)
            {
                const int c = std::min(32, n - pos);
                for (int i = 0; i < 6; ++i) q[i] += (tgt[i] - q[i]) * 0.35f;
                float* pl = l + pos; float* pr = r + pos;
                std::memcpy(dryL, pl, sizeof(float) * (size_t)c);
                std::memcpy(dryR, pr, sizeof(float) * (size_t)c);
                run(pl, pr, c);
                const bool allWet = (fam == F_UTIL) && mix >= 0.999f;
                if (!allWet)
                    for (int i = 0; i < c; ++i)
                    {
                        pl[i] = dryL[i] + (pl[i] - dryL[i]) * mix;
                        pr[i] = dryR[i] + (pr[i] - dryR[i]) * mix;
                        if (!(std::fabs(pl[i]) < 64.0f)) pl[i] = 0.0f;   // NaN/inf/runaway guard
                        if (!(std::fabs(pr[i]) < 64.0f)) pr[i] = 0.0f;
                    }
                pos += c;
            }
        }

        std::atomic<float> meter { 0.0f };   // gain reduction (dB) or activity 0..1 for UI

    private:
        // ---- shared ----
        float sr = 48000.0f; int idx = 0; const MachineInfo* info = &kMachines[0]; int fam = 0, var = 0;
        float tgt[6], q[6];
        std::vector<float> arena; int arenaSize = 0, cursor = 0;
        DelayLine dl[10]; PitchShifter ps[4];
        Svf sv[24]; Biquad bq[24]; OnePole op[24]; EnvFollower en[8]; Lfo lf[8]; Rng rng; Hilbert hil[2]; Os2 os[4];
        float st[160] = {};
        float bpm = 120.0f;
        float apBuf[4][640] = {}; int apLen[4] = { 100, 100, 100, 100 }, apPos[4] = { 0, 0, 0, 0 };
        float apProc(int i, float x, float g)
        {
            float& d = apBuf[i][apPos[i] % apLen[i]];
            const float y = d - g * x; d = x + g * y; d = flushDenorm(d);
            if (++apPos[i] >= apLen[i]) apPos[i] = 0;
            return y;
        }

        float* carve(int n)
        {
            n = std::max(n, 16);
            if (cursor + n > arenaSize) n = std::max(16, arenaSize - cursor);
            float* p = arena.data() + cursor; cursor += n; return p;
        }
        void init();
        void run(float* l, float* r, int n);

        void procUtil(float*, float*, int); void procDyn(float*, float*, int); void procEq(float*, float*, int);
        void procFlt(float*, float*, int);  void procDst(float*, float*, int); void procDly(float*, float*, int);
        void procRev(float*, float*, int);  void procMod(float*, float*, int); void procPit(float*, float*, int);
        void procSpa(float*, float*, int);  void procGim(float*, float*, int); void procForge(float*, float*, int);
    };
}

#include "fam_dyn_eq.h"
#include "fam_flt_dst.h"
#include "fam_dly_rev.h"
#include "fam_mod_pit_spa.h"
#include "fam_gim_forge.h"

namespace bg
{
    inline void Machine::init()
    {
        const int sr_i = (int)sr;
        switch (fam)
        {
            case F_DLY:
                dl[0].attach(carve((int)(sr * 2.2f)), (int)(sr * 2.2f));
                dl[1].attach(carve((int)(sr * 2.2f)), (int)(sr * 2.2f));
                if (var == 13) { ps[0].attach(carve(sr_i / 5), sr_i / 5); ps[1].attach(carve(sr_i / 5), sr_i / 5); }
                break;
            case F_REV:
                for (int i = 0; i < 8; ++i) { const int n = (int)(sr * 0.24f); dl[i % 8].attach(carve(n), n); }
                dl[8].attach(carve((int)(sr * 0.3f)), (int)(sr * 0.3f));
                dl[9].attach(carve((int)(sr * 0.3f)), (int)(sr * 0.3f));
                ps[0].attach(carve(sr_i / 5), sr_i / 5); ps[1].attach(carve(sr_i / 5), sr_i / 5);
                break;
            case F_MOD:
                dl[0].attach(carve((int)(sr * 0.08f)), (int)(sr * 0.08f));
                dl[1].attach(carve((int)(sr * 0.08f)), (int)(sr * 0.08f));
                dl[2].attach(carve((int)(sr * 0.08f)), (int)(sr * 0.08f));
                dl[3].attach(carve((int)(sr * 0.08f)), (int)(sr * 0.08f));
                break;
            case F_PIT:
                for (int i = 0; i < 4; ++i) ps[i].attach(carve(sr_i / 4), sr_i / 4);
                dl[0].attach(carve((int)(sr * 0.06f)), (int)(sr * 0.06f));
                dl[1].attach(carve((int)(sr * 0.06f)), (int)(sr * 0.06f));
                break;
            case F_SPA:
                dl[0].attach(carve((int)(sr * 0.1f)), (int)(sr * 0.1f));
                dl[1].attach(carve((int)(sr * 0.1f)), (int)(sr * 0.1f));
                break;
            case F_FLT:
                dl[0].attach(carve(sr_i / 12), sr_i / 12);
                dl[1].attach(carve(sr_i / 12), sr_i / 12);
                break;
            case F_GIM:
                dl[0].attach(carve((int)(sr * 1.0f)), (int)(sr * 1.0f));
                dl[1].attach(carve((int)(sr * 1.0f)), (int)(sr * 1.0f));
                dl[2].attach(carve((int)(sr * 0.6f)), (int)(sr * 0.6f));
                dl[3].attach(carve((int)(sr * 0.6f)), (int)(sr * 0.6f));
                ps[0].attach(carve(sr_i / 5), sr_i / 5);
                ps[1].attach(carve(sr_i / 5), sr_i / 5);
                break;
            case F_FORGE:
                dl[0].attach(carve((int)(sr * 1.0f)), (int)(sr * 1.0f));
                dl[1].attach(carve((int)(sr * 1.0f)), (int)(sr * 1.0f));
                dl[2].attach(carve((int)(sr * 0.2f)), (int)(sr * 0.2f));
                dl[3].attach(carve((int)(sr * 0.2f)), (int)(sr * 0.2f));
                ps[0].attach(carve(sr_i / 5), sr_i / 5);
                ps[1].attach(carve(sr_i / 5), sr_i / 5);
                break;
            default: break;
        }
        st[63] = -1.0f; // marks "needs first-run coefficient setup" for families that care
    }

    inline void Machine::run(float* l, float* r, int n)
    {
        switch (fam)
        {
            case F_UTIL:  procUtil(l, r, n);  break;
            case F_DYN:   procDyn(l, r, n);   break;
            case F_EQ:    procEq(l, r, n);    break;
            case F_FLT:   procFlt(l, r, n);   break;
            case F_DST:   procDst(l, r, n);   break;
            case F_DLY:   procDly(l, r, n);   break;
            case F_REV:   procRev(l, r, n);   break;
            case F_MOD:   procMod(l, r, n);   break;
            case F_PIT:   procPit(l, r, n);   break;
            case F_SPA:   procSpa(l, r, n);   break;
            case F_GIM:   procGim(l, r, n);   break;
            case F_FORGE: procForge(l, r, n); break;
            default: break;
        }
    }
}
