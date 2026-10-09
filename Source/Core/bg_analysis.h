#pragma once
// Metering / analysis (pure C++): ITU-R BS.1770 K-weighted loudness (momentary,
// short-term, gated integrated), 4x true-peak estimate, phase correlation,
// and a lock-free stereo ring the UI reads for scopes / FFT.
#include "bg_common.h"
#include <atomic>
#include <array>

namespace bg
{
    struct ScopeRing
    {
        static constexpr int N = 32768;   // power of two
        std::array<float, N> L {}, R {};
        std::atomic<uint32_t> w { 0 };
        void push(const float* l, const float* r, int n)
        {
            uint32_t p = w.load(std::memory_order_relaxed);
            for (int i = 0; i < n; ++i) { L[p & (N - 1)] = l[i]; R[p & (N - 1)] = r ? r[i] : l[i]; ++p; }
            w.store(p, std::memory_order_release);
        }
        // newest `count` samples, oldest first. A torn read only glitches one visual frame.
        void latest(float* dl, float* dr, int count) const
        {
            const uint32_t p = w.load(std::memory_order_acquire);
            for (int i = 0; i < count; ++i) { const uint32_t idx = (p - (uint32_t)count + (uint32_t)i) & (N - 1); if (dl) dl[i] = L[idx]; if (dr) dr[i] = R[idx]; }
        }
    };

    class LoudnessMeter
    {
    public:
        void prepare(double sampleRate)
        {
            sr = (float)sampleRate;
            setupK();
            subLen = (int)(sr * 0.1f);
            reset();
        }
        void reset()
        {
            for (auto& b : kb) b.reset();
            subAcc = 0; subCount = 0; hist.fill(0); histTotal = 0;
            for (auto& s : sub) s = 0;
            subIdx = 0; filled = 0;
            momentary.store(-100.0f); shortTerm.store(-100.0f); integrated.store(-100.0f);
            tpL = tpR = 0; corrLR = corrLL = corrRR = 0;
        }
        void process(const float* l, const float* r, int n)
        {
            float peakTp = 0;
            for (int i = 0; i < n; ++i)
            {
                const float a = l[i], b = r ? r[i] : l[i];
                // 4x true-peak estimate (cubic interpolation between samples)
                peakTp = std::max(peakTp, trueSample(a, 0)); peakTp = std::max(peakTp, trueSample(b, 1));
                const float ka = kb[1].process(kb[0].process(a)), kc = kb[3].process(kb[2].process(b));
                subAcc += (double)ka * ka + (double)kc * kc;
                corrLR += 0.0005f * (a * b - corrLR); corrLL += 0.0005f * (a * a - corrLL); corrRR += 0.0005f * (b * b - corrRR);
                if (++subCount >= subLen) { endSub(); }
            }
            truePeakDb.store(gainToDb(std::max(peakTp, 1e-6f)));
            const float den = std::sqrt(corrLL * corrRR) + 1e-9f;
            correlation.store(corrLL + corrRR < 1e-9f ? 1.0f : clampf(corrLR / den, -1.0f, 1.0f));
        }
        std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f }, truePeakDb { -100.0f }, correlation { 1.0f };

    private:
        float sr = 48000.0f; int subLen = 4800;
        Biquad kb[4];
        double subAcc = 0; int subCount = 0;
        double sub[30] = {}; int subIdx = 0, filled = 0;
        std::array<uint32_t, 800> hist {}; uint32_t histTotal = 0;
        float tpL = 0, tpR = 0; float h[2][4] = {};
        float corrLR = 0, corrLL = 0, corrRR = 0;

        void setupK()
        {
            // stage 1: high shelf, stage 2: RLB high-pass (BS.1770 constants, any sample rate)
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            double K = std::tan(3.141592653589793 * f0 / sr), Vh = std::pow(10.0, G / 20.0), Vb = std::pow(Vh, 0.4996667741545416);
            double a0 = 1.0 + K / Q + K * K;
            const double b0 = (Vh + Vb * K / Q + K * K) / a0, b1 = 2.0 * (K * K - Vh) / a0, b2 = (Vh - Vb * K / Q + K * K) / a0, a1 = 2.0 * (K * K - 1.0) / a0, a2 = (1.0 - K / Q + K * K) / a0;
            const double f1 = 38.13547087602444, Q1 = 0.5003270373238773;
            K = std::tan(3.141592653589793 * f1 / sr); a0 = 1.0 + K / Q1 + K * K;
            const double c1 = 2.0 * (K * K - 1.0) / a0, c2 = (1.0 - K / Q1 + K * K) / a0;
            for (int c = 0; c < 2; ++c)
            {
                Biquad& s1 = kb[c * 2]; Biquad& s2 = kb[c * 2 + 1];
                s1.b0 = (float)b0; s1.b1 = (float)b1; s1.b2 = (float)b2; s1.a1 = (float)a1; s1.a2 = (float)a2;
                s2.b0 = 1.0f; s2.b1 = -2.0f; s2.b2 = 1.0f; s2.a1 = (float)c1; s2.a2 = (float)c2;
            }
        }
        float trueSample(float x, int ch)
        {
            float* hh = h[ch];
            hh[0] = hh[1]; hh[1] = hh[2]; hh[2] = hh[3]; hh[3] = x;
            float pk = std::fabs(x);
            for (int k = 1; k < 4; ++k)
            {
                const float f = 0.25f * (float)k, y0 = hh[0], y1 = hh[1], y2 = hh[2], y3 = hh[3];
                const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                pk = std::max(pk, std::fabs(((c3 * f + c2) * f + c1) * f + y1));
            }
            return pk;
        }
        static float lufs(double ms) { return (float)(-0.691 + 10.0 * std::log10(std::max(ms, 1e-12))); }
        void endSub()
        {
            const double ms = subAcc / (double)subLen; subAcc = 0; subCount = 0;
            sub[subIdx] = ms; subIdx = (subIdx + 1) % 30; filled = std::min(filled + 1, 30);
            auto meanLast = [&](int cnt) { double s = 0; for (int i = 0; i < cnt; ++i) s += sub[(subIdx - 1 - i + 60) % 30]; return s / cnt; };
            if (filled >= 4)
            {
                const double m = meanLast(4); momentary.store(lufs(m));
                const float lu = lufs(m);
                if (lu > -70.0f) { const int bin = std::min(799, (int)((lu + 70.0f) * 10.0f)); ++hist[(size_t)bin]; ++histTotal; }
                integrateFromHist();
            }
            if (filled >= 30) shortTerm.store(lufs(meanLast(30)));
        }
        void integrateFromHist()
        {
            if (!histTotal) return;
            double sum = 0; uint64_t cnt = 0;
            for (int b = 0; b < 800; ++b) if (hist[(size_t)b]) { const double lu = -70.0 + (b + 0.5) / 10.0; sum += (double)hist[(size_t)b] * std::pow(10.0, (lu + 0.691) / 10.0); cnt += hist[(size_t)b]; }
            if (!cnt) return;
            const double relGate = -0.691 + 10.0 * std::log10(sum / (double)cnt) - 10.0;
            double s2 = 0; uint64_t c2 = 0;
            for (int b = 0; b < 800; ++b) if (hist[(size_t)b]) { const double lu = -70.0 + (b + 0.5) / 10.0; if (lu >= relGate) { s2 += (double)hist[(size_t)b] * std::pow(10.0, (lu + 0.691) / 10.0); c2 += hist[(size_t)b]; } }
            if (c2) integrated.store((float)(-0.691 + 10.0 * std::log10(s2 / (double)c2)));
        }
    };
}
