#pragma once
// Modulation + Pitch + Spatial families (included by bg_machine.h)
namespace bg
{
    inline void Machine::procMod(float* l, float* r, int n)
    {
        const float srv = sr;
        const float toneF = expMap(q[2], 1500.0f, 16000.0f);
        auto modDelay = [&](int mode)   // BBD-style chorus family (0..3, 7)
        {
            float rate = expMap(q[0], 0.15f, 8.0f), depth = q[1], base = 0.0065f, dmax = 0.0035f, hiss = q[5] * 0.003f, fb = q[3] * 0.4f;
            if (var == 1)
            {
                const int mode = q[3] < 0.33f ? 0 : (q[3] < 0.66f ? 1 : 2);
                static const float rr[3] = { 0.513f, 0.863f, 9.75f }, dd[3] = { 0.0025f, 0.0035f, 0.0006f };
                rate = rr[mode] * lerpf(0.6f, 1.6f, q[0]); dmax = dd[mode]; fb = 0.0f; base = 0.0055f;
            }
            if (var == 3) { base = 0.008f; dmax = 0.0035f; fb = 0.0f; }
            if (var == 7) { rate = expMap(q[0], 2.0f, 9.0f); base = 0.004f; dmax = 0.0025f; fb = 0.0f; }
            op[0].setFreq(toneF, srv); op[1].setFreq(toneF, srv);
            lf[0].setRate(rate, srv); lf[1].setRate(var == 7 ? rate * 1.31f : 5.3f, srv);
            const float spread = var == 7 ? 0.0f : q[4] * 0.5f;
            for (int i = 0; i < n; ++i)
            {
                float wob = 0.0f;
                if (var == 7) wob = (0.6f * lf[1].sine() + 0.4f * rng.bi()) * q[3] * 0.0004f * srv;
                const float s0 = lf[0].sine(), s1 = lf[0].sine(0.5f + spread);
                float x[2] = { l[i], r[i] }, w[2];
                for (int c = 0; c < 2; ++c)
                {
                    float d;
                    if (var == 3)
                    {
                        const float p0 = lf[0].sine(0.0f), p1 = lf[0].sine(0.3333f + (c ? spread * 0.2f : 0.0f)), p2 = lf[0].sine(0.6667f);
                        const float dm = depth * dmax * srv;
                        d = 0.0f;
                        d += dl[c].read(base * srv + dm * p0); d += dl[c].read(base * srv * 1.4f + dm * p1); d += dl[c].read(base * srv * 1.9f + dm * p2);
                        d *= 0.5f;
                        dl[c].write(x[c]);
                        w[c] = op[c].lp(d) + hiss * rng.bi();
                        continue;
                    }
                    const float m = (c ? s1 : s0) * depth * dmax * srv + wob;
                    d = dl[c].read(base * srv + m);
                    dl[c].write(x[c] + fb * d);
                    if (var == 7) d = fastTanh(d * (1.0f + q[5] * 3.0f));
                    w[c] = op[c].lp(d) + hiss * rng.bi();
                }
                lf[0].advance(); lf[1].advance();
                l[i] = w[0]; r[i] = w[1];
            }
        };
        auto phaser = [&](int stages, bool uni)
        {
            const float rate = expMap(q[0], 0.05f, 8.0f), depth = q[1];
            const float fbk = (var == 5 ? q[2] : q[2] * 0.2f) * 0.92f;
            lf[0].setRate(rate, srv);
            for (int i = 0; i < n; ++i)
            {
                float x[2] = { l[i], r[i] }, w[2];
                for (int c = 0; c < 2; ++c)
                {
                    float lfo = uni ? lf[0].sine(c ? q[4] * 0.5f : 0.0f) : lf[0].tri(c ? q[4] * 0.5f : 0.0f);
                    if (uni) { op[c + 4].setTime(expMap(q[3], 0.002f, 0.2f), srv); lfo = op[c + 4].lp(lfo); }
                    const float fc = 250.0f * std::pow(2.0f, (lfo * 0.5f + 0.5f) * depth * 4.5f);
                    float y = x[c] + st[16 + c] * fbk;
                    for (int s = 0; s < stages; ++s)
                    {
                        const float f = uni ? fc * (1.0f + 0.9f * (float)s) : fc;
                        const float t = std::tan(kPi * clampf(f, 20.0f, srv * 0.45f) / srv), a = (t - 1.0f) / (t + 1.0f);
                        float& z = st[s + c * 8];
                        const float o = a * y + z; z = flushDenorm(y - a * o); y = o;
                    }
                    st[16 + c] = fastTanh(y);
                    w[c] = y;
                }
                lf[0].advance();
                if (uni && q[2] > 0.5f) { l[i] = w[0]; r[i] = w[1]; }       // vibrato wet-only
                else { l[i] = 0.5f * (x[0] + w[0]); r[i] = 0.5f * (x[1] + w[1]); }
            }
        };
        auto bandTrem = [&](float split, bool xover)
        {
            op[0].setFreq(split, srv); op[1].setFreq(split, srv);
            lf[0].setRate(expMap(q[0], 0.5f, 12.0f), srv);
            const float depth = q[1];
            for (int i = 0; i < n; ++i)
            {
                const float p = lf[0].sine(), tri = lf[0].tri(), sq = fastTanh(lf[0].sine() * 6.0f);
                const float shape = q[2] < 0.5f ? lerpf(p, tri, q[2] * 2.0f) : lerpf(tri, sq, (q[2] - 0.5f) * 2.0f);
                float x[2] = { l[i], r[i] };
                for (int c = 0; c < 2; ++c)
                {
                    const float sh = shape * (c ? (1.0f - 2.0f * q[4]) : 1.0f);
                    const float lo = op[c].lp(x[c]), hi = x[c] - lo;
                    float y;
                    if (xover) y = lo * (1.0f - depth * 0.5f * (1.0f + sh)) + hi * (1.0f - depth * 0.5f * (1.0f - sh));
                    else y = x[c] * (1.0f - depth * (0.5f + 0.5f * sh + (q[3] - 0.5f) * 0.4f));
                    if (var == 9 && q[5] > 0.01f) y = fastTanh(y * (1.0f + q[5] * 2.0f)) / (1.0f + q[5]);
                    x[c] = y;
                }
                lf[0].advance();
                l[i] = x[0]; r[i] = x[1];
            }
        };

        switch (var)
        {
            case 0: case 1: case 3: case 7: modDelay(0); break;
            case 2: // Dimension D: two cross-fed modulated delays per side
            {
                static const float dep[4] = { 0.15f, 0.3f, 0.55f, 0.8f }, rt[4] = { 0.25f, 0.4f, 0.55f, 0.8f };
                const int mode = std::min(3, (int)(q[0] * 3.99f));
                lf[0].setRate(rt[mode], srv); op[0].setFreq(toneF, srv); op[1].setFreq(toneF, srv);
                const float depth = dep[mode] * (0.4f + q[1]), wdt = q[3];
                for (int i = 0; i < n; ++i)
                {
                    const float s = lf[0].sine(); lf[0].advance();
                    const float m = (l[i] + r[i]) * 0.5f;
                    dl[0].write(m);
                    const float a = dl[0].read((0.0068f + 0.0028f * depth * s) * srv), b = dl[0].read((0.0068f - 0.0028f * depth * s) * srv);
                    const float c1 = op[0].lp(a), c2 = op[1].lp(b) + q[5] * 0.002f * rng.bi();
                    l[i] = l[i] * 0.7f + (c1 - c2 * (1.0f - wdt)) * 0.7f; r[i] = r[i] * 0.7f + (c2 - c1 * (1.0f - wdt)) * 0.7f;
                }
                break;
            }
            case 4: // through-zero flanger
            {
                const float rate = expMap(q[0], 0.03f, 5.0f), center = lerpf(0.0008f, 0.006f, q[2]) * srv, pol = q[5] < 0.5f ? -1.0f : 1.0f, fb = (q[3] - 0.5f) * 1.8f * 0.9f;
                lf[0].setRate(rate, srv);
                for (int i = 0; i < n; ++i)
                {
                    float x[2] = { l[i], r[i] };
                    const float ph[2] = { lf[0].sine(0.0f), lf[0].sine(q[4] * 0.5f) }; lf[0].advance();
                    for (int c = 0; c < 2; ++c)
                    {
                        const float wetD = center * (1.0f + q[1] * ph[c]);
                        dl[c].write(x[c] + fb * st[c]);
                        const float dry = dl[c].read(center), wet = dl[c].read(std::max(wetD, 2.0f));
                        st[c] = flushDenorm(wet);
                        x[c] = (dry + pol * wet) * 0.7f;
                    }
                    l[i] = x[0]; r[i] = x[1];
                }
                break;
            }
            case 5: phaser(q[3] < 0.33f ? 4 : (q[3] < 0.66f ? 6 : 8), false); break;
            case 6: phaser(4, true); break;
            case 8: bandTrem(800.0f, q[5] > 0.5f); break;
            case 9: bandTrem(expMap(q[3], 300.0f, 2000.0f), true); break;
            case 14: bandTrem(expMap(q[2], 250.0f, 2500.0f), true); break;
            case 10: // rotary speaker
            {
                const float target = expMap(q[0], 0.4f, 7.0f), ramp = expMap(q[3], 0.3f, 6.0f);
                op[0].setFreq(800.0f, srv); op[1].setFreq(800.0f, srv);
                const float coef = 1.0f - std::exp(-1.0f / (ramp * srv)), drv = 1.0f + q[5] * 3.0f;
                for (int i = 0; i < n; ++i)
                {
                    st[0] += (target - st[0]) * coef; if (st[0] < 0.1f) st[0] = target;
                    st[1] += st[0] / srv; if (st[1] >= 1.0f) st[1] -= 1.0f;
                    st[2] += st[0] * 0.88f / srv; if (st[2] >= 1.0f) st[2] -= 1.0f;
                    const float m = fastTanh((l[i] + r[i]) * 0.5f * drv) / std::sqrt(drv);
                    const float lo = op[0].lp(m), hi = m - lo;
                    dl[0].write(hi);
                    float oL = 0, oR = 0;
                    for (int c = 0; c < 2; ++c)
                    {
                        const float ph = st[1] + (c ? q[4] * 0.5f : 0.0f), pd = st[2] + (c ? q[4] * 0.5f : 0.0f);
                        const float s = std::sin(kTwoPi * ph), cs = std::cos(kTwoPi * ph);
                        const float horn = dl[0].read((0.0006f + 0.0004f * s) * srv) * (1.0f - q[1] * 0.5f * (1.0f - cs));
                        const float drum = lo * (1.0f - q[2] * 0.4f * (1.0f - std::cos(kTwoPi * pd)));
                        (c ? oR : oL) = horn + drum;
                    }
                    l[i] = oL; r[i] = oR;
                }
                break;
            }
            case 11: // ring mod with optional zero-crossing pitch tracking
            {
                op[0].setFreq(1200.0f, srv); op[1].setFreq(toneF, srv); op[2].setFreq(toneF, srv);
                const float fixedF = expMap(q[0], 20.0f, 4000.0f);
                for (int i = 0; i < n; ++i)
                {
                    const float m = op[0].lp((l[i] + r[i]) * 0.5f);
                    st[5] += 1.0f;
                    if (st[4] <= 0.0f && m > 0.002f) { if (st[5] > 8.0f) { const float f = srv / st[5]; if (f > 50.0f && f < 1500.0f) st[6] += (f - st[6]) * 0.3f; } st[5] = 0.0f; }
                    st[4] = m > 0.002f ? 1.0f : (m < -0.002f ? -1.0f : st[4]); if (m < -0.002f) st[4] = 0.0f;
                    const float cf = lerpf(fixedF, std::max(st[6], 40.0f) * (0.5f + q[0] * 3.0f), q[3]);
                    float outv[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        st[8 + c] += cf * (c ? 1.0f + q[4] * 0.05f : 1.0f) / srv; if (st[8 + c] >= 1.0f) st[8 + c] -= 1.0f;
                        const float ph = st[8 + c];
                        const float car = q[5] < 0.5f ? std::sin(kTwoPi * ph) : (q[5] < 0.8f ? (ph < 0.5f ? 1.0f : -1.0f) : 4.0f * std::fabs(ph - 0.5f) - 1.0f);
                        const float x = c ? r[i] : l[i];
                        outv[c] = lerpf(x, x * car, q[1]);
                    }
                    l[i] = op[1].lp(outv[0]); r[i] = op[2].lp(outv[1]);
                }
                break;
            }
            case 12: // Bode-style frequency shifter
            {
                const float sgn = q[0] < 0.5f ? -1.0f : 1.0f, hz = sgn * expMap(std::fabs(q[0] - 0.5f) * 2.0f, 1.0f, 1000.0f) + (q[1] - 0.5f) * 40.0f;
                const float fb = q[2] * 0.7f;
                op[0].setFreq(expMap(q[5], 1500.0f, 18000.0f), srv); op[1].setFreq(expMap(q[5], 1500.0f, 18000.0f), srv);
                for (int i = 0; i < n; ++i)
                {
                    float x[2] = { l[i], r[i] };
                    for (int c = 0; c < 2; ++c)
                    {
                        float re, im; hil[c].process(x[c] + fb * st[c], re, im);
                        st[10 + c] += hz * (c ? 1.0f + q[4] * 0.2f : 1.0f) / srv; if (st[10 + c] > 1.0f) st[10 + c] -= 1.0f; if (st[10 + c] < -1.0f) st[10 + c] += 1.0f;
                        const float ph = kTwoPi * st[10 + c], cs = std::cos(ph), sn = std::sin(ph);
                        const float up = re * cs - im * sn, dn = re * cs + im * sn;
                        const float y = lerpf(dn, up, q[3]);
                        st[c] = fastTanh(flushDenorm(y));
                        x[c] = op[c].lp(y);
                    }
                    l[i] = x[0]; r[i] = x[1];
                }
                break;
            }
            default: // 13 auto-pan orbit
            {
                lf[0].setRate(expMap(q[0], 0.05f, 10.0f), srv); op[0].setFreq(expMap(q[5], 1000.0f, 18000.0f), srv); op[1].setFreq(expMap(q[5], 1000.0f, 18000.0f), srv);
                const int hd = (int)(q[4] * 0.012f * srv);
                for (int i = 0; i < n; ++i)
                {
                    const float s = lf[0].sine(), t = lf[0].tri(), sq = fastTanh(s * 8.0f);
                    const float sh = q[2] < 0.5f ? lerpf(s, t, q[2] * 2.0f) : lerpf(t, sq, (q[2] - 0.5f) * 2.0f);
                    lf[0].advance();
                    const float m = (l[i] + r[i]) * 0.5f, sd = (l[i] - r[i]) * 0.5f;
                    dl[0].write(m);
                    const float mm = hd > 1 ? dl[0].tap(hd) : m;
                    const float pan = sh * q[1] * 0.5f + 0.5f;
                    const float gl = std::cos(pan * kPi * 0.5f) * 1.4142f, gr = std::sin(pan * kPi * 0.5f) * 1.4142f;
                    l[i] = op[0].lp(m * gl) + sd * 0.5f; r[i] = op[1].lp(mm * gr) - sd * 0.5f;
                }
                break;
            }
        }
    }

