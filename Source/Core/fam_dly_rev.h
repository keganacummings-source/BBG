#pragma once
// Delay + Reverb families (included by bg_machine.h)
namespace bg
{
    inline void Machine::procDly(float* l, float* r, int n)
    {
        const float srv = sr;
        const float maxD = (float)dl[0].size - 8.0f;
        const float toneF = expMap(q[2], 600.0f, 12000.0f);
        op[0].setFreq(toneF, srv); op[1].setFreq(toneF, srv);
        op[2].setFreq(90.0f, srv); op[3].setFreq(90.0f, srv);
        const float drive = 1.0f + q[5] * 3.0f;
        auto loopFilter = [&](int c, float x) { x = op[c].lp(x); x = op[c + 2].hp(x); return drive > 1.001f ? fastTanh(x * drive) / drive : x; };
        auto smoothT = [&](int slot, float target) { st[slot] += (target - st[slot]) * 0.002f; if (st[slot] < 1.0f) st[slot] = target; return st[slot]; };

        switch (var)
        {
            case 0: // RE-201 three heads
            {
                const float base = expMap(q[0], 0.04f, 0.5f) * srv, fb = q[1] * 1.05f;
                const int mode = std::min(6, (int)(q[4] * 6.99f));
                static const float W[7][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 0 }, { 0, 1, 1 }, { 1, 0, 1 }, { 1, 1, 1 } };
                const float depth = q[3] * srv * 0.0012f;
                lf[0].setRate(0.7f, srv); lf[1].setRate(6.5f, srv);
                for (int i = 0; i < n; ++i)
                {
                    const float wob = (lf[0].sine() * 0.7f + lf[1].sine() * 0.3f + 0.15f * rng.bi()) * depth; lf[0].advance(); lf[1].advance();
                    const float b = smoothT(0, base);
                    float x[2] = { l[i], r[i] }, w[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        const float cb = b * (c ? 1.013f : 1.0f);
                        float h = 0, sum = 0;
                        for (int k = 0; k < 3; ++k) if (W[mode][k] > 0) { const float t = dl[c].read(std::min(cb * (float)(k + 1) + wob, maxD)); h += t; sum += 1; }
                        h = sum > 0 ? h / std::sqrt(sum) : 0.0f;
                        dl[c].write(x[c] + fb * loopFilter(c, h));
                        w[c] = h * 0.9f;
                    }
                    l[i] = w[0]; r[i] = w[1];
                }
                return;
            }
            case 1: case 7: // BBD / oil can
            {
                const float tmin = var == 1 ? 0.005f : 0.03f, tmax = var == 1 ? 0.6f : 0.5f;
                const float base = expMap(q[0], tmin, tmax) * srv, fb = q[1] * 1.0f;
                const float bw = clampf(8000.0f * std::sqrt(0.05f / (base / srv + 1e-4f)), 1500.0f, 9000.0f) * (var == 7 ? lerpf(1.0f, 0.4f, q[4]) : 1.0f);
                bq[0].setLP(srv, bw, 0.8f); bq[1] = bq[0]; bq[1].reset(); bq[4].setLP(srv, bw, 0.9f); bq[5] = bq[4]; bq[5].reset();
                const float depth = q[3] * srv * (var == 1 ? 0.002f : 0.004f);
                lf[0].setRate(var == 1 ? 0.45f : 6.0f, srv);
                const float hiss = (var == 1 ? q[4] : 0.15f) * 0.004f;
                for (int i = 0; i < n; ++i)
                {
                    const float m = lf[0].sine() * depth; lf[0].advance();
                    const float b = smoothT(0, base);
                    float x[2] = { l[i], r[i] }, w[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        float d = dl[c].read(std::min(b * (c ? 1.02f : 1.0f) + m, maxD));
                        d = bq[c].process(bq[c + 4].process(d)) + hiss * rng.bi();
                        dl[c].write(fastTanh((x[c] + fb * loopFilter(c, d)) * 0.9f) * 1.1f);
                        w[c] = d;
                    }
                    l[i] = w[0]; r[i] = w[1];
                }
                return;
            }
            case 2: // clean digital
            {
                const float base = expMap(q[0], 0.02f, 1.8f) * srv, fb = q[1] * 0.97f, depth = q[3] * srv * 0.003f;
                lf[0].setRate(0.6f, srv);
                for (int i = 0; i < n; ++i)
                {
                    const float m = lf[0].sine() * depth; lf[0].advance(); const float b = smoothT(0, base);
                    float x[2] = { l[i], r[i] }, w[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        const float d = dl[c].read(std::min(b * (c ? 1.0f + q[4] * 0.5f : 1.0f) + m, maxD));
                        dl[c].write(x[c] + fb * loopFilter(c, d)); w[c] = d;
                    }
                    l[i] = w[0]; r[i] = w[1];
                }
                return;
            }
            case 3: // ping-pong
            {
                const float base = expMap(q[0], 0.03f, 1.2f) * srv, fb = q[1] * 0.97f, depth = q[3] * srv * 0.002f;
                lf[0].setRate(0.5f, srv);
                for (int i = 0; i < n; ++i)
                {
                    const float m = lf[0].sine() * depth; lf[0].advance(); const float b = smoothT(0, base);
                    const float wl = dl[0].read(std::min(b + m, maxD)), wr = dl[1].read(std::min(b * (0.5f + q[4]) + m, maxD));
                    dl[0].write(0.5f * (l[i] + r[i]) + fb * loopFilter(0, wr));
                    dl[1].write(fb * loopFilter(1, wl));
                    l[i] = wl; r[i] = wr;
                }
                return;
            }
            case 4: case 5: // golden-ratio / fibonacci multitap
            {
                static const float G[5] = { 1.0f, 0.618034f, 0.381966f, 0.236068f, 0.145898f };
                static const float Fb[5] = { 8.0f / 8.0f, 5.0f / 8.0f, 3.0f / 8.0f, 2.0f / 8.0f, 1.0f / 8.0f };
                const float* T = var == 4 ? G : Fb;
                const float base = expMap(q[0], 0.05f, 1.6f) * srv, fb = q[1] * 0.9f, dec = 0.55f + q[4] * 0.4f, depth = q[3] * srv * 0.002f;
                lf[0].setRate(0.4f, srv);
                for (int i = 0; i < n; ++i)
                {
                    const float m = lf[0].sine() * depth; lf[0].advance(); const float b = smoothT(0, base);
                    float x[2] = { l[i], r[i] }, w[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        float acc = 0, a = 1.0f, tap0 = 0;
                        for (int k = 0; k < 5; ++k)
                        {
                            const int kk = c ? 4 - k : k;
                            const float t = dl[c].read(std::min(b * T[k] + m, maxD));
                            if (k == 0) tap0 = t;
                            acc += t * a * (((kk & 1) == 0) ? 1.0f : 0.55f); a *= dec;
                        }
                        dl[c].write(x[c] + fb * loopFilter(c, tap0));
                        w[c] = acc * 0.6f;
                    }
                    l[i] = w[0]; r[i] = w[1];
                }
                return;
            }
            case 6: // deep-sea dub
            {
                const float base = expMap(q[0], 0.06f, 1.4f) * srv, fb = q[1] * 1.0f + q[4] * 0.2f;
                lf[0].setRate(0.12f, srv);
                for (int i = 0; i < n; ++i)
                {
                    const float fc = expMap(q[2], 300.0f, 6000.0f) * std::pow(2.0f, lf[0].sine() * q[3] * 1.5f); lf[0].advance();
                    if ((i & 15) == 0) { op[0].setFreq(fc, srv); op[1].setFreq(fc, srv); }
                    const float b = smoothT(0, base);
                    float x[2] = { l[i], r[i] }, w[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        const float d = dl[c].read(std::min(b * (c ? 1.04f : 1.0f), maxD));
                        float f = op[c].lp(d); f = op[c + 2].hp(f); f = fastTanh(f * (1.0f + q[5] * 3.0f)) / (1.0f + q[5] * 1.5f);
                        dl[c].write(x[c] + fb * f); w[c] = d;
                    }
                    l[i] = w[0]; r[i] = w[1];
                }
                return;
            }
            case 8: // Echorec 4 heads
            {
                const float base = expMap(q[0], 0.03f, 0.4f) * srv, fb = q[1] * 1.0f;
                const int heads = std::max(1, (int)(q[4] * 15.0f));
                const float depth = q[3] * srv * 0.0012f;
                lf[0].setRate(0.9f, srv);
                for (int i = 0; i < n; ++i)
                {
                    const float wob = lf[0].sine() * depth; lf[0].advance(); const float b = smoothT(0, base);
                    float x[2] = { l[i], r[i] }, w[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        float acc = 0; int cnt = 0;
                        for (int k = 0; k < 4; ++k) if (heads & (1 << k)) { acc += dl[c].read(std::min(b * (float)(k + 1) * (c ? 1.01f : 1.0f) + wob, maxD)); ++cnt; }
                        acc = cnt ? acc / std::sqrt((float)cnt) : 0.0f;
                        const float s = q[5];
                        dl[c].write(fastTanh((x[c] + fb * loopFilter(c, acc)) * (1.0f + s)) / (1.0f + s * 0.5f));
                        w[c] = acc;
                    }
                    l[i] = w[0]; r[i] = w[1];
                }
                return;
            }
            case 9: // reverse delay (two-tap, ratio -1 crossfaded grains)
            {
                const float T = std::min(expMap(q[0], 0.06f, 1.5f) * srv, maxD * 0.9f), fb = q[1] * 0.9f;
                for (int i = 0; i < n; ++i)
                {
                    st[0] += 2.0f / T; if (st[0] >= 1.0f) st[0] -= 1.0f;
                    const float p1 = st[0], p2 = p1 + 0.5f - std::floor(p1 + 0.5f);
                    const float g1 = std::sin(kPi * p1), g2 = std::sin(kPi * p2);
                    float x[2] = { l[i], r[i] }, w[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        const float rev = (dl[c].readLin(p1 * T + 2.0f) * g1 * g1 + dl[c].readLin(p2 * T + 2.0f) * g2 * g2);
                        const float fwd = dl[c].readLin(T) * q[4];
                        dl[c].write(x[c] + fb * loopFilter(c, rev));
                        w[c] = rev + fwd;
                    }
                    l[i] = w[0]; r[i] = w[1];
                }
                return;
            }
            case 10: case 12: // freeze looper / beat repeater (loop engine over the delay line)
            {
                const bool hold = (var == 10) ? (q[4] > 0.5f) : (q[4] > 0.5f);
                const float T = std::min(expMap(q[0], var == 10 ? 0.08f : 0.02f, var == 10 ? 1.8f : 0.5f) * srv, maxD * 0.9f);
                const float fb = q[1] * 0.97f, depth = q[3] * srv * 0.002f;
                lf[0].setRate(0.5f, srv);
                if (!hold) { st[1] = 0.0f; st[2] = 0.0f; }
                for (int i = 0; i < n; ++i)
                {
                    float x[2] = { l[i], r[i] }, w[2];
                    if (hold)
                    {
                        st[1] += 1.0f;                                   // seconds of hold in samples
                        const float speed = (var == 12) ? std::max(0.25f, 1.0f - q[3] * std::min(st[1] / (srv * 2.0f), 1.0f)) : 1.0f;
                        st[2] += speed / T; if (st[2] >= 1.0f) st[2] -= 1.0f;
                        const float p1 = st[2], p2 = p1 + 0.5f - std::floor(p1 + 0.5f), g1 = std::sin(kPi * p1), g2 = std::sin(kPi * p2);
                        for (int c = 0; c < 2; ++c)
                        {
                            const float loop = dl[c].readLin(T - p1 * T + 2.0f) * g1 * g1 + dl[c].readLin(T - p2 * T + 2.0f) * g2 * g2;
                            w[c] = loop * (var == 12 ? std::pow(0.5f + q[1] * 0.5f, st[1] / (T * 2.0f)) : 1.0f);   // repeats decay
                        }
                    }
                    else
                    {
                        const float m = lf[0].sine() * depth; lf[0].advance();
                        for (int c = 0; c < 2; ++c)
                        {
                            const float d = dl[c].read(std::min(T + m, maxD));
                            dl[c].write(x[c] + fb * loopFilter(c, d));
                            w[c] = (var == 12) ? x[c] : d;
                        }
                    }
                    l[i] = w[0]; r[i] = w[1];
                }
                meter.store(hold ? 1.0f : 0.0f);
                return;
            }
            case 11: // ducking delay
            {
                const float base = expMap(q[0], 0.04f, 1.2f) * srv, fb = q[1] * 0.95f;
                en[0].set(0.002f, expMap(q[4], 0.05f, 1.0f), srv);
                for (int i = 0; i < n; ++i)
                {
                    const float e = clampf(en[0].process(std::max(std::fabs(l[i]), std::fabs(r[i]))) * 4.0f, 0.0f, 1.0f);
                    const float duck = 1.0f - q[3] * e; const float b = smoothT(0, base);
                    float x[2] = { l[i], r[i] }, w[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        const float d = dl[c].read(std::min(b * (c ? 1.02f : 1.0f), maxD));
                        dl[c].write(x[c] + fb * loopFilter(c, d)); w[c] = d * duck;
                    }
                    l[i] = w[0]; r[i] = w[1];
                }
                return;
            }
            default: // 13 shimmer delay
            {
                static const float iv[4] = { 0.5f, 0.7492f, 1.4983f, 2.0f };
                const float base = expMap(q[0], 0.08f, 1.5f) * srv, fb = q[1] * 0.93f;
                const float ratio = iv[std::min(3, (int)(q[4] * 3.99f))];
                for (int c = 0; c < 2; ++c) { ps[c].ratio = ratio; ps[c].setWindow(srv * 0.06f); }
                for (int i = 0; i < n; ++i)
                {
                    const float b = smoothT(0, base);
                    float x[2] = { l[i], r[i] }, w[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        const float d = dl[c].read(std::min(b * (c ? 1.015f : 1.0f), maxD));
                        const float shim = ps[c].process(d);
                        dl[c].write(x[c] + fb * loopFilter(c, lerpf(d, shim, q[3]))); w[c] = lerpf(d, shim, q[3] * 0.6f);
                    }
                    l[i] = w[0]; r[i] = w[1];
                }
                return;
            }
        }
    }

    inline void Machine::procRev(float* l, float* r, int n)
    {
        const float srv = sr;
        // per-variant character: length scale, T60 scale, damping bias, extras
        static const float lenS[11] = { 1.0f, 0.5f, 0.3f, 0.4f, 1.6f, 1.8f, 0.8f, 1.2f, 1.3f, 1.1f, 1.5f };
        static const float t60S[11] = { 1.0f, 0.6f, 0.5f, 0.35f, 1.8f, 2.2f, 0.25f, 1.2f, 1.4f, 1.3f, 2.0f };
        static const float baseMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.3f, 71.3f };
        const int v = std::min(var, 10);
        const float scale = lerpf(0.45f, 1.5f, q[0]) * lenS[v];
        float len[8], g[8];
        const float t60 = expMap(q[1], 0.2f, 20.0f) * t60S[v];
        const bool freeze = (v == 10 && q[5] > 0.5f);
        for (int j = 0; j < 8; ++j)
        {
            len[j] = std::min(baseMs[j] * 0.001f * scale * srv, (float)dl[j].size - 40.0f);
            g[j] = freeze ? 0.99995f : std::pow(10.0f, -3.0f * (len[j] / srv) / t60);
        }
        const float dampF = expMap(1.0f - q[2], 1200.0f, 16000.0f) * (v == 1 ? 1.6f : 1.0f);
        for (int j = 0; j < 8; ++j) op[j].setFreq(clampf(dampF, 800.0f, srv * 0.45f), srv);
        const float toneF = expMap(q[4], 1800.0f, 18000.0f);
        op[8].setFreq(toneF, srv); op[9].setFreq(toneF, srv);
        const float modD = (v == 0 || v == 4 || v == 5 || v == 7 || v == 8 || v == 10 ? q[5] : 0.3f) * srv * 0.0007f;
        const float pre = (v == 0 || v == 1 || v == 3 || v == 4 || v == 5 || v == 8 || v == 10 || v == 6) ? q[3] * 0.25f * srv : 0.0f;
        const float preBloom = (v == 7) ? expMap(q[3], 0.05f, 2.5f) : 0.0f;
        for (int k = 0; k < 4; ++k) lf[k].setRate(0.23f + 0.17f * (float)k, srv);
        if (v == 8) { ps[0].ratio = ps[1].ratio = (q[5] < 0.25f ? 0.5f : q[5] < 0.5f ? 0.7492f : q[5] < 0.75f ? 1.4983f : 2.0f); ps[0].setWindow(srv * 0.06f); ps[1].setWindow(srv * 0.06f); }
        if (v == 9) for (int b = 0; b < 3; ++b) { static const float cf[3] = { 1174.7f, 1568.0f, 2349.3f }; sv[b].setup(srv, cf[b], 150.0f); sv[b + 3].setup(srv, cf[b] * 1.003f, 150.0f); }
        const float apG = (v == 2) ? linMap(q[3], 0.3f, 0.8f) : (v == 1 ? 0.65f : 0.5f);
        const float apScale = srv / 48000.0f;
        static const int apBase[4] = { 142, 107, 379, 277 };
        for (int k = 0; k < 4; ++k) apLen[k] = std::min(639, std::max(8, (int)((float)apBase[k] * apScale)));
        en[0].set(0.0005f, 0.1f, srv);
        en[1].set(expMap(q[3], 0.02f, 1.0f), 0.2f, srv);
        const float gateHold = lerpf(0.06f, 0.5f, q[1]) * srv;

        float meterAcc = 0;
        for (int i = 0; i < n; ++i)
        {
            float inL = l[i], inR = r[i];
            const float inLevel = en[0].process(std::max(std::fabs(inL), std::fabs(inR)));
            if (v == 7) { const float e = clampf(en[1].process(std::max(std::fabs(inL), std::fabs(inR))) * 6.0f, 0.0f, 1.0f); inL *= e; inR *= e; (void)preBloom; }
            if (freeze) { inL = 0; inR = 0; }
            // pre-delay
            if (pre > 1.0f) { dl[8].write(inL); dl[9].write(inR); inL = dl[8].tap((int)std::min(pre, (float)dl[8].size - 4.0f)); inR = dl[9].tap((int)std::min(pre, (float)dl[9].size - 4.0f)); }
            // input diffusion (allpass)
            inL = apProc(0, inL, apG); inL = apProc(1, inL, apG);
            inR = apProc(2, inR, apG); inR = apProc(3, inR, apG);
            if (v == 2) { const float sp = fastTanh((inL + inR) * 0.5f * (1.0f + q[5] * 2.0f)); inL = inL * 0.5f + sp * 0.5f; inR = inR * 0.5f + sp * 0.5f; }
            if (v == 8) { const float sl = ps[0].process(st[1]), sr_ = ps[1].process(st[2]); inL += sl * q[4] * 0.5f; inR += sr_ * q[4] * 0.5f; }

            float outv[8], sum = 0;
            for (int j = 0; j < 8; ++j)
            {
                const float mod = lf[j & 3].sine((float)j * 0.125f) * modD;
                const float d = std::max(4.0f, len[j] + mod);
                outv[j] = op[j].lp(dl[j].read(d)) * g[j];
                sum += outv[j];
            }
            sum *= 0.25f;
            float wl = 0, wr = 0;
            for (int j = 0; j < 8; ++j)
            {
                const float u = outv[j] - sum;
                const float in = (j & 1) ? inR : inL;
                dl[j].write(flushDenorm(in * 0.5f + u));
                if (j & 1) wr += (j & 2 ? -outv[j] : outv[j]); else wl += (j & 2 ? -outv[j] : outv[j]);
            }
            wl *= 0.5f; wr *= 0.5f;
            st[1] = wl; st[2] = wr;
            if (v == 6)   // gated: open on input, close after hold
            {
                if (inLevel > 0.05f) st[3] = gateHold;
                st[3] -= 1.0f;
                const float tg = st[3] > 0 ? 1.0f : 0.0f;
                st[4] += (tg - st[4]) * (tg > st[4] ? 0.01f : 0.004f + q[5] * 0.01f);
                wl *= st[4]; wr *= st[4];
            }
            if (v == 9)   // shrine chimes ring on input
            {
                float cl = 0, cr = 0;
                for (int b = 0; b < 3; ++b) { sv[b].process(l[i] * 0.5f); sv[b + 3].process(r[i] * 0.5f); cl += sv[b].bp * sv[b].k; cr += sv[b + 3].bp * sv[b + 3].k; }
                const float wind = q[5] * 0.003f * rng.bi();
                wl += (cl * 6.0f * q[3]) + wind; wr += (cr * 6.0f * q[3]) + wind;
            }
            wl = op[8].lp(wl); wr = op[9].lp(wr);
            l[i] = wl * 1.2f; r[i] = wr * 1.2f;
            meterAcc = std::max(meterAcc, std::fabs(wl));
        }
        meter.store(clampf(meterAcc * 2.0f, 0.0f, 1.0f));
    }
}
