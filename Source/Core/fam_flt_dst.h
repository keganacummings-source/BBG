#pragma once
// Filters + Distortion families (included by bg_machine.h)
namespace bg
{
    inline void Machine::procFlt(float* l, float* r, int n)
    {
        const float srv = sr;
        const float cutBase = expMap(q[0], 20.0f, 18000.0f);

        if (var == 0)   // Moog-style ladder, 2x internal substeps
        {
            const float k = q[1] * 4.0f, drv = 1.0f + q[2] * 6.0f, comp = 1.0f + k * q[5] * 0.5f;
            en[0].set(0.001f, expMap(q[4], 0.02f, 0.8f), srv);
            for (int i = 0; i < n; ++i)
            {
                const float e = en[0].process(std::max(std::fabs(l[i]), std::fabs(r[i])));
                const float fc = clampf(cutBase * (1.0f + q[3] * 6.0f * e), 20.0f, srv * 0.32f);
                const float g = 1.0f - std::exp(-kPi * fc / srv);
                float x[2] = { l[i], r[i] };
                for (int c = 0; c < 2; ++c)
                {
                    float* y = &st[c * 4];
                    for (int s = 0; s < 2; ++s)
                    {
                        const float in = fastTanh(drv * x[c] - k * y[3]);
                        y[0] += g * (in - fastTanh(y[0]));
                        y[1] += g * (fastTanh(y[0]) - fastTanh(y[1]));
                        y[2] += g * (fastTanh(y[1]) - fastTanh(y[2]));
                        y[3] += g * (fastTanh(y[2]) - fastTanh(y[3]));
                    }
                    for (int j = 0; j < 4; ++j) y[j] = flushDenorm(y[j]);
                    x[c] = y[3] * comp;
                }
                l[i] = x[0]; r[i] = x[1];
            }
            return;
        }
        if (var == 1)   // MS-20 style HP -> LP with clipped resonance
        {
            const float lpQ = 0.6f + q[1] * 14.0f + q[5] * 25.0f, hpQ = 0.6f + q[3] * 12.0f, drv = 1.0f + q[4] * 5.0f;
            const float fl = cutBase, fh = expMap(q[2], 20.0f, 6000.0f);
            sv[0].setup(srv, fh, hpQ); sv[1].setup(srv, fh, hpQ); sv[2].setup(srv, fl, lpQ); sv[3].setup(srv, fl, lpQ);
            const float norm = 1.0f / (1.0f + 0.12f * (lpQ - 0.7f));
            for (int i = 0; i < n; ++i)
            {
                float a = fastTanh(l[i] * drv), b = fastTanh(r[i] * drv);
                if (q[5] > 0.6f) { const float k = (q[5] - 0.6f) * 0.002f; a += k * rng.bi(); b += k * rng.bi(); }
                sv[0].process(a); sv[1].process(b);
                sv[2].process(fastTanh(sv[0].hp)); sv[3].process(fastTanh(sv[1].hp));
                l[i] = fastTanh(sv[2].lp * norm * 1.4f); r[i] = fastTanh(sv[3].lp * norm * 1.4f);
            }
            return;
        }
        if (var == 2)   // TB-303 style 3-pole diode ladder with accent envelope
        {
            const float k = q[1] * 3.4f, envAmt = q[2], accent = q[4], dist = 1.0f + q[5] * 6.0f;
            en[0].set(0.0008f, expMap(q[3], 0.02f, 0.6f), srv);
            op[2].setFreq(110.0f, srv); op[3].setFreq(110.0f, srv);
            for (int i = 0; i < n; ++i)
            {
                const float e = en[0].process(std::max(std::fabs(l[i]), std::fabs(r[i])));
                const float fc = clampf(cutBase * (1.0f + envAmt * 5.0f * e * (1.0f + accent)), 20.0f, srv * 0.32f);
                const float g = 1.0f - std::exp(-kPi * fc / srv);
                float x[2] = { l[i], r[i] };
                for (int c = 0; c < 2; ++c)
                {
                    float* y = &st[c * 4];
                    for (int s = 0; s < 2; ++s)
                    {
                        const float fb = y[2];
                        const float fbHp = fb - op[c + 2].lp(fb);
                        const float in = fastTanh(x[c] * 1.5f - k * (1.0f + accent * 0.5f) * fbHp);
                        y[0] += g * (in - y[0]); y[1] += g * (y[0] - y[1]); y[2] += g * (y[1] - y[2]);
                    }
                    for (int j = 0; j < 3; ++j) y[j] = flushDenorm(y[j]);
                    x[c] = fastTanh(y[2] * dist * (1.0f + accent * 0.4f)) / std::sqrt(dist) * 1.3f;
                }
                l[i] = x[0]; r[i] = x[1];
            }
            return;
        }
        if (var == 3)   // SEM-style state-variable morph with LFO
        {
            const float qq = 0.5f + q[1] * 10.0f, drv = 1.0f + q[2] * 4.0f, m = q[5];
            lf[0].setRate(expMap(q[4], 0.1f, 12.0f), srv);
            for (int i = 0; i < n; ++i)
            {
                const float fc = cutBase * std::pow(2.0f, lf[0].sine() * q[3] * 2.0f); lf[0].advance();
                if ((i & 7) == 0) { sv[0].setup(srv, fc, qq); sv[1].setup(srv, fc, qq); }
                float a = fastTanh(l[i] * drv) / std::sqrt(drv), b = fastTanh(r[i] * drv) / std::sqrt(drv);
                sv[0].process(a); sv[1].process(b);
                auto mix = [&](Svf& s) { const float nt = s.lp + s.hp; return m < 0.5f ? lerpf(s.lp, nt, m * 2.0f) : lerpf(nt, s.hp, (m - 0.5f) * 2.0f); };
                l[i] = mix(sv[0]); r[i] = mix(sv[1]);
            }
            return;
        }
        if (var == 4)   // envelope filter / auto-wah
        {
            const float qq = 1.0f + q[1] * 12.0f, base = expMap(q[0], 150.0f, 1500.0f);
            en[0].set(0.002f, expMap(q[4], 0.02f, 0.4f), srv);
            const float sens = 1.0f + q[3] * 14.0f; const bool down = q[5] > 0.5f;
            for (int i = 0; i < n; ++i)
            {
                const float e = clampf(en[0].process(std::max(std::fabs(l[i]), std::fabs(r[i]))) * sens, 0.0f, 1.0f);
                const float fc = base * std::pow(2.0f, (down ? 1.0f - e : e) * q[2] * 4.5f);
                if ((i & 3) == 0) { sv[0].setup(srv, fc, qq); sv[1].setup(srv, fc, qq); }
                sv[0].process(l[i]); sv[1].process(r[i]);
                l[i] = fastTanh(sv[0].bp * sv[0].k * 1.5f); r[i] = fastTanh(sv[1].bp * sv[1].k * 1.5f);
            }
            return;
        }
        if (var == 5)   // vowel formant filter
        {
            static const float F[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
            lf[0].setRate(expMap(q[4], 0.1f, 4.0f), srv);
            const float qq = 4.0f + q[1] * 20.0f, gender = linMap(q[2], 0.75f, 1.35f), drv = 1.0f + q[3] * 5.0f;
            for (int i = 0; i < n; ++i)
            {
                if ((i & 7) == 0)
                {
                    float pos = q[0] * 4.0f + lf[0].sine() * q[5] * 2.0f; pos = clampf(pos, 0.0f, 3.9999f);
                    const int p0 = (int)pos; const float fr = pos - (float)p0;
                    for (int f = 0; f < 3; ++f)
                    {
                        const float fc = lerpf(F[p0][f], F[p0 + 1][f], fr) * gender;
                        sv[f].setup(srv, fc, qq); sv[f + 3].setup(srv, fc, qq);
                    }
                }
                lf[0].advance();
                const float a = fastTanh(l[i] * drv), b = fastTanh(r[i] * drv);
                float oa = 0, ob = 0; static const float gn[3] = { 1.0f, 0.6f, 0.35f };
                for (int f = 0; f < 3; ++f) { sv[f].process(a); sv[f + 3].process(b); oa += sv[f].bp * sv[f].k * gn[f]; ob += sv[f + 3].bp * sv[f + 3].k * gn[f]; }
                l[i] = oa * 1.3f; r[i] = ob * 1.3f;
            }
            return;
        }
        if (var == 6)   // Karplus-Strong comb resonator
        {
            const float f0 = expMap(q[0], 40.0f, 1200.0f), fb = 0.5f + q[1] * 0.495f * (q[5] > 0.5f ? -1.0f : 1.0f);
            const float dL = srv / (f0 * (1.0f + q[3] * 0.03f)), dR = srv / (f0 * (1.0f - q[3] * 0.03f));
            const float fbAbs = std::fabs(fb);
            op[0].setFreq(expMap(1.0f - q[2], 800.0f, 16000.0f), srv); op[1].setFreq(expMap(1.0f - q[2], 800.0f, 16000.0f), srv);
            const float ex = 0.5f + q[4];
            for (int i = 0; i < n; ++i)
            {
                const float yl = l[i] * ex + fb * op[0].lp(dl[0].read(dL - 1.0f));
                const float yr = r[i] * ex + fb * op[1].lp(dl[1].read(dR - 1.0f));
                dl[0].write(yl); dl[1].write(yr);
                l[i] = yl * (1.0f - fbAbs * 0.9f) * 0.8f; r[i] = yr * (1.0f - fbAbs * 0.9f) * 0.8f;
            }
            return;
        }
        if (var == 7)   // Lorenz attractor modulated filter
        {
            if (st[63] < 0.0f) { st[0] = 1.0f; st[1] = 1.0f; st[2] = 1.0f; st[63] = 0.0f; }
            const float dt = expMap(q[4], 0.0005f, 0.012f) * (48000.0f / srv), qq = 0.6f + q[1] * 12.0f, drv = 1.0f + q[2] * 5.0f;
            for (int i = 0; i < n; ++i)
            {
                float& x = st[0]; float& y = st[1]; float& z = st[2];
                const float dx = 10.0f * (y - x), dy = x * (28.0f - z) - y, dz = x * y - 2.6667f * z;
                x += dx * dt; y += dy * dt; z += dz * dt;
                const float xn = clampf(x / 20.0f, -1.5f, 1.5f);
                const float fc = cutBase * std::pow(2.0f, xn * q[3] * 3.0f);
                if ((i & 3) == 0) { sv[0].setup(srv, fc, qq); sv[1].setup(srv, fc, qq); }
                sv[0].process(fastTanh(l[i] * drv)); sv[1].process(fastTanh(r[i] * drv));
                l[i] = lerpf(sv[0].lp, sv[0].bp * sv[0].k, q[5]); r[i] = lerpf(sv[1].lp, sv[1].bp * sv[1].k, q[5]);
            }
            meter.store(clampf(std::fabs(st[0]) / 20.0f, 0.0f, 1.0f));
            return;
        }
        if (var == 8)   // resonator bank tuned to a scale
        {
            static const int sc[4][5] = { { 0, 2, 4, 7, 9 }, { 0, 3, 5, 7, 10 }, { 0, 2, 4, 6, 8 }, { 0, 1, 4, 5, 7 } };
            const int scale = std::min(3, (int)(q[5] * 3.99f));
            const float root = 36.0f + std::floor(q[0] * 36.0f), Q = 20.0f + q[1] * 400.0f, gain = 0.35f * std::sqrt(Q);
            for (int b = 0; b < 5; ++b)
            {
                const float note = root + (float)sc[scale][b] + (b >= 3 ? 12.0f : 0.0f);
                const float f = 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f) * (1.0f + q[2] * 0.01f * (float)(b - 2));
                const float fr = f * (1.0f + q[4] * 0.012f);
                sv[b].setup(srv, f, Q); sv[b + 5].setup(srv, fr, Q);
            }
            for (int i = 0; i < n; ++i)
            {
                float oa = 0, ob = 0;
                for (int b = 0; b < 5; ++b)
                {
                    const float w = 1.0f - (1.0f - q[3]) * (float)b * 0.18f;
                    sv[b].process(l[i]); sv[b + 5].process(r[i]);
                    oa += sv[b].bp * sv[b].k * w; ob += sv[b + 5].bp * sv[b + 5].k * w;
                }
                l[i] = fastTanh(oa * gain * 0.4f); r[i] = fastTanh(ob * gain * 0.4f);
            }
            return;
        }
        // var 9: 8-step pattern filter
        {
            const int seed = (int)(q[5] * 999.0f);
            if ((int)st[24] != seed + 1)
            {
                Rng pr; pr.seed(9001u + (uint32_t)seed * 131u);
                for (int s = 0; s < 8; ++s) st[16 + s] = pr.uni();
                st[24] = (float)(seed + 1);
            }
            const float rate = expMap(q[4], 0.5f, 16.0f) / srv, qq = 0.6f + q[1] * 10.0f, drv = 1.0f + q[2] * 5.0f, gl = 1.0f - std::exp(-1.0f / (0.004f * srv));
            for (int i = 0; i < n; ++i)
            {
                st[25] += rate; if (st[25] >= 8.0f) st[25] -= 8.0f;
                const float target = st[16 + (int)st[25]];
                st[26] += (target - st[26]) * gl;
                const float fc = cutBase * std::pow(2.0f, st[26] * q[3] * 5.0f);
                if ((i & 3) == 0) { sv[0].setup(srv, fc, qq); sv[1].setup(srv, fc, qq); }
                sv[0].process(fastTanh(l[i] * drv)); sv[1].process(fastTanh(r[i] * drv));
                l[i] = sv[0].lp; r[i] = sv[1].lp;
            }
            meter.store(st[26]);
        }
    }

