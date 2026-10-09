#pragma once
// BabyGirl polyphonic synth: 16 voices, 2 oscillators + sub + noise, 10 oscillator
// models, unison, 6 filter models, 2 ADSRs, LFO with 5 shapes, glide, pitch bend, mod wheel.
#include "bg_common.h"

namespace bg
{
    inline const char* const kOscNames[] = { "Sine", "Triangle", "Saw (BLEP)", "Square / PWM", "SuperSaw 7", "Wavetable Morph", "FM 2-Op", "Organ Drawbars", "Pluck (Karplus)", "Hard Sync Saw" };
    inline const char* const kFilterNames[] = { "LP 12", "LP 24", "Band Pass", "High Pass", "Notch", "Ladder LP" };
    inline const char* const kLfoNames[] = { "Sine", "Triangle", "Saw", "Square", "Sample & Hold" };
    constexpr int kNumOsc = 10, kNumFilters = 6, kNumLfo = 5;

    struct SynthParams
    {
        int osc1 = 2, osc2 = 2, uni = 1, fType = 1, lfoShape = 0;
        float osc2Semi = 0.0f, osc2Fine = 7.0f, mix = 0.5f, sub = 0.0f, noise = 0.0f, uniDetune = 0.2f, morph = 0.3f, fm = 0.3f, pwm = 0.5f;
        float cutoff = 3000.0f, res = 0.2f, drive = 0.0f, fEnv = 0.4f, keyTrack = 0.5f;
        float a = 0.005f, d = 0.3f, s = 0.7f, r = 0.3f, fa = 0.005f, fd = 0.3f, fs = 0.3f, fr = 0.3f;
        float lfoRate = 4.0f, lfoPitch = 0.0f, lfoFilter = 0.0f, lfoAmp = 0.0f, glide = 0.0f, level = 0.7f, velSens = 0.7f;
    };

    struct Adsr
    {
        enum { Idle, Atk, Dec, Sus, Rel } stage = Idle;
        float v = 0;
        void on() { stage = Atk; }
        void off() { if (stage != Idle) stage = Rel; }
        void kill() { stage = Idle; v = 0; }
        float tick(float a, float d, float s, float r, float sr)
        {
            switch (stage)
            {
                case Atk: v += 1.0f / std::max(a * sr, 1.0f); if (v >= 1.0f) { v = 1.0f; stage = Dec; } break;
                case Dec: v += (s - v) * (1.0f - std::exp(-4.0f / std::max(d * sr, 1.0f))); if (std::fabs(v - s) < 1e-4f) { v = s; stage = Sus; } break;
                case Sus: v = s; break;
                case Rel: v += (0.0f - v) * (1.0f - std::exp(-4.0f / std::max(r * sr, 1.0f))); if (v < 1e-4f) { v = 0; stage = Idle; } break;
                default: break;
            }
            return v;
        }
    };

    struct WaveTables
    {
        static constexpr int N = 2048, MIPS = 3, TABLES = 4;
        std::vector<float> t;  // [mip][table][N+1]
        WaveTables() { build(); }
        const float* get(int mip, int table) const { return &t[(size_t)((mip * TABLES + table) * (N + 1))]; }
        void build()
        {
            t.assign((size_t)(MIPS * TABLES * (N + 1)), 0.0f);
            static const int maxH[MIPS] = { 32, 10, 3 };
            for (int m = 0; m < MIPS; ++m)
                for (int k = 0; k < TABLES; ++k)
                {
                    float* tb = &t[(size_t)((m * TABLES + k) * (N + 1))]; float pk = 1e-9f;
                    Rng g; g.seed(1000u + (uint32_t)k);
                    float rnd[40]; for (float& x : rnd) x = g.uni();
                    for (int i = 0; i < N; ++i)
                    {
                        const float ph = kTwoPi * (float)i / (float)N; float y = 0;
                        for (int h = 1; h <= maxH[m]; ++h)
                        {
                            float a = 0;
                            if (k == 0) a = (h == 1) ? 1.0f : (h == 2 ? 0.3f : 0.0f);                       // soft sine+2nd
                            else if (k == 1) a = 1.0f / (float)h;                                           // saw
                            else if (k == 2) a = (h >= 3 && h <= 7) ? 1.0f / std::sqrt((float)h) : 0.5f / (float)h;   // vocal/formant-ish
                            else a = (h & 1 ? 1.0f : 0.0f) * (0.5f + rnd[h % 40]) / (float)std::sqrt((float)h);     // digital odd
                            y += a * std::sin((float)h * ph + (k == 3 ? rnd[h % 40] * 6.28f : 0.0f));
                        }
                        tb[i] = y; pk = std::max(pk, std::fabs(y));
                    }
                    for (int i = 0; i < N; ++i) tb[i] /= pk;
                    tb[N] = tb[0];
                }
        }
    };

