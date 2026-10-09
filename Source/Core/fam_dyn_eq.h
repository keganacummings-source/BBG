#pragma once
// Utility + Dynamics + EQ families (included by bg_machine.h)
namespace bg
{
    // gentle saturation helper: odd (tanh) + optional even harmonic with DC removal
    inline float colorSat(float x, float amt, float even, OnePole& dc)
    {
        if (amt < 0.001f) return x;
        const float k = 1.0f + amt * 3.0f;
        float y = fastTanh(x * k) / k;
        if (even > 0.0f) { const float e = y * y; y += even * amt * 0.35f * (e - dc.lp(e)); }
        return y;
    }

    inline void Machine::procUtil(float* l, float* r, int n)
    {
        if (var == 1)
        {
            const float g = dbToGain(linMap(q[0], -24.0f, 24.0f));
            const float pan = q[1] * 2.0f - 1.0f;
            const float pl = std::cos((pan * 0.5f + 0.5f) * kPi * 0.5f) * 1.4142f, pr = std::sin((pan * 0.5f + 0.5f) * kPi * 0.5f) * 1.4142f;
            const float w = q[2] * 2.0f;
            const float tilt = (q[3] - 0.5f) * 2.0f;
            op[0].setFreq(700.0f, sr); op[1].setFreq(700.0f, sr);
            const float gl = dbToGain(-tilt * 6.0f), gh = dbToGain(tilt * 6.0f);
            for (int i = 0; i < n; ++i)
            {
                float a = l[i], b = r[i];
                const float la = op[0].lp(a), lb = op[1].lp(b);
                a = la * gl + (a - la) * gh; b = lb * gl + (b - lb) * gh;
                const float m = (a + b) * 0.5f, s = (a - b) * 0.5f * w;
                l[i] = (m + s) * g * pl; r[i] = (m - s) * g * pr;
            }
        }
        else if (var == 2)
        {
            const bool mono = q[0] > 0.5f, inv = q[1] > 0.5f, swap = q[3] > 0.5f;
            op[0].setFreq(expMap(q[2], 1.0f, 120.0f), sr); op[1].setFreq(expMap(q[2], 1.0f, 120.0f), sr);
            for (int i = 0; i < n; ++i)
            {
                float a = op[0].hp(l[i]), b = op[1].hp(r[i]);
                if (swap) std::swap(a, b);
                if (mono) a = b = (a + b) * 0.5f;
                if (inv) { a = -a; b = -b; }
                l[i] = a; r[i] = b;
            }
        }
    }