    inline void Machine::procDst(float* l, float* r, int n)
    {
        const float srv = sr;
        const float drv = 1.0f + q[0] * q[0] * 40.0f + q[0] * 4.0f;
        const float bias = (q[3] - 0.5f) * 2.0f;
        const float outG = dbToGain(linMap(q[2], -18.0f, 6.0f));
        op[2].setFreq(18.0f, srv); op[3].setFreq(18.0f, srv);
        const float toneF = expMap(q[1], 700.0f, 14000.0f);
        op[4].setFreq(toneF, srv); op[5].setFreq(toneF, srv);
        // pre / post filters per voicing
        switch (var)
        {
            case 4: op[6].setFreq(1000.0f, srv); op[7].setFreq(1000.0f, srv); bq[1].setPeak(srv, 1000.0f, 0.6f, -q[5] * 14.0f); bq[3] = bq[1]; bq[3].reset(); op[8].setFreq(2200.0f, srv); op[9].setFreq(2200.0f, srv); break;
            case 5: bq[0].setLP(srv, expMap(1.0f - q[0], 1500.0f, 20000.0f), 0.7071f); bq[2] = bq[0]; bq[2].reset(); break;
            case 6: bq[0].setPeak(srv, 720.0f, 0.8f, q[4] * 8.0f); bq[2] = bq[0]; bq[2].reset(); break;
            case 7: bq[0].setPeak(srv, 60.0f, 1.0f, (q[4] - 0.2f) * 7.0f); bq[2] = bq[0]; bq[2].reset(); op[4].setFreq(expMap(1.0f - q[1], 3500.0f, 18000.0f), srv); op[5].setFreq(expMap(1.0f - q[1], 3500.0f, 18000.0f), srv); break;
            case 8: op[6].setFreq(180.0f, srv); op[7].setFreq(180.0f, srv); break;
            case 13: bq[0].setHP(srv, expMap(q[1], 500.0f, 6000.0f), 0.7071f); bq[2] = bq[0]; bq[2].reset(); break;
            default: break;
        }
        en[0].set(0.002f, 0.08f, srv);
        const float gateThr = ((var == 2 || var == 15) ? q[4] : q[5]) * 0.04f;
        const bool useOs = (var != 11 && var != 12 && var != 13 && var != 8);

        auto shape = [&](float v, int ch) -> float
        {
            switch (var)
            {
                case 0: { const float x = v * drv * (0.6f + q[4] * 0.8f) + bias * 0.4f; return x >= 0.0f ? fastTanh(x * 1.4f) : -fastTanh(-x * 0.8f); }
                case 1: { const float x = v * drv * (0.6f + q[4] * 0.8f) + bias * 0.3f; const float s = x >= 0.0f ? 1.0f : -1.0f; return s * (1.0f - std::exp(-std::fabs(x) * 1.5f)); }
                case 2: { const float x = v * drv; const float shift = 0.3f * bias + q[5] * 0.5f * st[6]; return fastTanh(x + shift) - fastTanh(shift); }
                case 3: { const float x = v * drv; float y = fastTanh(x * 2.0f) * 0.7f + clampf(x, -1.0f, 1.0f) * 0.3f; y = lerpf(y, std::fabs(y) * 1.4f - 0.3f, q[4] * 0.6f); return y + bias * 0.1f; }
                case 4: { const float s1 = fastTanh(v * drv); return fastTanh(s1 * 4.0f); }
                case 5: { const float x = v * drv * 2.0f; return q[5] < 0.5f ? clampf(x, -0.6f, 0.6f) * 1.6f : fastTanh(x * 2.0f) * 0.9f; }
                case 6: { const float x = v * drv * 1.5f; const float cl = x / (1.0f + std::fabs(x)); return lerpf(cl * 1.3f, v, q[5] * 0.7f); }
                case 7: { const float x = v * drv * 0.7f; const float y = fastTanh(x + 0.3f * q[5] * st[8 + ch]); st[8 + ch] = y; return y; }
                case 9: { float y = v * (1.0f + q[0] * 8.0f) + bias * 0.5f; const int stg = 1 + (int)(q[4] * 3.99f); for (int s = 0; s < stg; ++s) y = std::sin(kPi * 0.5f * y) * 1.15f; return lerpf(y, fastTanh(y), 1.0f - q[5]) ; }
                case 10: { const float x = v * drv; const float y = fastTanh(x * 3.0f); return lerpf(y, 2.0f * std::fabs(y) - 0.6f, q[4]) + bias * 0.1f; }
                case 14: { const float x = v * drv * 1.5f + bias * 0.1f; const float db = q[3] * 0.3f; const float y = x > db ? x - db : (x < -db ? x + db : 0.0f); return fastTanh(y * (1.5f + q[5] * 3.0f)); }
                case 15: { const float x = v * drv; const float nz = 1.0f - q[3] * std::fabs(rng.bi()) * 0.8f; return fastTanh((x * 2.0f + bias * 0.3f)) * nz; }
                case 16: { const float x = v * drv * 1.4f; const float p = 1.5f + q[3] * 5.0f; const float pp = x >= 0.0f ? p : p * (1.0f + q[4]); const float ax = std::fabs(x); const float y = x / std::pow(1.0f + std::pow(ax, pp), 1.0f / pp); return lerpf(y, v, q[5] * 0.6f); }
                default: return fastTanh(v * drv);
            }
        };

        for (int i = 0; i < n; ++i)
        {
            float in[2] = { l[i], r[i] }, out[2];
            st[6] = en[0].process(std::max(std::fabs(in[0]), std::fabs(in[1])));   // shared level (fuzz sag, gates)
            // sag: dynamic gain reduction for tube stages
            float sagG = 1.0f;
            if (var <= 1) sagG = 1.0f / (1.0f + q[5] * 1.5f * st[6]);
            const float gateG = (var == 2 || var == 3 || var == 10 || var == 15) ? clampf(st[6] / std::max(gateThr, 1e-5f), 0.0f, 1.0f) : 1.0f;
            const float gateE = gateThr;
            const float gG = gateG; (void)gateE;
            for (int c = 0; c < 2; ++c)
            {
                float x = in[c] * sagG;
                if (var == 4 || var == 6 || var == 5 || var == 7) x = bq[c * 2].process(x);   // pre filters
                float y;
                if (var == 8)
                {
                    const float low = op[c + 6].lp(x), high = x - low;
                    y = fastTanh(low * drv * (1.0f + q[4] * 2.0f) + bias * 0.3f) / (1.0f + q[4]) + fastTanh(high * drv * 0.5f) * 0.9f;
                }
                else if (var == 11)
                {
                    const float bits = 2.0f + q[0] * 14.0f, steps = std::pow(2.0f, bits - 1.0f);
                    const float div = expMap(1.0f - q[1], 1.0f, 64.0f);
                    st[16 + c] -= 1.0f;
                    if (st[16 + c] <= 0.0f) { st[16 + c] += div * (1.0f + q[4] * 0.8f * rng.bi()); st[18 + c] = std::round((x + (q[3] * rng.bi()) / steps) * steps) / steps; }
                    y = st[18 + c];
                }
                else if (var == 12)
                {
                    const float mu = linMap(q[3], 15.0f, 255.0f), bits = 12.0f - q[0] * 8.0f, steps = std::pow(2.0f, bits - 1.0f);
                    const float s = x < 0 ? -1.0f : 1.0f, cmp = s * std::log(1.0f + mu * std::min(std::fabs(x), 1.0f)) / std::log(1.0f + mu);
                    const float qd = std::round(cmp * steps) / steps;
                    const float ex = s * (std::pow(1.0f + mu, std::fabs(qd)) - 1.0f) / mu;
                    y = lerpf(qd, ex, q[4]) + q[5] * 0.02f * rng.bi();
                }
                else if (var == 13)
                {
                    const float band = clampf(bq[c * 2].process(x) * (1.0f + q[0] * 4.0f), -1.0f, 1.0f);
                    const float b2 = band * band, b3 = b2 * band, b5 = b3 * b2;
                    const float t2 = 2.0f * b2, t3 = 4.0f * b3 - 3.0f * band, t5 = 16.0f * b5 - 20.0f * b3 + 5.0f * band;
                    y = x + q[0] * (q[3] * t2 + q[4] * t3 + q[5] * t5) * 0.5f;
                }
                else if (useOs) y = os[c].process(x, [&](float v) { return shape(v, c); });
                else y = shape(x, c);
                y = op[c + 2].hp(y);   // DC removal
                // post tone
                switch (var)
                {
                    case 4: { const float lp = op[c + 6].lp(y), hp = y - op[c + 8].lp(y); y = lerpf(lp, hp, q[1]) + 0.0f; y = bq[c * 2 + 1].process(y); break; }
                    case 8: case 11: case 12: y = op[c + 4].lp(y); break;
                    case 13: break;
                    case 6: y = op[c + 4].lp(y) * 0.7f + y * 0.3f * q[1]; break;
                    case 7: y = op[c + 4].lp(y); break;
                    default: y = op[c + 4].lp(y); break;
                }
                out[c] = y * outG * gG * ((var == 4) ? 0.9f : 1.0f);
            }
            if (var == 15 && gG < 0.2f && q[5] > 0.01f && rng.uni() < q[5] * 0.002f) { out[0] += q[5] * 0.2f * rng.bi(); out[1] += q[5] * 0.2f * rng.bi(); }
            l[i] = out[0]; r[i] = out[1];
        }
        meter.store(clampf(st[6] * 2.0f, 0.0f, 1.0f));
    }
}