    class SynthEngine
    {
    public:
        static constexpr int kVoices = 16;
        void prepare(double sampleRate) { sr = (float)sampleRate; for (auto& v : voices) v = Voice(); lastFreq = 261.63f; }
        void setParams(const SynthParams& p) { P = p; }
        void pitchBend(float semis) { bend = semis; }
        void modWheel(float m) { mod = m; }
        void sustain(bool on) { sus = on; if (!on) for (auto& v : voices) if (v.active && v.held) { v.held = false; v.ampEnv.off(); v.fltEnv.off(); } }
        void allOff() { for (auto& v : voices) v = Voice(); }

        void noteOn(int note, float vel)
        {
            Voice* v = nullptr;
            for (auto& x : voices) if (!x.active) { v = &x; break; }
            if (!v) { uint32_t oldest = 0xFFFFFFFFu; for (auto& x : voices) if (x.age < oldest) { oldest = x.age; v = &x; } }
            *v = Voice();
            v->active = true; v->note = note; v->vel = vel; v->age = ++counter; v->down = true;
            v->freq = P.glide > 0.001f ? lastFreq : mtof(note);
            v->targetFreq = mtof(note); lastFreq = v->targetFreq;
            v->ampEnv.on(); v->fltEnv.on();
            Rng g; g.seed(counter * 2654435761u + 17u);
            for (int i = 0; i < 7; ++i) { v->ph1[i] = g.uni(); v->ph2[i] = g.uni(); }
            v->rng.seed(counter * 97u + 5u);
            if (P.osc1 == 8 || P.osc2 == 8)
            {
                const int len = std::max(8, std::min(kKs - 1, (int)(sr / v->targetFreq)));
                for (int i = 0; i < kKs; ++i) v->ks[i] = 0;
                for (int i = 0; i < len; ++i) v->ks[i] = v->rng.bi();
                v->ksLen = len; v->ksPos = 0;
            }
        }
        void noteOff(int note)
        {
            for (auto& v : voices) if (v.active && v.note == note && v.down)
            {
                v.down = false;
                if (sus) v.held = true; else { v.ampEnv.off(); v.fltEnv.off(); }
            }
        }

        void process(float* l, float* r, int n)
        {
            const float srv = sr;
            lfoInc = P.lfoRate / srv;
            for (int i = 0; i < n; ++i)
            {
                lfoPh += lfoInc; if (lfoPh >= 1.0f) { lfoPh -= 1.0f; shVal = rng.bi(); }
                const float lv = lfoVal();
                const float lfoDepthMul = 0.4f + 0.6f * mod + (mod > 0.0f ? 0.0f : 0.0f);
                float oL = 0, oR = 0;
                for (auto& v : voices)
                {
                    if (!v.active) continue;
                    float sl, sr_;
                    renderVoice(v, lv * lfoDepthMul, sl, sr_);
                    oL += sl; oR += sr_;
                    if (v.ampEnv.stage == Adsr::Idle) v.active = false;
                }
                const float g = P.level * 0.35f;
                l[i] += oL * g; r[i] += oR * g;
            }
        }
        int activeVoices() const { int c = 0; for (auto& v : voices) if (v.active) ++c; return c; }

    private:
        static constexpr int kKs = 2048;
        struct Voice
        {
            bool active = false, down = false, held = false; int note = 0; float vel = 0, freq = 440, targetFreq = 440; uint32_t age = 0;
            float ph1[7] = {}, ph2[7] = {}, phSub = 0, fmPh = 0, fmPh2 = 0, sync1 = 0, sync2 = 0;
            Adsr ampEnv, fltEnv; Svf f[4]; float lad[8] = {}; float ks[kKs] = {}; int ksLen = 64, ksPos = 0; float ksLp = 0; Rng rng;
        };
        float sr = 48000.0f; SynthParams P; Voice voices[kVoices]; uint32_t counter = 0;
        float lastFreq = 261.63f, bend = 0, mod = 0; bool sus = false;
        float lfoPh = 0, lfoInc = 0, shVal = 0; Rng rng; WaveTables wt;