    inline void Machine::procPit(float* l, float* r, int n)
    {
        const float srv = sr;
        const float toneF = expMap(q[5], 2000.0f, 18000.0f);
        op[0].setFreq(toneF, srv); op[1].setFreq(toneF, srv);
        auto semi = [](float s) { return std::pow(2.0f, s / 12.0f); };
        const float win = srv * lerpf(0.02f, 0.07f, q[2]);
        for (int k = 0; k < 4; ++k) ps[k].setWindow(win);

        switch (var)
        {
            case 0:
            {
                const float ratio = (q[0] < 0.5f ? 0.5f : 0.25f) * semi(q[3] * 0.5f);
                ps[0].ratio = ps[1].ratio = ratio; for (int k = 0; k < 2; ++k) ps[k].setWindow(srv * lerpf(0.03f, 0.08f, q[1]));
                for (int i = 0; i < n; ++i) { l[i] = op[0].lp(ps[0].process(l[i])) * (0.6f + q[4]); r[i] = op[1].lp(ps[1].process(r[i])) * (0.6f + q[4]); }
                break;
            }
            case 1:
            {
                const float s = std::round(lerpf(-12.0f, 12.0f, q[0])) + (q[1] - 0.5f) * 1.0f;
                ps[0].ratio = semi(s); ps[1].ratio = semi(s + q[4] * 0.3f);
                for (int i = 0; i < n; ++i)
                {
                    const float a = ps[0].process(l[i] + st[0] * q[3] * 0.7f), b = ps[1].process(r[i] + st[1] * q[3] * 0.7f);
                    st[0] = flushDenorm(a); st[1] = flushDenorm(b);
                    l[i] = op[0].lp(a); r[i] = op[1].lp(b);
                }
                break;
            }
            case 2:
            {
                const float c = q[0] * 0.5f;     // up to 50 cents
                ps[0].ratio = std::pow(2.0f, -c / 12.0f * 0.5f * 2.0f / 1.0f * 0.5f); ps[1].ratio = std::pow(2.0f, c / 12.0f * 0.5f);
                const float dmax = (float)dl[0].size - 8.0f, dsamp = std::min(q[1] * 0.04f * srv, dmax);
                for (int i = 0; i < n; ++i)
                {
                    const float a = ps[0].process(l[i] + st[0] * q[4] * 0.5f), b = ps[1].process(r[i] + st[1] * q[4] * 0.5f);
                    st[0] = flushDenorm(a); st[1] = flushDenorm(b);
                    dl[0].write(a); dl[1].write(b);
                    const float da = dsamp > 2 ? dl[0].read(dsamp) : a, db = dsamp > 2 ? dl[1].read(dsamp * 1.07f) : b;
                    const float w = q[3];
                    l[i] = op[0].lp(da * (0.5f + 0.5f * w) + db * (0.5f - 0.5f * w)); r[i] = op[1].lp(db * (0.5f + 0.5f * w) + da * (0.5f - 0.5f * w));
                }
                break;
            }
            case 3:
            {
                lf[0].setRate(expMap(q[3], 0.1f, 8.0f), srv);
                const float range = q[0] < 0.5f ? 12.0f : 24.0f;
                for (int i = 0; i < n; ++i)
                {
                    const float s = (q[1] - 0.5f) * 2.0f * range + lf[0].sine() * q[4] * range; lf[0].advance();
                    st[0] += (s - st[0]) * 0.002f;
                    ps[0].ratio = ps[1].ratio = semi(st[0]);
                    l[i] = op[0].lp(ps[0].process(l[i])); r[i] = op[1].lp(ps[1].process(r[i]));
                }
                break;
            }
            case 4: // pitch snap: AMDF/YIN-lite detector on decimated signal + scale quantiser
            {
                static const int sc[4][12] = { { 1,1,1,1,1,1,1,1,1,1,1,1 }, { 1,0,1,0,1,1,0,1,0,1,0,1 }, { 1,0,1,1,0,1,0,1,1,0,1,0 }, { 1,0,1,0,1,0,0,1,0,1,0,0 } };
                const int scale = std::min(3, (int)(q[1] * 3.99f)), key = (int)(q[0] * 11.99f);
                const float speed = expMap(1.0f - q[2], 0.002f, 0.2f), amount = q[3];
                const float smooth = 1.0f - std::exp(-1.0f / (speed * srv));
                for (int k = 0; k < 2; ++k) ps[k].setWindow(srv * lerpf(0.02f, 0.05f, q[4]));
                op[2].setFreq(1500.0f, srv);
                static const int N = 384;           // decimated history length (x4 decimation)
                float* hist = dl[0].buf;             // reuse arena memory as history ring
                for (int i = 0; i < n; ++i)
                {
                    const float m = op[2].lp((l[i] + r[i]) * 0.5f);
                    if (++st[1] >= 4.0f) { st[1] = 0.0f; dl[0].write(m); st[2] += 1.0f; }
                    if (st[2] >= 128.0f)
                    {
                        st[2] = 0.0f;
                        const float dsr = srv / 4.0f; const int minLag = (int)(dsr / 800.0f), maxLag = std::min(N / 2 - 2, (int)(dsr / 70.0f));
                        const int W = N / 2; float best = 1e9f; int bl = 0; float energy = 0;
                        auto H = [&](int k) { return hist[(dl[0].w - 1 - k + dl[0].size * 4) % dl[0].size]; };
                        for (int k = 0; k < W; ++k) energy += H(k) * H(k);
                        for (int lag = minLag; lag <= maxLag; ++lag)
                        {
                            float d = 0; for (int k = 0; k < W; k += 2) { const float df = H(k) - H(k + lag); d += df * df; }
                            if (d < best) { best = d; bl = lag; }
                        }
                        const bool voiced = energy / (float)W > 1e-6f && best < 0.35f * energy * 0.5f && bl > 0;
                        if (voiced)
                        {
                            const float f = dsr / (float)bl, midi = 69.0f + 12.0f * std::log2(f / 440.0f);
                            int bestN = (int)std::round(midi); float bd = 1e9f;
                            for (int dn = -6; dn <= 6; ++dn) { const int nn = (int)std::round(midi) + dn; if (sc[scale][((nn - key) % 12 + 12) % 12] && std::fabs((float)nn - midi) < bd) { bd = std::fabs((float)nn - midi); bestN = nn; } }
                            st[3] = std::pow(2.0f, ((float)bestN - midi) / 12.0f); st[4] = 1.0f; meter.store(f / 1000.0f);
                        }
                        else st[3] = 1.0f;
                    }
                    const float want = lerpf(1.0f, st[3] > 0 ? st[3] : 1.0f, amount);
                    st[5] += (want - (st[5] > 0 ? st[5] : 1.0f)) * smooth; if (st[5] <= 0.0f) st[5] = 1.0f;
                    ps[0].ratio = ps[1].ratio = st[5];
                    l[i] = op[0].lp(ps[0].process(l[i])); r[i] = op[1].lp(ps[1].process(r[i]));
                }
                break;
            }
            default: // 5 cascade detune "supersaw" thickener
            {
                const int voices = q[1] < 0.5f ? 2 : 4;
                lf[0].setRate(0.13f, srv); lf[1].setRate(0.21f, srv);
                const float sp = q[0] * 40.0f;
                for (int i = 0; i < n; ++i)
                {
                    const float dr = lf[0].sine() * q[4] * 8.0f, dr2 = lf[1].sine() * q[4] * 8.0f; lf[0].advance(); lf[1].advance();
                    ps[0].ratio = std::pow(2.0f, (-sp + dr) / 1200.0f); ps[1].ratio = std::pow(2.0f, (sp * 0.5f + dr2) / 1200.0f);
                    ps[2].ratio = std::pow(2.0f, (sp - dr) / 1200.0f); ps[3].ratio = std::pow(2.0f, (-sp * 0.5f - dr2) / 1200.0f);
                    const float m = (l[i] + r[i]) * 0.5f;
                    const float a = ps[0].process(m), b = ps[1].process(m);
                    float c = 0, d = 0; if (voices == 4) { c = ps[2].process(m); d = ps[3].process(m); }
                    const float w = q[3], nrm = voices == 4 ? 0.5f : 0.7f;
                    const float oL = (a + c * 0.5f + b * (1.0f - w)) * nrm, oR = (b + d * 0.5f + a * (1.0f - w)) * nrm;
                    l[i] = op[0].lp(oL); r[i] = op[1].lp(oR);
                }
                break;
            }
        }
    }

