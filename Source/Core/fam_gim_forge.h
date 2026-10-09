#pragma once
// Gimmick (weird) machines + the seeded FORGE (included by bg_machine.h)
namespace bg
{
    inline void Machine::procGim(float* l, float* r, int n)
    {
        const float srv = sr, lv = q[5] * 2.0f;
        const float maxD = (float)dl[0].size - 8.0f;
        auto expc = [&](float t) { return std::exp(-1.0f / std::max(t * srv, 1.0f)); };
        switch (var)
        {
            case 0: // haunted AM radio
            {
                const float f = expMap(q[0], 500.0f, 3500.0f), Q = linMap(q[4], 0.6f, 4.0f);
                sv[0].setup(srv, f, Q); sv[1].setup(srv, f, Q); op[0].setFreq(4000.0f, srv); en[0].set(0.005f, 0.1f, srv);
                const float wf = expMap(q[2], 1000.0f, 8000.0f);
                for (int i = 0; i < n; ++i)
                {
                    const float m = (l[i] + r[i]) * 0.5f;
                    sv[0].process(m); const float bp = sv[0].bp * sv[0].k;
                    const float e = en[0].process(m);
                    st[1] += (rng.bi() * q[3] * 0.05f - st[1]) * 0.0005f;
                    st[0] += wf * (1.0f + st[1]) / srv; if (st[0] > 1.0f) st[0] -= 1.0f;
                    const float stat = op[0].lp(rng.bi()) * q[1] * 0.35f * (1.0f + e * 3.0f);
                    const float o = fastTanh((bp * 2.0f + stat + std::sin(kTwoPi * st[0]) * q[2] * 0.06f) * 1.2f) * lv;
                    l[i] = r[i] = o;
                }
                break;
            }
            case 1: // sentient toaster
            {
                op[0].setFreq(expMap(1.0f - q[0], 1500.0f, 9000.0f), srv); op[1].setFreq(expMap(1.0f - q[0], 1500.0f, 9000.0f), srv);
                en[0].set(0.005f, 0.1f, srv);
                const float dingF = linMap(q[3], 1500.0f, 3500.0f), cool = expMap(q[2], 0.3f, 3.0f) * srv, dDec = expc(0.8f);
                for (int i = 0; i < n; ++i)
                {
                    const float m = (l[i] + r[i]) * 0.5f, e = en[0].process(m);
                    st[2] += 60.0f / srv; if (st[2] > 1.0f) st[2] -= 1.0f;
                    const float ph = kTwoPi * st[2];
                    const float hum = (std::sin(ph) + 0.5f * std::sin(2 * ph) + 0.25f * std::sin(3 * ph)) * q[1] * 0.04f * (1.0f + e * 3.0f * q[0]);
                    st[5] -= 1.0f;
                    if (e > 0.15f && st[6] < 0.15f && st[5] <= 0.0f) { st[3] = 1.0f; st[5] = cool; }
                    st[6] = e;
                    st[3] *= dDec; st[4] += dingF / srv; if (st[4] > 1.0f) st[4] -= 1.0f;
                    const float ding = std::sin(kTwoPi * st[4]) * st[3] * 0.25f;
                    const float crumb = (rng.uni() < q[4] * 0.002f) ? rng.bi() * 0.2f : 0.0f;
                    l[i] = (op[0].lp(l[i]) + hum + ding + crumb) * lv; r[i] = (op[1].lp(r[i]) + hum + ding + crumb) * lv;
                }
                break;
            }
            case 2: // gravity well
            {
                en[0].set(0.005f, 0.15f, srv);
                const float coef = expMap(q[1], 0.0005f, 0.05f), fb = q[2] * 0.7f;
                op[0].setFreq(expMap(1.0f - q[4], 1500.0f, 16000.0f), srv); op[1].setFreq(expMap(1.0f - q[4], 1500.0f, 16000.0f), srv);
                for (int i = 0; i < n; ++i)
                {
                    const float e = en[0].process(std::max(std::fabs(l[i]), std::fabs(r[i])));
                    const float tgt = 8.0f + q[0] * 0.03f * srv * fastTanh(e * (1.0f + q[3] * 8.0f));
                    st[0] += (tgt - st[0]) * coef;
                    float x[2] = { l[i], r[i] };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float d = dl[c].read(clampf(st[0], 4.0f, maxD));
                        dl[c].write(x[c] + fb * op[c].lp(d));
                        x[c] = d * lv;
                    }
                    l[i] = x[0]; r[i] = x[1];
                }
                break;
            }
            case 3: // dial-up modem
            {
                static const float tab[7] = { 1200, 2100, 980, 1650, 2400, 1800, 1300 };
                en[0].set(0.003f, 0.05f, srv);
                const int hold = (int)(expMap(q[1], 0.03f, 0.4f) * srv);
                for (int i = 0; i < n; ++i)
                {
                    const float m = (l[i] + r[i]) * 0.5f, e = clampf(en[0].process(m) * 3.0f, 0.0f, 1.0f);
                    if (--st[0] <= 0.0f) { st[0] = (float)hold; st[1] = tab[(int)(rng.uni() * 6.99f)] * (0.5f + q[0]); }
                    st[7] += (st[1] - st[7]) * 0.01f;
                    st[2] += st[7] / srv; if (st[2] > 1.0f) st[2] -= 1.0f;
                    const float ph = kTwoPi * st[2];
                    const float tone = std::sin(ph + q[3] * 8.0f * std::sin(ph * 3.7f));
                    const float o = (m * (1.0f - q[2] * 0.7f) + tone * e * q[2] * 1.5f + rng.bi() * q[4] * 0.05f * e) * lv;
                    l[i] = r[i] = fastTanh(o);
                }
                break;
            }
            case 4: // tape stop / vinyl brake
            {
                const float base = 64.0f, brake = expMap(q[1], 0.1f, 2.0f) * srv, restart = expMap(q[2], 0.05f, 1.5f) * srv;
                const float period = expMap(1.0f - q[0], 0.4f, 6.0f) * srv;
                lf[0].setRate(5.0f, srv);
                for (int i = 0; i < n; ++i)
                {
                    st[10] -= 1.0f;
                    int ph = (int)st[2];
                    if (ph == 0 && st[10] <= 0.0f) { st[2] = 1.0f; st[10] = brake; }
                    else if (ph == 1 && st[10] <= 0.0f) { st[2] = 2.0f; st[10] = 0.15f * srv; }
                    else if (ph == 2 && st[10] <= 0.0f) { st[2] = 3.0f; st[10] = restart; }
                    else if (ph == 3 && st[1] <= base + 2.0f) { st[2] = 0.0f; st[10] = period * (1.0f + q[4] * rng.bi() * 0.5f); }
                    float speed = 1.0f;
                    if (ph == 1) speed = std::max(0.0f, st[10] / brake);
                    else if (ph == 2) speed = 0.0f;
                    else if (ph == 3) speed = std::min(2.0f, 0.0f + (1.0f - st[10] / std::max(restart, 1.0f)) * 1.6f + 0.2f);
                    speed += lf[0].sine() * q[3] * 0.02f * (ph == 0 ? 0.0f : 1.0f); lf[0].advance();
                    st[1] = clampf(st[1] + (1.0f - speed), base, maxD * 0.9f);
                    if (ph == 0) st[1] += (base - st[1]) * 0.001f;
                    float x[2] = { l[i], r[i] };
                    for (int c = 0; c < 2; ++c) { dl[c].write(x[c]); x[c] = dl[c].read(std::max(st[1], 2.0f)) * lv; }
                    l[i] = x[0]; r[i] = x[1];
                }
                meter.store(st[2] > 0 ? 1.0f : 0.0f);
                break;
            }
            case 5: // bubble wrap
            {
                const float prob = expMap(q[0], 2.0f, 60.0f) / srv, dec = expc(expMap(q[2], 0.004f, 0.08f));
                for (int i = 0; i < n; ++i)
                {
                    if (rng.uni() < prob)
                        for (int v = 0; v < 8; ++v) if (st[16 + v * 4 + 2] < 0.01f) { st[16 + v * 4 + 0] = 0; st[16 + v * 4 + 1] = expMap(q[1], 400.0f, 3000.0f) * (0.7f + 0.6f * rng.uni() * (0.3f + q[4])); st[16 + v * 4 + 2] = 0.3f + 0.7f * rng.uni(); break; }
                    float pop = 0;
                    for (int v = 0; v < 8; ++v)
                    {
                        float* s = &st[16 + v * 4]; if (s[2] < 0.01f) continue;
                        s[0] += s[1] / srv; if (s[0] > 1.0f) s[0] -= 1.0f;
                        s[1] *= 0.9995f; s[2] *= dec; pop += std::sin(kTwoPi * s[0]) * s[2];
                    }
                    const float x[2] = { l[i], r[i] };
                    l[i] = (x[0] * (1.0f + q[3] * pop * 3.0f) + pop * 0.4f) * lv; r[i] = (x[1] * (1.0f + q[3] * pop * 3.0f) + pop * 0.4f) * lv;
                }
                break;
            }
            case 6: // cosmic ray resonators
            {
                static const float rt[3] = { 1.0f, 1.498f, 2.0f };
                const float f0 = expMap(q[1], 100.0f, 2000.0f), Q = expMap(q[2], 20.0f, 600.0f), prob = expMap(q[0], 0.5f, 40.0f) / srv, g = 0.3f * std::sqrt(Q);
                for (int b = 0; b < 3; ++b) { sv[b].setup(srv, f0 * rt[b], Q); sv[b + 3].setup(srv, f0 * rt[b] * (1.0f + q[3] * 0.03f), Q); }
                for (int i = 0; i < n; ++i)
                {
                    const float ex = (rng.uni() < prob) ? rng.bi() * 0.9f : 0.0f;
                    float oa = 0, ob = 0;
                    for (int b = 0; b < 3; ++b)
                    {
                        sv[b].process(ex + l[i] * 0.2f); sv[b + 3].process(ex * 0.8f + r[i] * 0.2f);
                        oa += sv[b].bp * sv[b].k; ob += sv[b + 3].bp * sv[b + 3].k;
                    }
                    const float hs = rng.bi() * q[4] * 0.01f;
                    l[i] = fastTanh(oa * g * 0.3f + hs) * lv; r[i] = fastTanh(ob * g * 0.3f + hs) * lv;
                }
                break;
            }
            case 7: // wind chimes triggered by transients
            {
                static const int sc[5] = { 0, 2, 4, 7, 9 };
                en[0].set(0.001f, 0.05f, srv); en[1].set(0.05f, 0.3f, srv);
                const float thr = linMap(1.0f - q[0], 0.02f, 0.4f), dec = expc(expMap(q[2], 0.2f, 4.0f));
                const float root = 60.0f + std::floor(q[1] * 24.0f);
                for (int i = 0; i < n; ++i)
                {
                    const float m = std::max(std::fabs(l[i]), std::fabs(r[i])), f = en[0].process(m), s = en[1].process(m);
                    if (f - s > thr && st[5] <= 0.0f)
                    {
                        st[5] = 0.06f * srv;
                        for (int v = 0; v < 8; ++v) if (st[64 + v * 8 + 4] < 0.005f)
                        {
                            const int deg = sc[(int)(rng.uni() * 4.99f)] + (rng.uni() < 0.4f ? 12 : 0) + (q[3] > 0.5f ? (rng.uni() < 0.5f ? 1 : 0) : 0);
                            st[64 + v * 8] = 440.0f * std::pow(2.0f, (root + (float)deg - 69.0f) / 12.0f);
                            st[64 + v * 8 + 4] = 0.35f; st[64 + v * 8 + 5] = (rng.uni() - 0.5f) * q[4] * 2.0f; break;
                        }
                    }
                    st[5] -= 1.0f;
                    float oa = 0, ob = 0;
                    for (int v = 0; v < 8; ++v)
                    {
                        float* s2 = &st[64 + v * 8]; if (s2[4] < 0.005f) continue;
                        s2[1] += s2[0] / srv; s2[2] += s2[0] * 2.76f / srv; s2[3] += s2[0] * 5.4f / srv;
                        if (s2[1] > 1.0f) s2[1] -= 1.0f; if (s2[2] > 1.0f) s2[2] -= 1.0f; if (s2[3] > 1.0f) s2[3] -= 1.0f;
                        const float y = (std::sin(kTwoPi * s2[1]) + 0.4f * std::sin(kTwoPi * s2[2]) + 0.15f * std::sin(kTwoPi * s2[3])) * s2[4];
                        s2[4] *= dec;
                        const float pan = clampf(0.5f + s2[5] * 0.5f, 0.0f, 1.0f); oa += y * (1.0f - pan) * 1.4f; ob += y * pan * 1.4f;
                    }
                    l[i] = (l[i] * 0.5f + oa) * lv; r[i] = (r[i] * 0.5f + ob) * lv;
                }
                break;
            }
            case 8: // heartbeat sidechain pump
            {
                const float bpm_ = linMap(q[0], 40.0f, 160.0f), gap = linMap(q[2], 0.12f, 0.4f), K = linMap(q[3], 6.0f, 30.0f);
                op[0].setFreq(18000.0f, srv); op[1].setFreq(18000.0f, srv);
                for (int i = 0; i < n; ++i)
                {
                    st[0] += bpm_ / 60.0f / srv; if (st[0] >= 1.0f) st[0] -= 1.0f;
                    const float ph = st[0], p1 = std::exp(-ph * K), p2 = ph > gap ? 0.7f * std::exp(-(ph - gap) * K) : 0.0f;
                    const float duck = clampf(p1 + p2, 0.0f, 1.0f);
                    if ((i & 7) == 0) { const float fc = expMap(1.0f - q[4] * duck, 500.0f, 18000.0f); op[0].setFreq(fc, srv); op[1].setFreq(fc, srv); }
                    const float g = 1.0f - q[1] * duck;
                    l[i] = op[0].lp(l[i]) * g * lv; r[i] = op[1].lp(r[i]) * g * lv;
                }
                break;
            }
            case 9: // lava lamp
            {
                const float visc = expMap(1.0f - q[0], 0.3f, 8.0f) * srv, Q = 0.6f + q[2] * 8.0f, drv = 1.0f + q[4] * 4.0f;
                const float coef = 1.0f - expc(expMap(1.0f - q[0], 0.2f, 4.0f));
                for (int i = 0; i < n; ++i)
                {
                    if (--st[0] <= 0.0f) { st[0] = visc * (0.5f + rng.uni()); st[1] = rng.bi(); st[3] = rng.bi(); }
                    st[2] += (st[1] - st[2]) * coef; st[4] += (st[3] - st[4]) * coef;
                    if ((i & 7) == 0) { const float fc = 500.0f * std::pow(2.0f, st[2] * q[1] * 4.0f); sv[0].setup(srv, fc, Q); sv[1].setup(srv, fc, Q); }
                    float x[2] = { fastTanh(l[i] * drv) / std::sqrt(drv), fastTanh(r[i] * drv) / std::sqrt(drv) };
                    for (int c = 0; c < 2; ++c)
                    {
                        sv[c].process(x[c]);
                        dl[c].write(sv[c].lp);
                        x[c] = dl[c].read(clampf(0.006f * srv + st[4] * q[3] * 0.003f * srv * (c ? -1.0f : 1.0f), 4.0f, maxD)) * lv;
                    }
                    l[i] = x[0]; r[i] = x[1];
                }
                break;
            }
            case 10: // washing machine
            {
                const float spin = expMap(q[0], 0.2f, 2.0f);
                op[0].setFreq(expMap(1.0f - q[2], 400.0f, 12000.0f), srv); op[1].setFreq(expMap(1.0f - q[2], 400.0f, 12000.0f), srv);
                bq[0].setPeak(srv, 120.0f, 1.5f, q[4] * 12.0f); bq[1] = bq[0]; bq[1].reset();
                if (st[2] == 0.0f) st[2] = 1.0f;
                for (int i = 0; i < n; ++i)
                {
                    if (--st[3] <= 0.0f) { st[3] = (2.0f + 3.0f * rng.uni()) * srv; if (rng.uni() < q[3]) st[2] = -st[2]; }
                    st[0] += st[2] * spin / srv; if (st[0] > 1.0f) st[0] -= 1.0f; if (st[0] < 0.0f) st[0] += 1.0f;
                    const float ph = kTwoPi * st[0], tr = 1.0f - 0.5f * q[1] * (0.5f + 0.5f * std::sin(ph));
                    float x[2] = { l[i], r[i] };
                    for (int c = 0; c < 2; ++c)
                    {
                        dl[c].write(bq[c].process(x[c]));
                        const float d = dl[c].read(0.004f * srv + std::cos(ph + (float)c) * q[1] * 0.002f * srv);
                        x[c] = op[c].lp(d) * tr * lv;
                    }
                    l[i] = x[0]; r[i] = x[1];
                }
                break;
            }
            case 11: // glitch slice repeater
            {
                const int len = (int)(expMap(q[0], 0.03f, 0.45f) * srv);
                for (int i = 0; i < n; ++i)
                {
                    float x[2] = { l[i], r[i] };
                    for (int c = 0; c < 2; ++c) dl[c].write(x[c]);
                    if (--st[0] <= 0.0f)
                    {
                        st[0] = (float)len;
                        if (rng.uni() < q[1] || q[4] > 0.5f)
                        {
                            st[1] = 1.0f; st[3] = (float)len; st[2] = rng.uni() < q[2] ? 1.0f : 0.0f; st[6] = std::pow(2.0f, q[3] * rng.bi() * 0.5f); st[4] = 0.0f;
                            for (int c = 0; c < 2; ++c) for (int k = 0; k < len && k < dl[c + 2].size; ++k) dl[c + 2].buf[k] = dl[c].tap(len - 1 - k);
                        }
                        else st[1] = 0.0f;
                    }
                    if (st[1] > 0.5f)
                    {
                        st[4] += st[6]; if (st[4] >= st[3]) st[4] -= st[3];
                        const float pos = st[2] > 0.5f ? st[3] - 1.0f - st[4] : st[4];
                        const int i0 = std::min((int)pos, dl[2].size - 2); const float fr = pos - (float)i0;
                        const float fade = std::min(1.0f, std::min(st[4], st[3] - st[4]) / 64.0f);
                        for (int c = 0; c < 2; ++c) x[c] = (dl[c + 2].buf[i0] * (1.0f - fr) + dl[c + 2].buf[std::min(i0 + 1, dl[2].size - 1)] * fr) * fade * lv;
                    }
                    else { x[0] *= lv; x[1] *= lv; }
                    l[i] = x[0]; r[i] = x[1];
                }
                meter.store(st[1]);
                break;
            }
            case 12: // random stutter gate
            {
                const int seed = (int)(q[3] * 999.0f);
                if ((int)st[24] != seed + 1) { Rng pr; pr.seed(4242u + (uint32_t)seed * 17u); for (int s = 0; s < 16; ++s) st[16 + s] = pr.uni(); st[24] = (float)(seed + 1); }
                const float stepLen = srv / expMap(q[0], 2.0f, 32.0f), sm = 1.0f - expc(expMap(q[2], 0.0005f, 0.03f));
                for (int i = 0; i < n; ++i)
                {
                    if (--st[0] <= 0.0f) { const int k = ((int)st[1]) & 15; st[1] += 1.0f; st[0] = stepLen * ((k & 1) ? (1.0f - q[4] * 0.33f) : (1.0f + q[4] * 0.33f)); st[2] = st[16 + k] < q[1] ? 1.0f : 0.0f; }
                    st[3] += (st[2] - st[3]) * sm;
                    l[i] *= st[3] * lv; r[i] *= st[3] * lv;
                }
                meter.store(st[3]);
                break;
            }
            case 13: // cassette warble
            {
                lf[0].setRate(0.6f, srv); lf[1].setRate(9.0f, srv);
                op[0].setFreq(expMap(1.0f - q[4], 3000.0f, 16000.0f), srv); op[1].setFreq(expMap(1.0f - q[4], 3000.0f, 16000.0f), srv);
                const float scale = 0.3f + q[0] * 1.4f;
                for (int i = 0; i < n; ++i)
                {
                    st[1] += (rng.bi() * 0.0004f * srv - st[1]) * 0.0003f;
                    const float d = 0.012f * srv + (lf[0].sine() * q[1] * 0.004f * srv + lf[1].sine() * q[2] * 0.0006f * srv + st[1]) * scale; lf[0].advance(); lf[1].advance();
                    if (st[2] <= 0.0f && rng.uni() < q[3] * 3.0f / srv) st[2] = 0.05f * srv;
                    const float dg = st[2] > 0.0f ? 0.2f : 1.0f; st[2] -= 1.0f; st[3] += (dg - st[3]) * 0.01f;
                    float x[2] = { l[i], r[i] };
                    for (int c = 0; c < 2; ++c) { dl[c].write(x[c]); x[c] = op[c].lp(dl[c].read(clampf(d, 4.0f, maxD))) * st[3] + rng.bi() * q[4] * 0.004f; x[c] *= lv; }
                    l[i] = x[0]; r[i] = x[1];
                }
                break;
            }
            case 14: // robot voice
            {
                const float f = expMap(q[0], 30.0f, 300.0f), fb = q[1] * 0.9f;
                const float dL = std::min(srv / f, maxD), dR = std::min(srv / (f * (1.0f + q[4] * 0.05f)), maxD);
                sv[0].setup(srv, expMap(q[3], 600.0f, 3000.0f), 2.0f); sv[1].setup(srv, expMap(q[3], 600.0f, 3000.0f), 2.0f);
                for (int i = 0; i < n; ++i)
                {
                    st[0] += f / srv; if (st[0] > 1.0f) st[0] -= 1.0f; st[1] += f * (1.0f + q[4] * 0.05f) / srv; if (st[1] > 1.0f) st[1] -= 1.0f;
                    const float car[2] = { std::sin(kTwoPi * st[0]), std::sin(kTwoPi * st[1]) };
                    float x[2] = { l[i], r[i] };
                    for (int c = 0; c < 2; ++c)
                    {
                        float y = lerpf(x[c], x[c] * car[c], q[2]);
                        const float d = dl[c].read(c ? dR : dL);
                        dl[c].write(y + fb * d);
                        y = (y + d * 0.6f);
                        sv[c].process(y); y = lerpf(y, sv[c].bp * sv[c].k * 2.0f, 0.4f * q[3]);
                        x[c] = fastTanh(y * 0.8f) * lv;
                    }
                    l[i] = x[0]; r[i] = x[1];
                }
                break;
            }
            case 15: // underwater
            {
                const float fc = expMap(1.0f - clampf(q[0] * 0.8f + q[2] * 0.2f, 0.0f, 1.0f), 250.0f, 12000.0f);
                bq[0].setLP(srv, fc, 0.9f); bq[1] = bq[0]; bq[1].reset(); bq[2].setLowShelf(srv, 160.0f, q[4] * 8.0f); bq[3] = bq[2]; bq[3].reset();
                lf[0].setRate(0.3f, srv);
                for (int i = 0; i < n; ++i)
                {
                    const float m = lf[0].sine() * q[3] * 0.003f * srv; lf[0].advance();
                    if (st[2] < 0.01f && rng.uni() < q[1] * 8.0f / srv) { st[2] = 0.3f; st[1] = 300.0f + 600.0f * rng.uni(); st[0] = 0; }
                    st[1] *= 1.00006f; st[0] += st[1] / srv; if (st[0] > 1.0f) st[0] -= 1.0f; st[2] *= 0.9995f;
                    const float bub = std::sin(kTwoPi * st[0]) * st[2] * 0.4f;
                    float x[2] = { l[i], r[i] };
                    for (int c = 0; c < 2; ++c)
                    {
                        dl[c].write(x[c]);
                        float y = dl[c].read(clampf(0.005f * srv + m * (c ? -1.0f : 1.0f), 4.0f, maxD));
                        y = bq[c + 2].process(bq[c].process(y)); y = fastTanh(y * (1.0f + q[4] * 2.0f)) + bub;
                        x[c] = y * lv;
                    }
                    l[i] = x[0]; r[i] = x[1];
                }
                break;
            }
            case 16: // walkie-talkie
            {
                bq[0].setHP(srv, linMap(q[0], 400.0f, 800.0f), 0.9f); bq[1] = bq[0]; bq[1].reset();
                bq[2].setLP(srv, linMap(q[0], 2500.0f, 4000.0f), 0.9f); bq[3] = bq[2]; bq[3].reset();
                en[0].set(0.002f, 0.08f, srv);
                for (int i = 0; i < n; ++i)
                {
                    const float m = (l[i] + r[i]) * 0.5f, e = en[0].process(m);
                    const bool open = e > 0.01f;
                    if (open && st[0] < 0.5f) { st[4] = 0.005f * srv; }          // click on open
                    if (!open && st[0] > 0.5f) { st[3] = 0.08f * srv; }          // squelch burst on close
                    st[0] = open ? 1.0f : 0.0f;
                    float y = bq[2].process(bq[0].process(m));
                    y = fastTanh(y * (1.0f + q[4] * 8.0f)) / (1.0f + q[4] * 2.0f);
                    float nz = rng.bi() * q[2] * 0.03f;
                    if (st[3] > 0) { nz += rng.bi() * q[1] * 0.4f; st[3] -= 1.0f; }
                    if (st[4] > 0) { nz += std::sin(st[4] * 0.9f) * q[3] * 0.5f; st[4] -= 1.0f; }
                    l[i] = r[i] = (y + nz) * lv;
                }
                break;
            }
            case 17: // time-warp reverse chunks
            {
                const float T = std::min(expMap(q[0], 0.1f, 0.5f) * srv, maxD * 0.9f), spd = std::pow(2.0f, (q[3] - 0.5f) * 2.0f), fb = q[4] * 0.6f, sm = 1.0f - expc(expMap(q[2], 0.005f, 0.2f));
                for (int i = 0; i < n; ++i)
                {
                    if (--st[0] <= 0.0f) { st[0] = T; st[1] = rng.uni() < q[1] ? 1.0f : 0.0f; }
                    st[2] += (st[1] - st[2]) * sm;
                    st[3] += 2.0f * spd / T; if (st[3] >= 1.0f) st[3] -= 1.0f;
                    const float p1 = st[3], p2 = p1 + 0.5f - std::floor(p1 + 0.5f), g1 = std::sin(kPi * p1), g2 = std::sin(kPi * p2);
                    float x[2] = { l[i], r[i] };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float rev = dl[c].readLin(p1 * T + 2.0f) * g1 * g1 + dl[c].readLin(p2 * T + 2.0f) * g2 * g2;
                        const float fwd = dl[c].readLin(T);
                        const float w = lerpf(fwd, rev, st[2]);
                        dl[c].write(x[c] + fb * w);
                        x[c] = w * lv;
                    }
                    l[i] = x[0]; r[i] = x[1];
                }
                break;
            }
            case 18: // spectral smear (long allpass cascade)
            {
                const int stages = 2 + (int)(q[1] * 10.0f);
                op[0].setFreq(expMap(q[4], 1500.0f, 18000.0f), srv); op[1].setFreq(expMap(q[4], 1500.0f, 18000.0f), srv);
                lf[0].setRate(expMap(q[2], 0.05f, 3.0f), srv);
                for (int i = 0; i < n; ++i)
                {
                    const float a = clampf(0.1f + 0.85f * q[0] + lf[0].sine() * q[2] * 0.1f, -0.97f, 0.97f); lf[0].advance();
                    float x[2] = { l[i], r[i] };
                    for (int c = 0; c < 2; ++c)
                    {
                        float y = x[c] + st[60 + c] * q[3] * 0.9f;
                        for (int s = 0; s < stages; ++s) { float& z = st[32 + s + c * 12]; const float o = a * y + z; z = flushDenorm(y - a * o); y = o; }
                        st[60 + c] = fastTanh(y);
                        x[c] = op[c].lp(y) * lv;
                    }
                    l[i] = x[0]; r[i] = x[1];
                }
                break;
            }
            case 19: // radio scanner
            {
                const float rate = expMap(q[0], 0.3f, 8.0f);
                for (int i = 0; i < n; ++i)
                {
                    st[0] -= 1.0f;
                    if (st[0] <= 0.0f) { st[0] = srv / rate; st[1] = expMap(rng.uni(), 400.0f, 4500.0f); if (rng.uni() < q[3]) st[2] = 1.5f * srv; }
                    st[2] -= 1.0f; const bool locked = st[2] > 0.0f;
                    st[4] += (st[1] - st[4]) * 0.002f;
                    if ((i & 7) == 0) { sv[0].setup(srv, locked ? 1500.0f : st[4], locked ? 0.7f : 6.0f); sv[1].setup(srv, locked ? 1500.0f : st[4], locked ? 0.7f : 6.0f); }
                    sv[0].process(l[i]); sv[1].process(r[i]);
                    const float g = locked ? 1.0f : (1.0f - q[2] * 0.8f);
                    const float nz = rng.bi() * q[4] * (locked ? 0.01f : 0.12f);
                    l[i] = (sv[0].bp * sv[0].k * 1.5f * g + nz) * lv; r[i] = (sv[1].bp * sv[1].k * 1.5f * g + nz) * lv;
                }
                break;
            }
            case 20: // endless drone sustainer
            {
                const float T = std::min(expMap(q[1], 0.05f, 0.8f) * srv, maxD * 0.9f); const bool hold = q[4] > 0.5f;
                const float ratio = std::pow(2.0f, (q[3] - 0.5f) * 2.0f);
                ps[0].ratio = ps[1].ratio = ratio; ps[0].setWindow(srv * 0.06f); ps[1].setWindow(srv * 0.06f);
                op[0].setFreq(expMap(1.0f - q[2], 1200.0f, 16000.0f), srv); op[1].setFreq(expMap(1.0f - q[2], 1200.0f, 16000.0f), srv);
                for (int i = 0; i < n; ++i)
                {
                    float x[2] = { l[i], r[i] };
                    if (!hold)
                    {
                        for (int c = 0; c < 2; ++c) dl[c].write(x[c]);
                        st[1] = 0.0f; st[2] = 0.0f;
                    }
                    else
                    {
                        st[2] += 1.0f / T; if (st[2] >= 1.0f) st[2] -= 1.0f;
                        const float p1 = st[2], p2 = p1 + 0.5f - std::floor(p1 + 0.5f), g1 = std::sin(kPi * p1), g2 = std::sin(kPi * p2);
                        for (int c = 0; c < 2; ++c) x[c] = dl[c].readLin(T - p1 * T + 2.0f) * g1 * g1 + dl[c].readLin(T - p2 * T + 2.0f) * g2 * g2;
                    }
                    const float a = ps[0].process(x[0]), b = ps[1].process(x[1]);
                    l[i] = op[0].lp(lerpf(x[0], a, 0.9f * std::fabs(q[3] - 0.5f) * 2.0f)) * lv; r[i] = op[1].lp(lerpf(x[1], b, 0.9f * std::fabs(q[3] - 0.5f) * 2.0f)) * lv;
                }
                meter.store(hold ? 1.0f : 0.0f);
                break;
            }
            case 21: // granular cloud (12 grains)
            {
                const float dens = expMap(q[2], 2.0f, 80.0f), prob = dens / srv, amp = 1.2f / std::sqrt(std::max(1.0f, dens * expMap(q[1], 0.01f, 0.2f)));
                for (int i = 0; i < n; ++i)
                {
                    dl[0].write(l[i]); dl[1].write(r[i]);
                    if (rng.uni() < prob)
                        for (int g = 0; g < 12; ++g) if (st[64 + g * 8] < 0.5f)
                        {
                            float* s2 = &st[64 + g * 8];
                            s2[0] = 1.0f; s2[1] = 0.0f; s2[2] = expMap(q[1], 0.01f, 0.2f) * srv;
                            s2[3] = clampf(q[0] * 0.8f * srv + 40.0f + q[4] * rng.uni() * 0.15f * srv, 40.0f, maxD - 8.0f);
                            s2[4] = std::pow(2.0f, ((q[3] - 0.5f) * 24.0f + q[4] * 12.0f * rng.bi()) / 12.0f);
                            s2[5] = clampf(0.5f + q[4] * 0.5f * rng.bi(), 0.0f, 1.0f); break;
                        }
                    float oa = 0, ob = 0;
                    for (int g = 0; g < 12; ++g)
                    {
                        float* s2 = &st[64 + g * 8]; if (s2[0] < 0.5f) continue;
                        const float w = 0.5f - 0.5f * std::cos(kTwoPi * s2[1] / s2[2]);
                        s2[3] += 1.0f - s2[4];
                        if (s2[3] < 4.0f || s2[3] > maxD - 4.0f) { s2[0] = 0.0f; continue; }
                        const float a = dl[0].readLin(s2[3]) * w, b = dl[1].readLin(s2[3]) * w;
                        oa += (a * (1.0f - s2[5]) + b * 0.3f) * 1.3f; ob += (b * s2[5] + a * 0.3f) * 1.3f;
                        s2[1] += 1.0f; if (s2[1] >= s2[2]) s2[0] = 0.0f;
                    }
                    l[i] = oa * amp * lv; r[i] = ob * amp * lv;
                }
                break;
            }
            case 22: // blown speaker
            {
                const float d = 1.0f + q[0] * 8.0f;
                bq[0].setPeak(srv, 100.0f, 4.0f, q[2] * 14.0f); bq[1] = bq[0]; bq[1].reset();
                bq[2].setLP(srv, expMap(1.0f - q[4], 2500.0f, 14000.0f), 0.8f); bq[3] = bq[2]; bq[3].reset();
                op[0].setFreq(5000.0f, srv); op[1].setFreq(5000.0f, srv);
                en[0].set(0.002f, 0.05f, srv);
                for (int i = 0; i < n; ++i)
                {
                    const float e = en[0].process(std::max(std::fabs(l[i]), std::fabs(r[i])));
                    if (st[0] <= 0.0f && rng.uni() < q[0] * 0.0005f * 48000.0f / srv) st[0] = 80.0f;
                    const float dg = st[0] > 0.0f ? 0.1f : 1.0f; st[0] -= 1.0f;
                    float x[2] = { l[i], r[i] };
                    for (int c = 0; c < 2; ++c)
                    {
                        float y = x[c] * d; y = y > 0.0f ? fastTanh(y) : std::max(y, -0.4f) * 1.2f;
                        y = bq[c].process(y) * dg;
                        dl[c].write(y);
                        const float rat = dl[c].read(clampf((0.0015f + 0.0015f * rng.uni() * q[1] * e * 4.0f) * srv, 4.0f, maxD)) * q[1] * 0.6f;
                        const float fz = (rng.bi() - op[c].lp(rng.bi())) * e * q[3] * 2.0f;
                        x[c] = bq[c + 2].process(y + rat + fz) * lv;
                    }
                    l[i] = x[0]; r[i] = x[1];
                }
                break;
            }
            default: // 23 haunted music box
            {
                static const int sc[5] = { 0, 2, 4, 7, 9 };
                const float root = 60.0f + std::floor(q[0] * 36.0f), Q = 50.0f + q[1] * 600.0f, g = 0.3f * std::sqrt(Q) * (0.3f + q[3]);
                const float wind = q[2] * 0.6f;
                for (int i = 0; i < n; ++i)
                {
                    st[0] += 1.0f / (4.0f * srv); if (st[0] >= 1.0f) st[0] -= 1.0f;
                    if ((i & 15) == 0)
                        for (int b = 0; b < 5; ++b)
                        {
                            const float f = 440.0f * std::pow(2.0f, (root + (float)sc[b] - 69.0f) / 12.0f) * (1.0f - wind * st[0]);
                            sv[b].setup(srv, f, Q); sv[b + 5].setup(srv, f * (1.0f + q[4] * 0.01f * (float)(b + 1)), Q);
                        }
                    float oa = 0, ob = 0;
                    for (int b = 0; b < 5; ++b) { sv[b].process(l[i]); sv[b + 5].process(r[i]); oa += sv[b].bp * sv[b].k; ob += sv[b + 5].bp * sv[b + 5].k; }
                    l[i] = fastTanh(oa * g * 0.4f) * lv; r[i] = fastTanh(ob * g * 0.4f) * lv;
                }
                break;
            }
        }
    }

    // FORGE: each seed (0..9999) deterministically builds a different chain of
    // filter -> shaper -> modulated delay -> pitch/ring -> diffusion -> filter.
    inline void Machine::procForge(float* l, float* r, int n)
    {
        const float srv = sr;
        const int seed = std::min(kForgeSeeds - 1, (int)(q[0] * (float)kForgeSeeds));
        if ((int)st[0] != seed + 1)
        {
            Rng g; g.seed(0xB1B1u + (uint32_t)seed * 2654435761u);
            for (int k = 0; k < 4; ++k) g.nextU();
            st[0] = (float)(seed + 1);
            st[1] = (float)(g.nextU() % 5);                    // pre filter type
            st[2] = expMap(g.uni(), 100.0f, 8000.0f); st[3] = 0.5f + g.uni() * 5.0f;
            st[4] = (float)(g.nextU() % 6);                    // shaper
            st[5] = 1.0f + g.uni() * 6.0f;
            st[6] = 1.0f + g.uni() * 39.0f;                    // delay ms
            st[7] = g.uni() * 0.75f; st[8] = expMap(g.uni(), 0.1f, 8.0f); st[9] = g.uni() * 4.0f;
            st[10] = (float)(g.nextU() % 4);                   // post filter type
            st[11] = expMap(g.uni(), 400.0f, 12000.0f); st[12] = 0.5f + g.uni() * 3.0f;
            static const float pr[8] = { 1.0f, 1.0f, 1.0f, 0.5f, 2.0f, 1.4983f, 0.7492f, 1.0595f };
            st[13] = pr[g.nextU() % 8]; st[14] = st[13] == 1.0f ? 0.0f : 0.3f + g.uni() * 0.4f;
            st[15] = g.uni();                                   // diffusion amount
            st[16] = (g.uni() < 0.25f) ? expMap(g.uni(), 30.0f, 1500.0f) : 0.0f;   // ring hz
            st[17] = g.uni();                                   // stereo phase offset
            for (int k = 0; k < 4; ++k) { sv[k].reset(); }
        }
        const float inten = q[1], tone = std::pow(2.0f, (q[2] - 0.5f) * 3.0f), motion = q[3], space = q[4], lv = q[5] * 2.0f;
        const int pre = (int)st[1], shp = (int)st[4], post = (int)st[10];
        const float drive = 1.0f + (st[5] - 1.0f) * inten * 1.5f;
        const float fb = st[7] * inten, dly = std::min(st[6] * 0.001f * srv, (float)dl[0].size - 8.0f);
        sv[0].setup(srv, st[2] * tone, st[3]); sv[1].setup(srv, st[2] * tone, st[3]);
        sv[2].setup(srv, st[11] * tone, st[12]); sv[3].setup(srv, st[11] * tone, st[12]);
        lf[0].setRate(st[8], srv);
        for (int k = 0; k < 4; ++k) apLen[k] = std::max(8, std::min(639, (int)((float)(37 + k * 53) * srv / 48000.0f)));
        for (int c = 0; c < 2; ++c) { ps[c].ratio = st[13]; ps[c].setWindow(srv * 0.04f); }
        const float apG = 0.3f + 0.4f * st[15], apAmt = space;
        for (int i = 0; i < n; ++i)
        {
            const float mod = lf[0].sine() * st[9] * motion * 0.001f * srv; lf[0].advance();
            float x[2] = { l[i], r[i] };
            for (int c = 0; c < 2; ++c)
            {
                float y = x[c];
                sv[c].process(y);
                switch (pre) { case 1: y = sv[c].lp; break; case 2: y = sv[c].hp; break; case 3: y = sv[c].bp * sv[c].k * 1.5f; break; case 4: y = sv[c].lp + sv[c].hp; break; default: break; }
                const float v = y * drive;
                switch (shp)
                {
                    case 0: y = fastTanh(v); break;
                    case 1: y = std::sin(kPi * 0.5f * v); break;
                    case 2: y = fastTanh(std::fabs(v) * 2.0f - 0.5f) * (v < 0 ? -1.0f : 1.0f) * 0.8f; break;
                    case 3: { const float s = std::pow(2.0f, 3.0f + (1.0f - inten) * 8.0f); y = std::round(v * s) / s; break; }
                    case 4: y = v > 0 ? fastTanh(v * 2.0f) : fastTanh(v * 0.5f) * 1.6f; break;
                    default: y = clampf(v, -0.7f, 0.7f) * 1.3f; break;
                }
                y = fastTanh(y);
                const float d = dl[c].read(std::max(4.0f, dly * (c ? 1.0f + st[17] * 0.3f : 1.0f) + mod));
                dl[c].write(y + fb * d);
                y = lerpf(y, d, 0.5f * (0.3f + space));
                if (st[16] > 0.0f) { st[20 + c] += st[16] * (c ? 1.0f + st[17] * 0.1f : 1.0f) / srv; if (st[20 + c] > 1.0f) st[20 + c] -= 1.0f; y *= lerpf(1.0f, std::sin(kTwoPi * st[20 + c]), inten * 0.8f); }
                if (st[14] > 0.0f) y = lerpf(y, ps[c].process(y), st[14]);
                if (apAmt > 0.01f) { const float a = apProc(c * 2, apProc(c * 2 + 1, y, apG), apG); y = lerpf(y, a, apAmt); }
                sv[c + 2].process(y);
                switch (post) { case 1: y = sv[c + 2].lp; break; case 2: y = sv[c + 2].hp; break; case 3: y = lerpf(y, sv[c + 2].bp * sv[c + 2].k * 1.5f, 0.7f); break; default: break; }
                x[c] = fastTanh(y) * lv;
            }
            l[i] = x[0]; r[i] = x[1];
        }
        meter.store((float)seed / (float)kForgeSeeds);
    }
}