    inline void Machine::procDyn(float* l, float* r, int n)
    {
        const float srv = sr;
        auto expc = [&](float t) { return std::exp(-1.0f / std::max(t * srv, 1.0f)); };

        // ======================= non-generic dynamics =======================
        if (var == 8)   // soft-clip brickwall limiter
        {
            const float drive = dbToGain(q[0] * 18.0f), ceilDb = linMap(q[1], -12.0f, 0.0f), ceil = dbToGain(ceilDb);
            const float relC = expc(expMap(q[2], 0.01f, 0.5f)), atkC = expc(0.0001f), soft = q[3], outG = dbToGain(linMap(q[5], -12.0f, 6.0f));
            float g = st[0] > 0.0f ? st[0] : 1.0f; float minG = 1.0f;
            for (int i = 0; i < n; ++i)
            {
                const float a = l[i] * drive, b = r[i] * drive;
                const float pk = q[4] > 0.5f ? std::max(std::fabs(a), std::fabs(b)) : 0.0f;
                const float pa = q[4] > 0.5f ? pk : std::fabs(a), pb = q[4] > 0.5f ? pk : std::fabs(b);
                const float need = std::min(1.0f, ceil / (std::max(pa, pb) + 1e-9f));
                if (need < g) g = need + (g - need) * atkC; else g += (need - g) * (1.0f - relC);
                minG = std::min(minG, g);
                float oa = a * g, ob = b * g;
                oa = lerpf(clampf(oa, -ceil, ceil), ceil * fastTanh(oa / ceil), soft);
                ob = lerpf(clampf(ob, -ceil, ceil), ceil * fastTanh(ob / ceil), soft);
                l[i] = oa * outG; r[i] = ob * outG;
            }
            st[0] = g; meter.store(-gainToDb(minG));
            return;
        }
        if (var == 9)   // transient shaper
        {
            en[0].set(expMap(q[2], 0.0003f, 0.003f), expMap(q[2], 0.01f, 0.05f), srv);
            en[1].set(expMap(q[2], 0.005f, 0.03f), expMap(q[2], 0.06f, 0.3f), srv);
            const float atkAmt = (q[0] - 0.5f) * 2.0f, susAmt = (q[1] - 0.5f) * 2.0f, outG = dbToGain(linMap(q[3], -12.0f, 12.0f));
            const float sm = 1.0f - expc(0.0002f); float act = 0;
            for (int i = 0; i < n; ++i)
            {
                const float m = std::max(std::fabs(l[i]), std::fabs(r[i]));
                const float f = en[0].process(m), s = en[1].process(m);
                const float tr = clampf((f - s) / (s + 1e-4f), 0.0f, 6.0f), su = clampf((s - f) / (s + 1e-4f), 0.0f, 1.0f);
                float gt = 1.0f + (atkAmt > 0 ? atkAmt * 0.9f * std::min(tr, 3.0f) : atkAmt * 0.6f * std::min(tr, 1.5f));
                gt *= 1.0f + susAmt * su * 1.5f;
                gt = clampf(gt, 0.1f, 6.0f);
                st[0] += (gt - st[0]) * sm; if (st[0] == 0.0f) st[0] = 1.0f;
                float a = l[i] * st[0] * outG, b = r[i] * st[0] * outG;
                if (q[4] > 0.01f) { const float k = 1.0f + q[4] * 3.0f; a = fastTanh(a * k) / k; b = fastTanh(b * k) / k; }
                l[i] = a; r[i] = b; act = std::max(act, std::fabs(st[0] - 1.0f));
            }
            meter.store(std::min(act * 6.0f, 20.0f));
            return;
        }
        if (var == 10)  // gate / expander
        {
            en[0].set(0.0001f, 0.005f, srv);
            const float thr = linMap(q[0], -70.0f, -5.0f), rng_ = dbToGain(linMap(q[1], 0.0f, -80.0f)), hyst = q[5] * 10.0f;
            const float aC = expc(expMap(q[2], 0.0002f, 0.02f)), rC = expc(expMap(q[4], 0.01f, 1.0f));
            const int holdN = (int)(expMap(q[3], 0.002f, 0.5f) * srv);
            for (int i = 0; i < n; ++i)
            {
                const float lv = gainToDb(en[0].process(std::max(std::fabs(l[i]), std::fabs(r[i]))));
                if (lv > thr) { st[3] = 1.0f; st[4] = (float)holdN; }
                else if (lv < thr - hyst) { if (st[4] > 0) st[4] -= 1.0f; else st[3] = 0.0f; }
                const float t = st[3] > 0.5f ? 1.0f : rng_;
                const float c = t > st[5] ? aC : rC;
                st[5] = t + (st[5] - t) * c; if (st[5] <= 0.0f) st[5] = rng_;
                l[i] *= st[5]; r[i] *= st[5];
            }
            meter.store(-gainToDb(std::max(st[5], 1e-4f)));
            return;
        }
        if (var == 11)  // split-band de-esser
        {
            const float f = expMap(q[1], 3000.0f, 10000.0f), qq = linMap(q[3], 4.0f, 0.8f);
            sv[0].setup(srv, f, qq); sv[1].setup(srv, f, qq);
            en[0].set(0.0002f, 0.02f, srv);
            const float thr = linMap(q[0], -10.0f, -50.0f), range = linMap(q[2], 3.0f, 24.0f);
            bq[0].setHighShelf(srv, 11000.0f, q[5] * 6.0f); bq[1] = bq[0]; bq[1].reset();
            float maxGr = 0;
            for (int i = 0; i < n; ++i)
            {
                sv[0].process(l[i]); sv[1].process(r[i]);
                const float bl = sv[0].bp * sv[0].k, br = sv[1].bp * sv[1].k;   // unity-gain band-pass
                const float d = gainToDb(en[0].process(std::max(std::fabs(bl), std::fabs(br))));
                const float gr = clampf((d - thr) * 0.8f, 0.0f, range);
                const float gl = dbToGain(-gr);
                maxGr = std::max(maxGr, gr);
                float a = l[i] - bl * (1.0f - gl), b = r[i] - br * (1.0f - gl);
                if (q[4] > 0.5f) { a = bl; b = br; }
                l[i] = bq[0].process(a); r[i] = bq[1].process(b);
            }
            meter.store(maxGr);
            return;
        }
        if (var == 12)  // vocal rider
        {
            const float target = linMap(q[0], -30.0f, -10.0f), range = linMap(q[1], 3.0f, 15.0f);
            const float sp = expc(expMap(q[2], 0.05f, 2.0f) * (0.5f + q[5])), gate = linMap(q[3], -70.0f, -30.0f), mk = dbToGain(q[4] * 12.0f);
            const float rmsC = expc(0.12f);
            for (int i = 0; i < n; ++i)
            {
                const float e = 0.5f * (l[i] * l[i] + r[i] * r[i]);
                st[1] = e + (st[1] - e) * rmsC;
                const float lv = 10.0f * std::log10(st[1] + 1e-12f);
                float want = clampf(target - lv, -range, range);
                if (lv < gate) want = st[0];
                st[0] += (want - st[0]) * (1.0f - sp);
                const float g = dbToGain(st[0]) * mk;
                l[i] *= g; r[i] *= g;
            }
            meter.store(std::fabs(st[0]));
            return;
        }
        if (var == 13)  // 3-band complementary-split dynamics
        {
            const float f1 = expMap(q[3], 80.0f, 400.0f), f2 = expMap(q[4], 1000.0f, 6000.0f), outG = dbToGain(linMap(q[5], -12.0f, 12.0f));
            sv[0].setup(srv, f1, 0.7071f); sv[1].setup(srv, f1, 0.7071f); sv[2].setup(srv, f2, 0.7071f); sv[3].setup(srv, f2, 0.7071f);
            const float aC = expc(0.010f), rC = expc(0.120f);
            float thr[3], tot = 0;
            for (int b = 0; b < 3; ++b) { thr[b] = -3.0f - q[b] * 32.0f; en[b].set(0.001f, 0.05f, srv); }
            for (int i = 0; i < n; ++i)
            {
                float bl[3], br[3];
                float lo = sv[0].process(l[i]); float hl = l[i] - lo; float mi = sv[2].process(hl); bl[0] = lo; bl[1] = mi; bl[2] = hl - mi;
                float lo2 = sv[1].process(r[i]); float hr = r[i] - lo2; float mi2 = sv[3].process(hr); br[0] = lo2; br[1] = mi2; br[2] = hr - mi2;
                float oL = 0, oR = 0;
                for (int b = 0; b < 3; ++b)
                {
                    const float d = gainToDb(en[b].process(std::max(std::fabs(bl[b]), std::fabs(br[b]))));
                    const float tg = std::max(0.0f, (d - thr[b]) * 0.75f);
                    float& g = st[10 + b];
                    g = tg + (g - tg) * (tg > g ? aC : rC);
                    const float gg = std::exp(-g * 0.115129f);
                    oL += bl[b] * gg; oR += br[b] * gg; tot = std::max(tot, g);
                }
                l[i] = oL * outG; r[i] = oR * outG;
            }
            meter.store(tot);
            return;
        }
        if (var == 14)  // upward maximizer
        {
            en[0].set(0.001f, 0.05f, srv);
            const float thr = linMap(q[0], -60.0f, -10.0f), maxB = linMap(q[1], 0.0f, 18.0f), ceil = dbToGain(linMap(q[4], -9.0f, 0.0f)), outG = dbToGain(linMap(q[5], -12.0f, 6.0f));
            const float aC = expc(expMap(q[2], 0.002f, 0.1f)), rC = expc(expMap(q[3], 0.03f, 1.0f));
            for (int i = 0; i < n; ++i)
            {
                const float d = gainToDb(en[0].process(std::max(std::fabs(l[i]), std::fabs(r[i]))));
                const float tg = clampf((thr - d) * 0.5f, 0.0f, maxB);
                st[0] = tg + (st[0] - tg) * (tg < st[0] ? aC : rC);
                const float g = dbToGain(st[0]) * outG;
                l[i] = ceil * fastTanh(l[i] * g / ceil); r[i] = ceil * fastTanh(r[i] * g / ceil);
            }
            meter.store(st[0]);
            return;
        }

        // ======================= generic compressors (variants 0..7) =======================
        float preDb = 0, thr = -20, ratio = 4, atk = 0.003f, rel = 0.15f, makeDb = 0, knee = 6, rmsMix = 0, sat = 0, even = 0, scHp = 0;
        bool opto = false, varMu = false, dualMono = false;
        float relFast = 0.06f, relSlow = 2.0f, tiltT = 0.0f;
        switch (var)
        {
            case 0: preDb = q[0] * 26.0f; thr = -6.0f; ratio = q[4] < 0.25f ? 4.0f : q[4] < 0.5f ? 8.0f : q[4] < 0.75f ? 12.0f : 20.0f;
                    atk = expMap(1.0f - q[2], 2e-5f, 8e-4f); rel = expMap(1.0f - q[3], 0.05f, 1.1f); makeDb = linMap(q[1], -12.0f, 12.0f); knee = 0; sat = q[5] * 0.6f; break;
            case 1: preDb = q[0] * 30.0f; thr = -10.0f; ratio = lerpf(8.0f, 30.0f, q[4]); atk = 2e-5f; rel = expMap(1.0f - q[3], 0.04f, 0.5f);
                    makeDb = linMap(q[1], -12.0f, 12.0f); knee = 0; sat = 0.4f + q[5] * 0.6f; break;
            case 2: thr = -q[0] * 42.0f; ratio = q[2] < 0.5f ? 3.0f : 10.0f; atk = 0.010f; opto = true; knee = 12.0f; rmsMix = 0.7f;
                    makeDb = linMap(q[1], -6.0f, 18.0f); sat = q[5] * 0.5f; even = q[4]; scHp = q[3] * 1500.0f; relFast = 0.06f; relSlow = lerpf(1.2f, 4.0f, q[4]); break;
            case 3: { preDb = q[0] * 12.0f; thr = linMap(q[1], 0.0f, -30.0f); varMu = true; knee = 20.0f;
                    static const float ta[6] = { 0.0002f, 0.0002f, 0.0004f, 0.0008f, 0.002f, 0.005f }, tr[6] = { 0.3f, 0.8f, 2.0f, 5.0f, 10.0f, 0.0f };
                    const int ti = std::min(5, (int)(q[2] * 5.99f)); atk = ta[ti]; rel = tr[ti] > 0 ? tr[ti] : 1.0f; opto = (tr[ti] == 0.0f); relFast = 0.2f; relSlow = 8.0f;
                    makeDb = linMap(q[3], -6.0f, 18.0f); sat = q[4] * 0.6f; even = 0.7f; dualMono = q[5] < 0.5f; break; }
            case 4: { thr = linMap(q[0], 0.0f, -30.0f); ratio = q[1] < 0.34f ? 2.0f : q[1] < 0.67f ? 4.0f : 10.0f;
                    static const float ta[6] = { 0.0001f, 0.0003f, 0.001f, 0.003f, 0.01f, 0.03f }, tr[5] = { 0.1f, 0.3f, 0.6f, 1.2f, 0.0f };
                    atk = ta[std::min(5, (int)(q[2] * 5.99f))]; const float rv = tr[std::min(4, (int)(q[3] * 4.99f))]; rel = rv > 0 ? rv : 0.4f; opto = (rv == 0.0f); relFast = 0.1f; relSlow = 1.2f;
                    knee = 4.0f; makeDb = linMap(q[4], 0.0f, 15.0f); scHp = expMap(q[5], 20.0f, 300.0f); rmsMix = 0.2f; sat = 0.1f; break; }
            case 5: thr = linMap(q[0], 0.0f, -30.0f); ratio = lerpf(1.5f, 20.0f, q[1]); knee = linMap(q[2], 0.0f, 30.0f); makeDb = linMap(q[3], -12.0f, 12.0f);
                    atk = expMap(1.0f - q[4], 0.0005f, 0.02f); rel = lerpf(0.05f, 0.4f, q[5]); rmsMix = 1.0f; break;
            case 6: thr = linMap(q[0], 0.0f, -35.0f); ratio = expMap(q[1], 1.5f, 20.0f); atk = expMap(q[2], 0.00005f, 0.05f); rel = expMap(q[3], 0.03f, 1.5f);
                    sat = q[4] * 0.8f; even = 1.0f; makeDb = linMap(q[5], -12.0f, 12.0f); knee = 3.0f; break;
            default: thr = -10.0f - q[0] * 30.0f; ratio = 20.0f; atk = expMap(q[1], 0.0001f, 0.03f); rel = expMap(q[2], 0.02f, 0.8f);
                    makeDb = q[0] * 20.0f + linMap(q[5], -6.0f, 6.0f); knee = 6.0f; sat = q[4] * 0.7f; tiltT = (q[3] - 0.5f) * 2.0f; break;
        }
        const float preG = dbToGain(preDb), makeG = dbToGain(makeDb);
        const float aC = expc(atk), rC = expc(rel), rCf = expc(relFast), rCs = expc(relSlow), rmsC = expc(0.015f);
        if (scHp > 0) { op[4].setFreq(scHp, srv); op[5].setFreq(scHp, srv); }
        if (var == 7) { op[6].setFreq(900.0f, srv); op[7].setFreq(900.0f, srv); }
        const float slopeBase = 1.0f;
        (void)slopeBase;
        float grMax = 0;

        auto calc = [&](float det, float& g) -> float
        {
            const float ldb = 20.0f * std::log10(det + 1e-9f), over = ldb - thr;
            const float rt = varMu ? 1.5f + clampf(over / 14.0f, 0.0f, 1.0f) * 5.0f : ratio;
            const float slope = 1.0f - 1.0f / rt;
            float t;
            if (knee > 0.01f && 2.0f * over < -knee) t = 0.0f;
            else if (knee > 0.01f && 2.0f * std::fabs(over) <= knee) { const float x = over + knee * 0.5f; t = slope * x * x / (2.0f * knee); }
            else t = over > 0.0f ? over * slope : 0.0f;
            float c;
            if (t > g) c = aC;
            else c = opto ? lerpf(rCf, rCs, clampf(g / 8.0f, 0.0f, 1.0f)) : rC;
            g = t + (g - t) * c;
            return std::exp(-g * 0.115129f);
        };

        for (int i = 0; i < n; ++i)
        {
            const float a = l[i] * preG, b = r[i] * preG;
            float sa = a, sb = b;
            if (scHp > 0) { sa = op[4].hp(a); sb = op[5].hp(b); }
            float ga, gb;
            const float pa = std::fabs(sa), pb = std::fabs(sb);
            if (dualMono)
            {
                ga = calc(pa, st[0]); gb = calc(pb, st[1]);
            }
            else
            {
                float det = std::max(pa, pb);
                if (rmsMix > 0.0f) { st[2] = det * det + (st[2] - det * det) * rmsC; det = lerpf(det, std::sqrt(st[2]) * 1.2f, rmsMix); }
                ga = gb = calc(det, st[0]); st[1] = st[0];
            }
            float oa = a * ga * makeG, ob = b * gb * makeG;
            if (sat > 0.0f) { oa = colorSat(oa, sat, even, op[0]); ob = colorSat(ob, sat, even, op[1]); }
            if (var == 7) { const float la = op[6].lp(oa), lb = op[7].lp(ob); oa = la * (1.0f - tiltT * 0.6f) + (oa - la) * (1.0f + tiltT * 0.6f); ob = lb * (1.0f - tiltT * 0.6f) + (ob - lb) * (1.0f + tiltT * 0.6f); }
            l[i] = oa; r[i] = ob;
            grMax = std::max(grMax, std::max(st[0], st[1]));
        }
        meter.store(grMax);
    }