        static float mtof(int n) { return 440.0f * std::pow(2.0f, ((float)n - 69.0f) / 12.0f); }
        float lfoVal() const
        {
            switch (P.lfoShape) { case 0: return std::sin(kTwoPi * lfoPh); case 1: return 4.0f * std::fabs(lfoPh - 0.5f) - 1.0f; case 2: return 2.0f * lfoPh - 1.0f; case 3: return lfoPh < 0.5f ? 1.0f : -1.0f; default: return shVal; }
        }

        float osc(int type, float* ph, int uniN, float freq, Voice& v, float& phAux, bool second)
        {
            const float dt0 = freq / sr;
            if (type == 8 && !second) { return ksTick(v); }
            float acc = 0; const int cnt = (type == 4) ? 7 : ((type == 7 || type == 8) ? 1 : uniN);
            for (int u = 0; u < cnt; ++u)
            {
                float det = 0;
                if (type == 4) det = ((float)u - 3.0f) / 3.0f * (0.2f + P.uniDetune) * 0.3f;
                else if (cnt > 1) det = ((float)u - (float)(cnt - 1) * 0.5f) / ((float)(cnt - 1) * 0.5f) * P.uniDetune * 0.5f;
                const float dt = dt0 * std::pow(2.0f, det / 12.0f * (type == 4 ? 1.0f : 2.0f));
                float& p = ph[u]; p += dt; if (p >= 1.0f) { p -= 1.0f; if (type == 9) phAux = 0.0f; }
                float y = 0;
                switch (type)
                {
                    case 0: y = std::sin(kTwoPi * p); break;
                    case 1: y = 4.0f * std::fabs(p - 0.5f) - 1.0f; break;
                    case 2: case 4: y = 2.0f * p - 1.0f - polyBlep(p, dt); break;
                    case 3: { const float w = clampf(P.pwm, 0.05f, 0.95f); y = (p < w ? 1.0f : -1.0f) + polyBlep(p, dt) - polyBlep(std::fmod(p + 1.0f - w, 1.0f), dt); break; }
                    case 5:
                    {
                        const int mip = freq < 500.0f ? 0 : (freq < 2000.0f ? 1 : 2);
                        const float pos = clampf(P.morph, 0.0f, 1.0f) * (float)(WaveTables::TABLES - 1); const int t0 = std::min(WaveTables::TABLES - 2, (int)pos); const float tf = pos - (float)t0;
                        const float fi = p * (float)WaveTables::N; const int i0 = std::min((int)fi, WaveTables::N - 1); const float fr = fi - (float)i0;
                        const float* a = wt.get(mip, t0); const float* b = wt.get(mip, t0 + 1);
                        const float ya = a[i0] + (a[i0 + 1] - a[i0]) * fr, yb = b[i0] + (b[i0 + 1] - b[i0]) * fr;
                        y = ya + (yb - ya) * tf; break;
                    }
                    case 6:   // 2-op FM, ratio from morph (0.5..4), index from fm
                    {
                        const float ratio = 0.5f + std::floor(P.morph * 7.0f) * 0.5f; float& mp = (second ? v.fmPh2 : v.fmPh);
                        mp += dt * ratio * (u == 0 ? 1.0f : 1.0f); if (mp >= 1.0f) mp -= 1.0f;
                        y = std::sin(kTwoPi * p + P.fm * 6.0f * std::sin(kTwoPi * mp)); break;
                    }
                    case 7:
                    {
                        static const float dr[8] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
                        const float bar[8] = { 1.0f, 0.8f, 0.7f, P.morph, P.fm, 0.3f, 0.2f, 0.1f }; float s = 0, tot = 0;
                        for (int k = 0; k < 8; ++k) { if (freq * dr[k] > sr * 0.45f) break; s += bar[k] * std::sin(kTwoPi * std::fmod(p * dr[k] * 2.0f, 1.0f)); tot += bar[k]; }
                        y = s / std::max(tot, 1.0f) * 1.4f; break;
                    }
                    case 9:   // hard sync: slave ratio from morph
                    {
                        phAux += dt * (1.0f + P.morph * 7.0f); if (phAux >= 1.0f) phAux -= 1.0f;
                        y = 2.0f * phAux - 1.0f; break;
                    }
                    default: y = 0; break;
                }
                acc += y;
            }
            return acc / std::sqrt((float)cnt);
        }