    inline void Machine::procSpa(float* l, float* r, int n)
    {
        const float srv = sr;
        switch (var)
        {
            case 0:
            {
                const float w = q[0] * 2.0f, mg = dbToGain(linMap(q[1], -12.0f, 12.0f)), sg = dbToGain(linMap(q[2], -12.0f, 12.0f));
                const float bal = q[5] * 2.0f - 1.0f, gl = bal > 0 ? 1.0f - bal : 1.0f, gr = bal < 0 ? 1.0f + bal : 1.0f;
                op[0].setFreq(expMap(q[3], 40.0f, 300.0f), srv); op[1].setFreq(expMap(q[4], 20.0f, 600.0f), srv);
                for (int i = 0; i < n; ++i)
                {
                    const float m = (l[i] + r[i]) * 0.5f; float s = (l[i] - r[i]) * 0.5f;
                    if (q[3] > 0.02f) s = s - op[0].lp(s);          // bass mono: remove low side content
                    if (q[4] > 0.02f) s = op[1].hp(s);
                    s *= w * sg;
                    l[i] = (m * mg + s) * gl; r[i] = (m * mg - s) * gr;
                }
                break;
            }
            case 1:
            {
                const int d = std::max(1, (int)(q[0] * 0.03f * srv)); op[0].setFreq(expMap(q[2], 1500.0f, 16000.0f), srv);
                lf[0].setRate(0.7f, srv);
                const float bal = q[5] * 2.0f - 1.0f;
                for (int i = 0; i < n; ++i)
                {
                    const float m = (l[i] + r[i]) * 0.5f;
                    dl[0].write(m);
                    const float dd = op[0].lp(dl[0].read((float)d + lf[0].sine() * q[4] * 0.002f * srv)); lf[0].advance();
                    const float s = (l[i] - r[i]) * 0.5f;
                    const float hl = lerpf(l[i], m + s * (1.0f - q[3]), q[1]), hr = lerpf(r[i], dd + (-s) * (1.0f - q[3]), q[1]);
                    l[i] = hl * (bal > 0 ? 1.0f - bal : 1.0f); r[i] = hr * (bal < 0 ? 1.0f + bal : 1.0f);
                }
                break;
            }
            case 2:
            {
                static const float PI2 = kTwoPi;
                lf[0].setRate(expMap(q[3], 0.05f, 2.0f), srv);
                const float dist = 1.0f / (1.0f + q[1] * 3.0f), shadow = q[2], ele = q[4];
                bq[0].setNotch(srv, 5500.0f + ele * 4000.0f, 3.0f); bq[1] = bq[0]; bq[1].reset();
                for (int i = 0; i < n; ++i)
                {
                    float th = (q[0] - 0.5f) * PI2 + (q[3] > 0.01f ? lf[0].ph * PI2 : 0.0f); lf[0].advance();
                    const float sn = std::sin(th), itd = 0.00066f * srv * std::fabs(sn);
                    const float m = bq[0].process((l[i] + r[i]) * 0.5f) * dist;
                    dl[0].write(m); dl[1].write(m);
                    const bool right = sn > 0;    // source on right: left ear is far
                    float a = dl[0].read(2.0f + (right ? itd : 0.0f)), b = dl[1].read(2.0f + (right ? 0.0f : itd));
                    const float fc = expMap(1.0f - shadow * std::fabs(sn), 1200.0f, 20000.0f);
                    op[right ? 0 : 1].setFreq(fc, srv); op[right ? 1 : 0].setFreq(20000.0f, srv);
                    a = op[0].lp(a) * (right ? 1.0f - 0.4f * shadow * std::fabs(sn) : 1.0f); b = op[1].lp(b) * (right ? 1.0f : 1.0f - 0.4f * shadow * std::fabs(sn));
                    const float room = dl[0].tap((int)(0.011f * srv)) * q[5] * 0.3f;
                    l[i] = a + room; r[i] = b - room;
                }
                break;
            }
            case 3:
            {
                const float f = expMap(q[0], 40.0f, 400.0f), w = q[2] * 2.0f;
                sv[0].setup(srv, f, 0.7071f); sv[1].setup(srv, f, 0.7071f); sv[2].setup(srv, f, 0.7071f); sv[3].setup(srv, f, 0.7071f);
                const bool steep = q[1] > 0.5f;
                for (int i = 0; i < n; ++i)
                {
                    float la = sv[0].process(l[i]), ra = sv[1].process(r[i]);
                    if (steep) { la = sv[2].process(la); ra = sv[3].process(ra); }
                    const float lowM = (la + ra) * 0.5f * (1.0f + q[3] * 0.3f);
                    const float hl = l[i] - la, hr = r[i] - ra, m = (hl + hr) * 0.5f, s = (hl - hr) * 0.5f * w;
                    l[i] = lowM + m + s; r[i] = lowM + m - s;
                }
                break;
            }
            case 4:
            {
                const float g1 = lerpf(0.2f, 0.7f, q[2]), g2 = lerpf(0.3f, 0.75f, q[2]);
                for (int k = 0; k < 4; ++k) apLen[k] = std::max(8, std::min(639, (int)((float)(k == 0 ? 113 : k == 1 ? 59 : k == 2 ? 97 : 71) * srv / 48000.0f)));
                op[0].setFreq(expMap(q[4], 2000.0f, 18000.0f), srv); op[1].setFreq(expMap(q[4], 2000.0f, 18000.0f), srv);
                const int stg = q[1] < 0.5f ? 1 : 2;
                for (int i = 0; i < n; ++i)
                {
                    float a = l[i], b = r[i];
                    a = apProc(0, a, g1 * (1.0f + q[3] * 0.2f * rng.bi())); if (stg == 2) a = apProc(1, a, g2);
                    b = apProc(2, b, -g1); if (stg == 2) b = apProc(3, b, -g2);
                    const float dl_ = lerpf(l[i], op[0].lp(a), q[0]), dr_ = lerpf(r[i], op[1].lp(b), q[0]);
                    const float m = (l[i] + r[i]) * 0.5f;
                    l[i] = lerpf(dl_, m + (dl_ - m) * 0.5f, q[5] * 0.5f); r[i] = lerpf(dr_, m + (dr_ - m) * 0.5f, q[5] * 0.5f);
                }
                break;
            }
            case 5:
            {
                const int d = std::max(1, (int)((0.0002f + q[1] * 0.0004f) * srv)); op[0].setFreq(expMap(q[2], 500.0f, 2200.0f), srv); op[1].setFreq(expMap(q[2], 500.0f, 2200.0f), srv);
                const float feed = q[0] * 0.5f, g = 1.0f / (1.0f + feed * 0.5f) * dbToGain(linMap(q[3], -6.0f, 6.0f));
                for (int i = 0; i < n; ++i)
                {
                    dl[0].write(l[i]); dl[1].write(r[i]);
                    const float fl = op[0].lp(dl[1].tap(d)), fr = op[1].lp(dl[0].tap(d));
                    l[i] = (l[i] + feed * fl) * g; r[i] = (r[i] + feed * fr) * g;
                }
                break;
            }
            case 6:
            {
                lf[0].setRate(expMap(q[0], 0.05f, 4.0f), srv); op[0].setFreq(18000.0f, srv); op[1].setFreq(18000.0f, srv);
                for (int i = 0; i < n; ++i)
                {
                    const float ph = lf[0].ph * kTwoPi; lf[0].advance();
                    const float s = std::sin(ph), c = std::cos(ph);
                    const float m = (l[i] + r[i]) * 0.5f; dl[0].write(m);
                    const float x = dl[0].read(2.0f + (c * 0.5f + 0.5f) * q[3] * 0.003f * srv);
                    const float pan = s * q[1] * 0.5f + 0.5f, dist = 1.0f - q[4] * 0.5f * (0.5f + 0.5f * c);
                    const float fc = expMap(clampf(0.5f + (c * 0.5f - 0.0f) * (q[2] - 0.5f) + (q[5] - 0.5f) * 0.4f, 0.0f, 1.0f), 2500.0f, 20000.0f);
                    op[0].setFreq(fc, srv); op[1].setFreq(fc, srv);
                    l[i] = op[0].lp(x * std::cos(pan * kPi * 0.5f) * 1.4142f * dist); r[i] = op[1].lp(x * std::sin(pan * kPi * 0.5f) * 1.4142f * dist);
                }
                break;
            }
            default:
            {
                const float tl = std::max(2.0f, expMap(q[0], 0.002f, 0.09f) * srv), trr = std::max(2.0f, expMap(q[1], 0.002f, 0.09f) * srv), fb = q[2] * 0.7f, cx = q[3];
                op[0].setFreq(expMap(q[4], 1500.0f, 16000.0f), srv); op[1].setFreq(expMap(q[4], 1500.0f, 16000.0f), srv);
                const float w = q[5] * 2.0f;
                for (int i = 0; i < n; ++i)
                {
                    const float a = dl[0].read(tl), b = dl[1].read(trr);
                    dl[0].write(l[i] + fb * op[0].lp(a * (1.0f - cx) + b * cx)); dl[1].write(r[i] + fb * op[1].lp(b * (1.0f - cx) + a * cx));
                    const float m = (a + b) * 0.5f, s = (a - b) * 0.5f * w;
                    l[i] = l[i] * 0.5f + m + s; r[i] = r[i] * 0.5f + m - s;
                }
                break;
            }
        }
    }
}