    inline void Machine::procEq(float* l, float* r, int n)
    {
        const float srv = sr;
        auto both = [&](int slot) { bq[slot + 8].b0 = bq[slot].b0; bq[slot + 8].b1 = bq[slot].b1; bq[slot + 8].b2 = bq[slot].b2; bq[slot + 8].a1 = bq[slot].a1; bq[slot + 8].a2 = bq[slot].a2; };
        const float c5 = q[5];
        bool useBands = true;
        auto bip = [](float k, float range) { return (k - 0.5f) * 2.0f * range; };
        switch (var)
        {
            case 0:
                bq[0].setLowShelf(srv, 60.0f, q[0] * 13.5f, 0.6f);
                bq[1].setLowShelf(srv, 100.0f, -q[1] * 16.0f, 0.6f);
                bq[2].setPeak(srv, expMap(q[4], 3000.0f, 16000.0f), 0.7f, q[2] * 18.0f);
                bq[3].setHighShelf(srv, 10000.0f, -q[3] * 16.0f, 0.6f);
                break;
            case 1:
                bq[0].setLowShelf(srv, 100.0f, bip(q[0], 16.0f), 0.6f);
                bq[1].setPeak(srv, expMap(q[4], 300.0f, 1500.0f), 1.1f, bip(q[1], 16.0f));
                bq[2].setPeak(srv, expMap(q[4], 1500.0f, 7200.0f), 1.1f, bip(q[2], 16.0f));
                bq[3].setHighShelf(srv, 12000.0f, bip(q[3], 16.0f), 0.6f);
                break;
            case 2:
                bq[0].setLowShelf(srv, 100.0f, bip(q[0], 15.0f), 0.7f);
                bq[1].setPeak(srv, expMap(q[4], 250.0f, 2500.0f), 1.3f, bip(q[1], 15.0f));
                bq[2].setPeak(srv, expMap(q[4], 1500.0f, 8000.0f), 1.3f, bip(q[2], 15.0f));
                bq[3].setHighShelf(srv, 8000.0f, bip(q[3], 15.0f), 0.7f);
                break;
            case 3:
                bq[0].setLowShelf(srv, 100.0f, bip(q[0], 12.0f), 0.8f);
                bq[1].setPeak(srv, expMap(q[4], 200.0f, 1000.0f), 1.0f + std::fabs(bip(q[1], 1.0f)) * 2.5f, bip(q[1], 12.0f));
                bq[2].setPeak(srv, expMap(q[4], 1500.0f, 5000.0f), 1.0f + std::fabs(bip(q[2], 1.0f)) * 2.5f, bip(q[2], 12.0f));
                bq[3].setHighShelf(srv, 10000.0f, bip(q[3], 12.0f), 0.8f);
                break;
            case 4:
                bq[0].setLowShelf(srv, expMap(q[4], 100.0f, 400.0f), bip(q[0], 15.0f), 0.5f);
                bq[1].setPeak(srv, 1000.0f, 1.0f, 0.0f); bq[2].setPeak(srv, 1000.0f, 1.0f, 0.0f);
                bq[3].setHighShelf(srv, expMap(q[4], 2000.0f, 6000.0f), bip(q[3], 15.0f), 0.5f);
                break;
            case 5:
            {
                const float f = expMap(q[4], 200.0f, 3000.0f), g = bip(q[0], 12.0f);
                bq[0].setLowShelf(srv, f, -g, 0.5f); bq[1].setPeak(srv, 1000.0f, 1.0f, 0.0f); bq[2].setPeak(srv, 1000.0f, 1.0f, 0.0f);
                bq[3].setHighShelf(srv, f, g, 0.5f);
                break;
            }
            case 6:
                bq[0].setHP(srv, expMap(q[1], 1500.0f, 9000.0f), 0.7071f);
                bq[1].setPeak(srv, 800.0f, 0.8f, q[3] * 4.0f);
                bq[2].setHighShelf(srv, 10000.0f, q[4] * 8.0f, 0.7f);
                bq[3].setLP(srv, expMap(1.0f - q[5], 3000.0f, 16000.0f), 0.7071f);
                useBands = false;
                break;
            case 7:
            {
                const float a = (q[0] - 0.5f) * 2.0f;
                bq[0].setLowShelf(srv, expMap(q[1], 60.0f, 200.0f), a * 9.0f, 0.6f);
                bq[1].setPeak(srv, 1000.0f, 0.7f, -std::fabs(a) * 4.0f * (q[3] * 2.0f) * (a < 0 ? -1.0f : 1.0f));
                bq[2].setPeak(srv, 3000.0f, 0.7f, 0.0f);
                bq[3].setHighShelf(srv, expMap(q[2], 5000.0f, 12000.0f), a * 7.0f, 0.6f);
                break;
            }
            case 8:
                bq[0].setHP(srv, expMap(q[0], 100.0f, 600.0f), 0.9f); bq[1].setLP(srv, expMap(q[1], 1500.0f, 7000.0f), 0.9f);
                bq[2].setPeak(srv, expMap(q[2], 600.0f, 2500.0f), 1.0f + q[3] * 4.0f, 3.0f + q[3] * 12.0f);
                bq[3].setPeak(srv, 3000.0f, 1.0f, 0.0f);
                break;
            default:
                bq[0].setHP(srv, linMap(q[0], 250.0f, 400.0f), 0.9f); bq[1].setLP(srv, linMap(q[1], 2800.0f, 3800.0f), 0.9f);
                bq[2].setPeak(srv, 1100.0f, 1.2f, q[2] * 10.0f); bq[3].setPeak(srv, 3000.0f, 1.0f, 0.0f);
                break;
        }
        for (int s = 0; s < 4; ++s) both(s);
        const bool radioLike = (var == 8 || var == 9);
        const bool exciter = (var == 6);
        const float airDrive = 1.0f + q[0] * 10.0f, blend = q[2];
        const float colAmt = (var <= 4 || var == 5) ? c5 : 0.0f;
        const float evenAmt = (var == 0) ? 0.8f : (var == 1 ? 0.4f : (var == 3 ? 0.3f : 0.0f));
        const float drv = radioLike ? (1.0f + (var == 8 ? q[4] : q[3]) * 6.0f) : 1.0f;
        if (exciter) { op[4].setFreq(expMap(1.0f - q[5], 2000.0f, 14000.0f), srv); }
        (void)useBands;
        for (int i = 0; i < n; ++i)
        {
            float a = l[i], b = r[i];
            if (exciter)
            {
                const float ha = bq[0].process(a), hb = bq[8].process(b);
                float xa = fastTanh(ha * airDrive) * 0.7f + std::fabs(ha) * 0.3f, xb = fastTanh(hb * airDrive) * 0.7f + std::fabs(hb) * 0.3f;
                xa = bq[3].process(xa); xb = bq[11].process(xb);
                xa = bq[2].process(xa); xb = bq[10].process(xb);
                a += xa * blend * 0.8f; b += xb * blend * 0.8f;
                a = bq[1].process(a); b = bq[9].process(b);
            }
            else
            {
                for (int s = 0; s < 4; ++s) { a = bq[s].process(a); b = bq[s + 8].process(b); }
            }
            if (colAmt > 0.0f) { a = colorSat(a, colAmt, evenAmt, op[0]); b = colorSat(b, colAmt, evenAmt, op[1]); }
            if (radioLike) { a = fastTanh(a * drv) / drv * 1.2f; b = fastTanh(b * drv) / drv * 1.2f; }
            if (var == 8 && q[5] > 0.01f) { lf[0].setRate(120.0f, srv); const float m = 1.0f - q[5] * 0.4f * (0.5f + 0.5f * lf[0].sine()); lf[0].advance(); a *= m; b *= m; }
            if (var == 9)
            {
                if (q[4] > 0.01f && rng.uni() < q[4] * 0.0006f) st[0] = q[4] * 0.4f * rng.bi();
                st[0] *= 0.996f; a += st[0]; b += st[0];
                if (q[5] > 0.01f) { lf[1].setRate(25.0f, srv); const float m = 1.0f - q[5] * 0.5f * (0.5f + 0.5f * lf[1].sq()); lf[1].advance(); a *= m; b *= m; }
            }
            l[i] = a; r[i] = b;
        }
    }
}