        float ksTick(Voice& v)
        {
            const int i1 = (v.ksPos + 1) % v.ksLen;
            const float y = v.ks[v.ksPos]; const float nx = 0.5f * (y + v.ks[i1]) * (0.996f + 0.003f * P.morph);
            v.ks[v.ksPos] = flushDenorm(nx); v.ksPos = i1; return y;
        }

        void renderVoice(Voice& v, float lfo, float& outL, float& outR)
        {
            const float srv = sr;
            v.freq += (v.targetFreq - v.freq) * (P.glide > 0.001f ? 1.0f - std::exp(-1.0f / (P.glide * srv)) : 1.0f);
            const float semis = bend + lfo * P.lfoPitch * 2.0f;
            const float f1 = v.freq * std::pow(2.0f, semis / 12.0f);
            const float f2 = f1 * std::pow(2.0f, (P.osc2Semi + P.osc2Fine / 100.0f) / 12.0f);
                        float o1 = osc(P.osc1, v.ph1, P.uni, f1, v, v.sync1, false);
            float o2 = osc(P.osc2, v.ph2, P.uni, f2, v, v.sync2, true);
            float s = o1 * (1.0f - P.mix) * 2.0f * 0.5f + o2 * P.mix * 2.0f * 0.5f;
            v.phSub += f1 * 0.5f / srv; if (v.phSub >= 1.0f) v.phSub -= 1.0f;
            if (P.sub > 0.001f) s += P.sub * std::sin(kTwoPi * std::fmod(v.phSub, 1.0f)) * 0.8f;
            if (P.noise > 0.001f) s += P.noise * v.rng.bi() * 0.5f;
            // stereo spread for unison
            const float spread = P.uni > 1 ? clampf(P.uniDetune * 1.5f, 0.0f, 1.0f) : 0.0f;
            float sl = s, sr2 = s;
            if (spread > 0.0f) { const float w = 0.5f * (o1 * (1.0f - P.mix) + o2 * P.mix) * spread * ((v.note & 1) ? 1.0f : -1.0f) * 0.3f; sl += w; sr2 -= w; }
            const float ae = v.ampEnv.tick(P.a, P.d, P.s, P.r, srv), fe = v.fltEnv.tick(P.fa, P.fd, P.fs, P.fr, srv);
            const float velG = 1.0f - P.velSens + P.velSens * v.vel;
            // filter
            const float key = std::pow(2.0f, (((float)v.note - 60.0f) / 12.0f) * P.keyTrack);
            float cut = P.cutoff * key * std::pow(2.0f, fe * P.fEnv * 6.0f + lfo * P.lfoFilter * 3.0f);
            cut = clampf(cut, 20.0f, srv * 0.45f);
            const float drv = 1.0f + P.drive * 6.0f;
            float y[2] = { sl, sr2 };
            for (int c = 0; c < 2; ++c)
            {
                float x = y[c]; if (drv > 1.01f) x = fastTanh(x * drv) / std::sqrt(drv);
                switch (P.fType)
                {
                    case 5:
                    {
                        const float g = 1.0f - std::exp(-kPi * cut / srv), k = P.res * 4.0f; float* st = &v.lad[c * 4];
                        for (int k2 = 0; k2 < 2; ++k2)
                        {
                            const float in = fastTanh(x - k * st[3]);
                            st[0] += g * (in - fastTanh(st[0])); st[1] += g * (fastTanh(st[0]) - fastTanh(st[1]));
                            st[2] += g * (fastTanh(st[1]) - fastTanh(st[2])); st[3] += g * (fastTanh(st[2]) - fastTanh(st[3]));
                        }
                        x = st[3] * (1.0f + k * 0.35f); break;
                    }
                    default:
                    {
                        const float Q = 0.55f + P.res * 11.0f;
                        v.f[c].setup(srv, cut, Q); v.f[c].process(x);
                        if (P.fType == 0) x = v.f[c].lp;
                        else if (P.fType == 1) { v.f[c + 2].setup(srv, cut, 0.6f + P.res * 2.0f); v.f[c + 2].process(v.f[c].lp); x = v.f[c + 2].lp; }
                        else if (P.fType == 2) x = v.f[c].bp * v.f[c].k;
                        else if (P.fType == 3) x = v.f[c].hp;
                        else x = v.f[c].lp + v.f[c].hp;
                        break;
                    }
                }
                y[c] = x;
            }
            const float ampMod = 1.0f - P.lfoAmp * 0.5f * (1.0f - lfo);
            const float g = ae * velG * ampMod;
            outL = y[0] * g; outR = y[1] * g;
        }
    };
}
