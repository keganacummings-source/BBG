#pragma once
// Synthesized drum machine (16 pads, MIDI notes 36..51) + 16-step host-synced sequencer.
#include "bg_common.h"

namespace bg
{
    inline const char* const kPadNames[16] = {
        "808 Kick", "909 Kick", "Punch Kick", "909 Snare", "808 Snare", "Rimshot", "Clap", "Closed Hat",
        "Open Hat", "Pedal Hat", "Low Tom", "Mid Tom", "High Tom", "Cowbell", "Crash", "Ride Bell" };
    constexpr int kPads = 16, kSteps = 16, kDrumBaseNote = 36;

    struct PadParams { float tune = 0.5f, decay = 0.5f, level = 0.8f, pan = 0.5f; };

    class DrumEngine
    {
    public:
        void prepare(double sampleRate) { sr = (float)sampleRate; for (auto& v : voices) v = Voice(); }
        void setPad(int i, const PadParams& p) { pads[i] = p; }
        void setMaster(float level) { master = level; }

        void trigger(int pad, float vel)
        {
            if (pad < 0 || pad >= kPads) return;
            // hats choke each other
            if (pad >= 7 && pad <= 9) for (int h = 7; h <= 9; ++h) if (h != pad) voices[h].alive = false;
            Voice& v = voices[pad];
            v = Voice(); v.alive = true; v.vel = vel; v.rng.seed(1234u + (uint32_t)pad * 77u + (uint32_t)(++counter));
            const float tune = std::pow(2.0f, (pads[pad].tune - 0.5f) * 1.0f);   // +-6 semitones
            v.tune = tune; v.dec = 0.35f + pads[pad].decay * 1.3f;
        }
        bool active() const { for (auto& v : voices) if (v.alive) return true; return false; }

        void process(float* l, float* r, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                float oL = 0, oR = 0;
                for (int p = 0; p < kPads; ++p)
                {
                    Voice& v = voices[p]; if (!v.alive) continue;
                    const float y = render(p, v) * pads[p].level * v.vel;
                    const float pan = pads[p].pan, gl = std::cos(pan * kPi * 0.5f) * 1.4142f, gr = std::sin(pan * kPi * 0.5f) * 1.4142f;
                    oL += y * gl; oR += y * gr;
                }
                l[i] += oL * master; r[i] += oR * master;
            }
        }

    private:
        struct Voice
        {
            bool alive = false; float vel = 1, tune = 1, dec = 1; double t = 0; float ph[8] = {}; Svf f1, f2; Rng rng; float env2 = 0;
        };
        float sr = 48000.0f, master = 0.8f; PadParams pads[kPads]; Voice voices[kPads]; uint32_t counter = 0;

        float noise(Voice& v) { return v.rng.bi(); }
        static float ex(double t, float tau) { return (float)std::exp(-t / (double)tau); }

        float metallic(Voice& v, float base)
        {
            static const float F[6] = { 205.3f, 369.6f, 304.4f, 522.7f, 540.0f, 800.0f };
            float s = 0;
            for (int k = 0; k < 6; ++k) { v.ph[k] += F[k] * base / sr; if (v.ph[k] >= 1.0f) v.ph[k] -= 1.0f; s += v.ph[k] < 0.5f ? 1.0f : -1.0f; }
            return s * (1.0f / 6.0f);
        }

