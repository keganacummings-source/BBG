#pragma once
// BabyGirl core DSP primitives. Pure standard C++ (no JUCE) so the whole DSP core
// can be unit-tested on any machine: see tests/dsp_selftest.cpp
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <vector>

namespace bg
{
    constexpr float kPi = 3.14159265358979f;
    constexpr float kTwoPi = 6.28318530717959f;

    inline float flushDenorm(float x) { return (std::fabs(x) < 1.0e-18f) ? 0.0f : x; }
    inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
    inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
    inline float dbToGain(float db) { return std::pow(10.0f, db * 0.05f); }
    inline float gainToDb(float g) { return 20.0f * std::log10(std::max(g, 1.0e-9f)); }
    // map knob 0..1 to exponential range lo..hi
    inline float expMap(float k, float lo, float hi) { return lo * std::pow(hi / lo, clampf(k, 0.0f, 1.0f)); }
    inline float linMap(float k, float lo, float hi) { return lo + (hi - lo) * clampf(k, 0.0f, 1.0f); }

    // rational tanh approximation, smooth and bounded
    inline float fastTanh(float x)
    {
        if (x > 3.0f) return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    struct Rng
    {
        uint32_t s = 0x9E3779B9u;
        void seed(uint32_t v) { s = v ? v : 0x9E3779B9u; }
        uint32_t nextU() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        float uni() { return (float)(nextU() >> 8) * (1.0f / 16777216.0f); }   // [0,1)
        float bi() { return uni() * 2.0f - 1.0f; }                              // [-1,1)
    };

    struct OnePole
    {
        float a = 0.0f, z = 0.0f;
        void setFreq(float f, float sr) { a = 1.0f - std::exp(-kTwoPi * clampf(f, 1.0f, sr * 0.45f) / sr); }
        void setTime(float seconds, float sr) { a = seconds <= 0.0f ? 1.0f : 1.0f - std::exp(-1.0f / (seconds * sr)); }
        float lp(float x) { z += a * (x - z); z = flushDenorm(z); return z; }
        float hp(float x) { return x - lp(x); }
        void reset() { z = 0.0f; }
    };

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0.0f; }
        float process(float x)
        {
            const float y = b0 * x + z1;
            z1 = flushDenorm(b1 * x - a1 * y + z2);
            z2 = flushDenorm(b2 * x - a2 * y);
            return y;
        }
        void set(float nb0, float nb1, float nb2, float na0, float na1, float na2)
        {
            const float i = 1.0f / na0;
            b0 = nb0 * i; b1 = nb1 * i; b2 = nb2 * i; a1 = na1 * i; a2 = na2 * i;
        }
        static void prep(float sr, float& f, float& q)
        {
            f = clampf(f, 10.0f, sr * 0.47f);
            q = std::max(q, 0.05f);
        }
        void setLP(float sr, float f, float q = 0.7071f) { prep(sr, f, q); const float w = kTwoPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q); set((1 - c) * 0.5f, 1 - c, (1 - c) * 0.5f, 1 + al, -2 * c, 1 - al); }
        void setHP(float sr, float f, float q = 0.7071f) { prep(sr, f, q); const float w = kTwoPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q); set((1 + c) * 0.5f, -(1 + c), (1 + c) * 0.5f, 1 + al, -2 * c, 1 - al); }
        void setBP(float sr, float f, float q = 1.0f) { prep(sr, f, q); const float w = kTwoPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q); set(al, 0, -al, 1 + al, -2 * c, 1 - al); }
        void setNotch(float sr, float f, float q = 1.0f) { prep(sr, f, q); const float w = kTwoPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q); set(1, -2 * c, 1, 1 + al, -2 * c, 1 - al); }
        void setAP(float sr, float f, float q = 0.7071f) { prep(sr, f, q); const float w = kTwoPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q); set(1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al); }
        void setPeak(float sr, float f, float q, float gainDb)
        {
            prep(sr, f, q);
            const float A = std::pow(10.0f, gainDb / 40.0f), w = kTwoPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q);
            set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
        void setLowShelf(float sr, float f, float gainDb, float q = 0.7071f)
        {
            prep(sr, f, q);
            const float A = std::pow(10.0f, gainDb / 40.0f), w = kTwoPi * f / sr, c = std::cos(w), s = std::sin(w);
            const float al = s / (2 * q), t = 2 * std::sqrt(A) * al;
            set(A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - t),
                (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
        }
        void setHighShelf(float sr, float f, float gainDb, float q = 0.7071f)
        {
            prep(sr, f, q);
            const float A = std::pow(10.0f, gainDb / 40.0f), w = kTwoPi * f / sr, c = std::cos(w), s = std::sin(w);
            const float al = s / (2 * q), t = 2 * std::sqrt(A) * al;
            set(A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t),
                (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
        }
    };

    // Zero-delay-feedback (TPT) state variable filter
    struct Svf
    {
        float g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
        float lp = 0, bp = 0, hp = 0;
        void reset() { ic1 = ic2 = 0; }
        void setup(float sr, float f, float q)
        {
            f = clampf(f, 5.0f, sr * 0.47f);
            g = std::tan(kPi * f / sr);
            k = 1.0f / std::max(q, 0.05f);
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        float process(float x)
        {
            const float v3 = x - ic2;
            bp = a1 * ic1 + a2 * v3;
            lp = ic2 + a2 * ic1 + a3 * v3;
            ic1 = flushDenorm(2 * bp - ic1);
            ic2 = flushDenorm(2 * lp - ic2);
            hp = x - k * bp - lp;
            return lp;
        }
    };

    struct EnvFollower
    {
        float env = 0, ac = 0.1f, rc = 0.001f;
        void set(float attackSec, float releaseSec, float sr)
        {
            ac = 1.0f - std::exp(-1.0f / std::max(attackSec * sr, 1.0f));
            rc = 1.0f - std::exp(-1.0f / std::max(releaseSec * sr, 1.0f));
        }
        float process(float x)
        {
            const float a = std::fabs(x);
            env += (a > env ? ac : rc) * (a - env);
            env = flushDenorm(env);
            return env;
        }
    };

    struct Lfo
    {
        float ph = 0.0f;
        void setRate(float hz, float sr) { inc = hz / sr; }
        void advance() { ph += inc; if (ph >= 1.0f) ph -= 1.0f; if (ph < 0.0f) ph += 1.0f; }
        float sine(float off = 0) const { float p = ph + off; p -= std::floor(p); return std::sin(kTwoPi * p); }
        float tri(float off = 0) const { float p = ph + off; p -= std::floor(p); return 4.0f * std::fabs(p - 0.5f) - 1.0f; }
        float saw(float off = 0) const { float p = ph + off; p -= std::floor(p); return 2.0f * p - 1.0f; }
        float sq(float off = 0) const { float p = ph + off; p -= std::floor(p); return p < 0.5f ? 1.0f : -1.0f; }
        float inc = 0.0f;
    };

    // Delay line over externally owned memory (arena), cubic Hermite read
    struct DelayLine
    {
        float* buf = nullptr; int size = 0; int w = 0;
        void attach(float* mem, int n) { buf = mem; size = n; w = 0; if (buf) std::memset(buf, 0, sizeof(float) * (size_t)n); }
        void write(float x) { buf[w] = x; if (++w >= size) w = 0; }
        float tap(int d) const { int i = w - 1 - d; while (i < 0) i += size; return buf[i % size]; }
        float read(float d) const
        {
            d = clampf(d, 1.0f, (float)(size - 4));
            const int di = (int)d; const float f = d - (float)di;
            const float y0 = tap(di - 1 < 0 ? 0 : di - 1), y1 = tap(di), y2 = tap(di + 1), y3 = tap(di + 2);
            const float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + c0;
        }
        float readLin(float d) const
        {
            d = clampf(d, 1.0f, (float)(size - 2));
            const int di = (int)d; const float f = d - (float)di;
            return tap(di) * (1.0f - f) + tap(di + 1) * f;
        }
    };

    inline float polyBlep(float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    // 2x oversampling wrapper for nonlinear stages (zero-stuff + 4th-order LP each way)
    struct Os2
    {
        Biquad u1, u2, d1, d2;
        void prepare(float sr) { const float f = sr * 0.46f; u1.setLP(sr * 2, f, 0.54f); u2.setLP(sr * 2, f, 1.31f); d1 = u1; d2 = u2; u1.reset(); u2.reset(); d1.reset(); d2.reset(); }
        template <class F> float process(float x, F&& f)
        {
            float a = u2.process(u1.process(x * 2.0f)), b = u2.process(u1.process(0.0f));
            a = f(a); b = f(b);
            d2.process(d1.process(a));
            return d2.process(d1.process(b));
        }
    };

    // Granular two-tap pitch shifter (classic delay-modulation technique)
    struct PitchShifter
    {
        DelayLine dl; float phase = 0.0f; float winSamples = 2048.0f; float ratio = 1.0f;
        void attach(float* mem, int n) { dl.attach(mem, n); phase = 0.0f; }
        void setWindow(float samples) { winSamples = clampf(samples, 256.0f, (float)(dl.size - 8) * 0.5f); }
        float process(float x)
        {
            dl.write(x);
            const float inc = (1.0f - ratio) / winSamples;
            phase += inc; phase -= std::floor(phase);
            const float p2 = phase + 0.5f - std::floor(phase + 0.5f);
            const float d1 = phase * winSamples + 2.0f, d2 = p2 * winSamples + 2.0f;
            const float g1 = std::sin(kPi * phase), g2 = std::sin(kPi * p2);
            return dl.readLin(d1) * g1 * g1 + dl.readLin(d2) * g2 * g2;
        }
    };

    // Niemitalo 2x4-pole allpass Hilbert pair (90 degree phase split)
    struct Hilbert
    {
        float c1[4] = { 0.6923878f, 0.9360654323f, 0.9882295227f, 0.9987488453f };
        float c2[4] = { 0.4021921162f, 0.8561710882f, 0.9722909546f, 0.9952884791f };
        float x1[4][3] = {}, y1[4][3] = {}, x2[4][3] = {}, y2[4][3] = {};
        float d = 0.0f;
        void process(float in, float& re, float& im)
        {
            float v = in;
            for (int i = 0; i < 4; ++i)
            {
                const float a = c1[i] * c1[i];
                const float y = a * (v + y1[i][1]) - x1[i][1];
                x1[i][1] = x1[i][0]; x1[i][0] = v; y1[i][1] = y1[i][0]; y1[i][0] = flushDenorm(y); v = y;
            }
            float w = in;
            for (int i = 0; i < 4; ++i)
            {
                const float a = c2[i] * c2[i];
                const float y = a * (w + y2[i][1]) - x2[i][1];
                x2[i][1] = x2[i][0]; x2[i][0] = w; y2[i][1] = y2[i][0]; y2[i][0] = flushDenorm(y); w = y;
            }
            re = d; d = v;   // path 1 delayed one sample
            im = w;
        }
    };
}
