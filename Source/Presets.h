#pragma once
#include "DreamShare.h"
#include <vector>

namespace BabyGirl
{
    struct PSlot { const char* machine; float mix; float p[6]; };

    inline DreamPreset makeRackPreset(const char* id, const char* name, const char* author, const char* category, const char* tags,
                                      std::vector<PSlot> slots, std::vector<PresetSetting> extras = {})
    {
        DreamPreset d;
        d.id = id; d.name = name; d.author = author; d.category = category; d.tags = tags; d.legacyOnly = false;
        for (size_t i = 0; i < slots.size() && i < 16; ++i)
        {
            const juce::String pre = "R" + juce::String((int)i + 1) + "_";
            d.settings.push_back({ pre + "MACH", 0.0f, slots[i].machine });
            d.settings.push_back({ pre + "ON", 1.0f, {} });
            d.settings.push_back({ pre + "MIX", slots[i].mix, {} });
            for (int k = 0; k < 6; ++k) d.settings.push_back({ pre + "P" + juce::String(k + 1), slots[i].p[k], {} });
        }
        for (auto& e : extras) d.settings.push_back(e);
        return d;
    }

    inline std::vector<DreamPreset> getFactoryPresets()
    {
        std::vector<DreamPreset> v;

        // ---- the original five (legacy layout, kept as shipped) ----
        v.push_back({ "babygirl-main", "BabyGirl Signature", "Kegana Cummings", "Producer Lead", "Warm, Tape, Tube, Master",
                      4.2f, 1850.0f, 0.48f, 0.36f, 0.32f, 0.25f, 0.28f, 0.35f, 1.25f, 0.45f });
        v.push_back({ "kyoto-shrine-ambient", "Kyoto Shrine Ghost", "KyotoSpxrit", "Lofi Ambient", "Vinyl, Cassette, Shimmer",
                      2.5f, 1400.0f, 0.35f, 0.52f, 0.65f, 0.55f, 0.60f, 0.75f, 1.45f, 0.28f });
        v.push_back({ "hush-acid-furnace", "Hush Acid Furnace", "Hush Core", "Bass / Acid", "Pentode, Resonant, Driven",
                      8.2f, 950.0f, 0.84f, 0.16f, 0.10f, 0.05f, 0.08f, 0.15f, 1.05f, 0.70f });
        v.push_back({ "master-final-polish", "Final Polish Mastering Suite", "Master Labs", "Mastering", "Multiband, TruePeak, Iron",
                      1.8f, 14000.0f, 0.15f, 0.10f, 0.08f, 0.00f, 0.00f, 0.12f, 1.35f, 0.55f });
        v.push_back({ "dreamshare-celestial", "DreamShare Celestial Cloud", "DreamShare Collective", "Ether Pad", "Vast, Modulated, Shimmer",
                      2.5f, 2800.0f, 0.52f, 0.48f, 0.45f, 0.30f, 0.35f, 0.75f, 1.60f, 0.35f });

        // ---- machine-rack presets ----
        v.push_back(makeRackPreset("fx-vocal-silk", "Vocal Chain: Silk", "BabyGirl", "Vocal", "vocal, de-ess, comp, eq",
            { { "Split-Band De-Esser", 1.0f, { 0.5f, 0.6f, 0.5f, 0.4f, 0.0f, 0.2f } },
              { "1176-LN FET Limiter", 1.0f, { 0.5f, 0.5f, 0.7f, 0.6f, 0.2f, 0.2f } },
              { "Neve 1073 Class-A EQ", 1.0f, { 0.5f, 0.55f, 0.5f, 0.62f, 0.5f, 0.4f } },
              { "LA-2A Optical Leveler", 0.8f, { 0.35f, 0.5f, 0.0f, 0.0f, 0.4f, 0.1f } },
              { "EMT 140 Plate", 0.14f, { 0.5f, 0.4f, 0.4f, 0.1f, 0.7f, 0.7f } } }));

        v.push_back(makeRackPreset("fx-master-glue", "Mastering: Glue & Gloss", "BabyGirl", "Mastering", "master, bus, limiter",
            { { "SSL G-Bus Compressor", 1.0f, { 0.35f, 0.2f, 0.3f, 0.5f, 0.3f, 0.3f } },
              { "Pultec EQP-1A Program EQ", 1.0f, { 0.3f, 0.2f, 0.35f, 0.05f, 0.5f, 0.2f } },
              { "Aphex-Style Aural Exciter", 0.35f, { 0.4f, 0.65f, 0.5f, 0.1f, 0.5f, 0.2f } },
              { "M/S Stereo Imager", 1.0f, { 0.58f, 0.5f, 0.55f, 0.25f, 0.25f, 0.5f } },
              { "Soft-Clip Brickwall Limiter", 1.0f, { 0.3f, 0.97f, 0.4f, 0.5f, 1.0f, 0.5f } } }));

        v.push_back(makeRackPreset("fx-drum-smash", "Drum Bus Smash", "BabyGirl", "Drums", "drums, parallel, smash",
            { { "Transient Shaper", 1.0f, { 0.7f, 0.5f, 0.5f, 0.5f, 0.2f, 0.5f } },
              { "NY Smash Parallel Crusher", 0.45f, { 0.85f, 0.2f, 0.4f, 0.5f, 0.4f, 0.4f } },
              { "Tape Saturation Deck", 1.0f, { 0.4f, 0.4f, 0.5f, 0.5f, 0.6f, 0.4f } } },
            { { "DRM_PATTERN", 0.0f, "3" } }));

        v.push_back(makeRackPreset("fx-dub-station", "Dub Station", "BabyGirl", "Delay / Dub", "dub, delay, spring",
            { { "MS-20 Dual Sallen-Key", 1.0f, { 0.55f, 0.45f, 0.15f, 0.3f, 0.3f, 0.0f } },
              { "King-Tubby Deep Sea Dub", 0.4f, { 0.45f, 0.7f, 0.35f, 0.4f, 0.1f, 0.4f } },
              { "Accutronics Spring Tank", 0.3f, { 0.5f, 0.55f, 0.5f, 0.5f, 0.6f, 0.5f } } }));

        v.push_back(makeRackPreset("fx-lofi-cassette", "Lofi Cassette Dream", "KyotoSpxrit", "Lofi", "lofi, tape, nostalgia",
            { { "Tape Start/Stop Warble", 1.0f, { 0.5f, 0.5f, 0.4f, 0.25f, 0.6f, 0.5f } },
              { "Tape Saturation Deck", 1.0f, { 0.5f, 0.6f, 0.5f, 0.5f, 0.6f, 0.5f } },
              { "Vintage AM Radio Speaker", 0.4f, { 0.35f, 0.6f, 0.5f, 0.4f, 0.3f, 0.1f } },
              { "BBD Analog Delay", 0.28f, { 0.3f, 0.45f, 0.35f, 0.3f, 0.4f, 0.3f } },
              { "Kyoto Shrine Chimes", 0.25f, { 0.7f, 0.7f, 0.4f, 0.5f, 0.7f, 0.4f } } }));

        v.push_back(makeRackPreset("fx-shoegaze", "Shoegaze Wall", "BabyGirl", "Guitar", "guitar, fuzz, wash",
            { { "Big Muff Pi Sustainer", 1.0f, { 0.65f, 0.45f, 0.5f, 0.5f, 0.3f, 0.5f } },
              { "CE-2 BBD Chorus", 0.5f, { 0.3f, 0.6f, 0.5f, 0.0f, 0.7f, 0.2f } },
              { "Echorec 4-Head Drum", 0.3f, { 0.4f, 0.55f, 0.45f, 0.3f, 0.7f, 0.5f } },
              { "Shimmer Cloud Reverb", 0.45f, { 0.8f, 0.8f, 0.4f, 0.1f, 0.6f, 1.0f } } }));

        v.push_back(makeRackPreset("syn-acid-bass", "Acid 303 Bass + Beat", "BabyGirl", "Bass / Acid", "acid, bass, synth, drums",
            { { "TB-303 Diode Squelch", 1.0f, { 0.4f, 0.75f, 0.7f, 0.4f, 0.6f, 0.4f } },
              { "Germanium Fuzz Face", 0.4f, { 0.5f, 0.5f, 0.5f, 0.5f, 0.1f, 0.4f } },
              { "Ping-Pong Cross-Feedback", 0.2f, { 0.3f, 0.45f, 0.55f, 0.1f, 0.5f, 0.1f } } },
            { { "SYN_OSC1", 2.0f, {} }, { "SYN_OSC2", 3.0f, {} }, { "SYN_FTYPE", 5.0f, {} }, { "SYN_CUT", 700.0f, {} }, { "SYN_RES", 0.55f, {} },
              { "SYN_FENV", 0.6f, {} }, { "SYN_D", 0.18f, {} }, { "SYN_S", 0.2f, {} }, { "SYN_SUB", 0.5f, {} }, { "DRM_PATTERN", 0.0f, "5" } }));

        v.push_back(makeRackPreset("fx-haunted-toybox", "Haunted Toy Box", "BabyGirl", "Weird", "weird, music box, granular",
            { { "Haunted Music Box", 0.6f, { 0.5f, 0.6f, 0.3f, 0.4f, 0.2f, 0.5f } },
              { "Granular Cloud Machine", 0.5f, { 0.5f, 0.4f, 0.5f, 0.5f, 0.4f, 0.5f } },
              { "Bubble Wrap Popper", 0.4f, { 0.4f, 0.5f, 0.3f, 0.4f, 0.5f, 0.5f } },
              { "Cryo Ice-Cavern", 0.3f, { 0.85f, 0.85f, 0.35f, 0.3f, 0.7f, 0.5f } } }));

        v.push_back(makeRackPreset("fx-telephone-vox", "Telephone Vox", "BabyGirl", "Vocal FX", "vocal, lofi, radio",
            { { "Telephone Line (300-3.4k)", 1.0f, { 0.5f, 0.5f, 0.5f, 0.4f, 0.1f, 0.0f } },
              { "Walkie-Talkie Squelch", 0.6f, { 0.5f, 0.4f, 0.3f, 0.5f, 0.4f, 0.5f } },
              { "Robot Voice Comb", 0.3f, { 0.4f, 0.6f, 0.4f, 0.5f, 0.2f, 0.5f } } }));

        v.push_back(makeRackPreset("fx-space-drone", "Space Drone", "BabyGirl", "Ambient", "ambient, drone, freeze",
            { { "Endless Drone Sustainer", 0.8f, { 0.5f, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f } },
              { "Bode Frequency Shifter", 0.4f, { 0.52f, 0.5f, 0.3f, 0.5f, 0.2f, 0.7f } },
              { "Cryo Ice-Cavern", 0.5f, { 0.9f, 0.9f, 0.35f, 0.3f, 0.7f, 0.5f } } }));

        v.push_back(makeRackPreset("syn-wide-pad", "Supersaw Wide Pad", "BabyGirl", "Pad", "pad, supersaw, wide",
            { { "Solina String Ensemble", 0.6f, { 0.35f, 0.6f, 0.5f, 0.2f, 0.9f, 0.2f } },
              { "Concert Hall FDN", 0.35f, { 0.7f, 0.65f, 0.5f, 0.15f, 0.55f, 0.35f } } },
            { { "SYN_OSC1", 4.0f, {} }, { "SYN_OSC2", 2.0f, {} }, { "SYN_UNI", 3.0f, {} }, { "SYN_A", 0.5f, {} }, { "SYN_R", 1.6f, {} },
              { "SYN_CUT", 2200.0f, {} }, { "SYN_FENV", 0.2f, {} } }));

        v.push_back(makeRackPreset("drm-808-trap", "808 Trap Kit", "BabyGirl", "Drums", "trap, 808, hats",
            { { "Tape Saturation Deck", 1.0f, { 0.35f, 0.4f, 0.5f, 0.5f, 0.6f, 0.4f } },
              { "Soft-Clip Brickwall Limiter", 1.0f, { 0.4f, 0.95f, 0.4f, 0.5f, 1.0f, 0.5f } } },
            { { "DRM_PATTERN", 0.0f, "2" }, { "DRM_SEQ", 2.0f, {} } }));

        v.push_back(makeRackPreset("fx-forge-roulette", "FORGE Roulette", "BabyGirl", "Weird", "forge, random, seeds",
            { { "FORGE: Seeded Random Machine", 0.7f, { 0.1234f, 0.6f, 0.5f, 0.5f, 0.3f, 0.5f } },
              { "FORGE: Seeded Random Machine", 0.5f, { 0.7777f, 0.5f, 0.5f, 0.4f, 0.4f, 0.5f } } }));

        v.push_back(makeRackPreset("fx-weird-science", "Weird Science Lab", "BabyGirl", "Weird", "chaos, modem, warp",
            { { "Lorenz Chaos Filter", 0.8f, { 0.5f, 0.55f, 0.2f, 0.6f, 0.35f, 0.5f } },
              { "Dial-Up Modem Mangler", 0.4f, { 0.5f, 0.4f, 0.5f, 0.3f, 0.2f, 0.5f } },
              { "Gravity Well Warp", 0.5f, { 0.5f, 0.4f, 0.4f, 0.5f, 0.5f, 0.5f } },
              { "Spectral Smear Blur", 0.5f, { 0.6f, 0.6f, 0.3f, 0.4f, 0.6f, 0.5f } } }));

        v.push_back(makeRackPreset("init-clean", "Init (Clean)", "BabyGirl", "Utility", "init, clean", {}));
        return v;
    }
}