        float render(int pad, Voice& v)
        {
            const double ts = v.t / (double)sr; v.t += 1.0;
            const float d = v.dec;
            float y = 0, tail = 0;
            switch (pad)
            {
                case 0: { const float f = (48.0f + 110.0f * ex(ts, 0.04f)) * v.tune; v.ph[0] += f / sr; y = std::sin(kTwoPi * v.ph[0]) * ex(ts, 0.45f * d) + noise(v) * ex(ts, 0.002f) * 0.2f; tail = ex(ts, 0.45f * d); break; }
                case 1: { const float f = (52.0f + 190.0f * ex(ts, 0.025f)) * v.tune; v.ph[0] += f / sr; y = fastTanh(std::sin(kTwoPi * v.ph[0]) * 1.8f) * ex(ts, 0.28f * d) + noise(v) * ex(ts, 0.003f) * 0.35f; tail = ex(ts, 0.28f * d); break; }
                case 2: { const float f = (55.0f + 90.0f * ex(ts, 0.03f)) * v.tune; v.ph[0] += f / sr; v.f1.setup(sr, 3500.0f, 0.8f); v.f1.process(noise(v)); y = std::sin(kTwoPi * v.ph[0]) * ex(ts, 0.2f * d) + v.f1.lp * ex(ts, 0.004f) * 0.5f; tail = ex(ts, 0.2f * d); break; }
                case 3: case 4:
                {
                    const float a = (pad == 3 ? 185.0f : 175.0f) * v.tune, b = a * 1.8f; v.ph[0] += a / sr; v.ph[1] += b / sr;
                    v.f1.setup(sr, pad == 3 ? 5500.0f : 4000.0f, 0.9f); v.f1.process(noise(v));
                    y = (std::sin(kTwoPi * v.ph[0]) * ex(ts, 0.09f * d) + std::sin(kTwoPi * v.ph[1]) * ex(ts, 0.06f * d)) * 0.5f + v.f1.hp * ex(ts, (pad == 3 ? 0.17f : 0.13f) * d) * 0.8f;
                    tail = ex(ts, 0.17f * d); break;
                }
                case 5: { v.ph[0] += 500.0f * v.tune / sr; v.ph[1] += 1700.0f * v.tune / sr; y = (std::sin(kTwoPi * v.ph[0]) + std::sin(kTwoPi * v.ph[1]) * 0.7f) * ex(ts, 0.02f * d) + noise(v) * ex(ts, 0.004f) * 0.5f; tail = ex(ts, 0.02f * d); break; }
                case 6:
                {
                    v.f1.setup(sr, 1200.0f * v.tune, 1.2f); v.f1.process(noise(v));
                    float env = 0; for (int k = 0; k < 4; ++k) { const double tk = ts - 0.011 * k; if (tk >= 0) env = std::max(env, ex(tk, k == 3 ? 0.08f * d : 0.004f)); }
                    y = v.f1.bp * v.f1.k * env * 2.0f; tail = ex(ts, 0.25f * d); break;
                }
                case 7: case 8: case 9:
                {
                    const float tau = (pad == 7 ? 0.04f : (pad == 8 ? 0.35f : 0.09f)) * d; v.f1.setup(sr, 7000.0f, 0.9f); v.f1.process(metallic(v, v.tune) + noise(v) * 0.15f);
                    y = v.f1.hp * ex(ts, tau) * 1.2f; tail = ex(ts, tau); break;
                }
                case 10: case 11: case 12:
                {
                    const float base = pad == 10 ? 90.0f : (pad == 11 ? 130.0f : 180.0f); const float f = (base + base * 0.8f * ex(ts, 0.05f)) * v.tune;
                    v.ph[0] += f / sr; y = std::sin(kTwoPi * v.ph[0]) * ex(ts, 0.28f * d) + noise(v) * ex(ts, 0.003f) * 0.15f; tail = ex(ts, 0.28f * d); break;
                }
                case 13: { v.f1.setup(sr, 700.0f * v.tune, 2.0f); v.f1.process(metallic(v, v.tune * 2.0f)); y = v.f1.bp * v.f1.k * ex(ts, 0.28f * d) * 1.4f; tail = ex(ts, 0.28f * d); break; }
                case 14: { v.f1.setup(sr, 3500.0f, 0.8f); v.f1.process(metallic(v, v.tune * 1.3f) * 0.6f + noise(v) * 0.6f); y = v.f1.hp * ex(ts, 1.3f * d) * 0.9f; tail = ex(ts, 1.3f * d); break; }
                default: { v.f1.setup(sr, 4500.0f, 1.2f); v.f1.process(metallic(v, v.tune * 1.1f)); v.ph[6] += 640.0f * v.tune / sr; y = v.f1.hp * ex(ts, 0.7f * d) * 0.8f + std::sin(kTwoPi * v.ph[6]) * ex(ts, 0.5f * d) * 0.25f; tail = ex(ts, 0.7f * d); break; }
            }
            if (tail < 0.0005f || ts > 6.0) v.alive = false;
            return y;
        }
    };

    // 16-step sequencer. velocity grid 0..127 (0 = off). Fires triggers sample-accurately.
    class DrumSequencer
    {
    public:
        struct Ev { int offset; int pad; float vel; int step; long k; };
        uint8_t grid[kPads][kSteps] = {};

        void clear() { for (auto& r : grid) for (auto& c : r) c = 0; }
        static constexpr int kPatterns = 10;
        static const char* patternName(int i)
        {
            static const char* n[kPatterns] = { "Empty", "House 4x4", "Trap Hat Roll", "Boom-Bap", "Drum & Bass", "Techno Driver", "Breakbeat", "Afrobeat Groove", "Half-Time", "Electro" };
            return n[i % kPatterns];
        }
        void loadPattern(int idx)
        {
            clear();
            auto put = [&](int pad, std::initializer_list<int> steps, int vel = 100) { for (int s : steps) grid[pad][s] = (uint8_t)vel; };
            switch (idx % kPatterns)
            {
                case 1: put(1, { 0, 4, 8, 12 }, 115); put(6, { 4, 12 }); put(7, { 2, 6, 10, 14 }, 80); put(8, { 2, 10 }, 70); break;
                case 2: put(0, { 0, 7, 10 }, 120); put(4, { 8 }, 110); put(7, { 0, 1, 2, 3, 4, 6, 7, 8, 10, 11, 12, 13, 14, 15 }, 70); put(6, { 8 }, 90); break;
                case 3: put(2, { 0, 5, 10 }, 110); put(3, { 4, 12 }, 105); put(7, { 0, 2, 4, 6, 8, 10, 12, 14 }, 75); break;
                case 4: put(1, { 0, 10 }, 120); put(3, { 4, 12 }, 115); put(7, { 0, 2, 4, 6, 8, 10, 12, 14 }, 80); put(5, { 7, 15 }, 60); break;
                case 5: put(1, { 0, 4, 8, 12 }, 120); put(8, { 2, 6, 10, 14 }, 90); put(6, { 4, 12 }, 100); put(13, { 3, 11 }, 50); break;
                case 6: put(2, { 0, 6, 10 }, 115); put(3, { 4, 12 }, 110); put(3, { 7, 15 }, 60); put(7, { 0, 2, 4, 6, 8, 10, 12, 14 }, 70); break;
                case 7: put(2, { 0, 3, 8, 11 }, 105); put(5, { 2, 5, 7, 10, 13, 15 }, 80); put(13, { 0, 6, 12 }, 70); put(7, { 0, 2, 4, 6, 8, 10, 12, 14 }, 65); put(11, { 9, 14 }, 70); break;
                case 8: put(0, { 0, 11 }, 120); put(4, { 8 }, 115); put(7, { 0, 4, 8, 12 }, 80); put(9, { 14 }, 70); break;
                case 9: put(0, { 0, 7, 10 }, 115); put(4, { 4, 12 }, 105); put(6, { 4, 12 }, 90); put(7, { 0, 2, 4, 6, 8, 10, 12, 14 }, 75); put(13, { 3, 9, 14 }, 60); break;
                default: break;
            }
        }
        // ppqStart: quarter-note position at block start. Returns number of events.
        int collect(double ppqStart, double bpm, int n, double sr, double swing, Ev* out, int maxEv) const
        {
            if (bpm < 20.0) bpm = 120.0;
            const double ppqPerSample = bpm / 60.0 / sr, ppqEnd = ppqStart + (double)n * ppqPerSample;
            int cnt = 0;
            const long kStart = (long)std::floor(ppqStart * 4.0) - 1, kEnd = (long)std::ceil(ppqEnd * 4.0) + 1;
            for (long k = kStart; k <= kEnd; ++k)
            {
                const double pos = (double)k * 0.25 + (((k % 2) + 2) % 2 == 1 ? swing * 0.0833333 : 0.0);
                if (pos < ppqStart || pos >= ppqEnd) continue;
                const int step = (int)(((k % kSteps) + kSteps) % kSteps);
                const int off = std::min(n - 1, (int)((pos - ppqStart) / ppqPerSample));
                for (int p = 0; p < kPads; ++p) if (grid[p][step] && cnt < maxEv) out[cnt++] = { off, p, (float)grid[p][step] / 127.0f, step, k };
            }
            return cnt;
        }
    };
}

// Engine-level tests used by tests/dsp_selftest.cpp
#ifdef BG_SELFTEST_ENGINES
#endif
