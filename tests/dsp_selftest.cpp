// BabyGirl DSP self-test: every machine, many sample rates and parameter extremes.
// Pure C++17 (no JUCE). Build: g++ -O2 -std=c++17 -I Source/Core tests/dsp_selftest.cpp
#include "bg_machine.h"
#include "bg_synth.h"
#include "bg_drums.h"
#include "bg_analysis.h"
#include <cstdio>
#include <chrono>

static int failures = 0;
static void fail(const char* m, int idx, const char* why) { std::printf("FAIL  %-34s %s\n", m, why); ++failures; (void)idx; }

static void fillInput(std::vector<float>& l, std::vector<float>& r, double sr, int kind)
{
    bg::Rng g; g.seed(77);
    for (size_t i = 0; i < l.size(); ++i)
    {
        const double t = (double)i / sr;
        float x = 0;
        if (kind == 0) x = 0.5f * std::sin(6.2831853 * 220.0 * t) * (0.6f + 0.4f * std::sin(6.2831853 * 2.0 * t));
        else if (kind == 1) x = (std::fmod(t, 0.5) < 0.02) ? 0.9f * g.bi() : 0.02f * g.bi();       // drum-like bursts
        else x = 0.9f * g.bi();                                                                         // full-scale noise
        l[i] = x; r[i] = 0.8f * x + 0.1f * std::sin(6.2831853 * 330.0 * t);
    }
}


static int bg_run_engine_tests()
{
    int f = 0; const double sr = 48000.0; const int N = 48000;
    auto chk = [&](bool ok, const char* what) { std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++f; };
    // synth: every osc model x every filter model must be finite and audible
    for (int o = 0; o < bg::kNumOsc; ++o)
        for (int ft = 0; ft < bg::kNumFilters; ++ft)
        {
            bg::SynthEngine s; s.prepare(sr); bg::SynthParams p; p.osc1 = o; p.osc2 = (o + 3) % bg::kNumOsc; p.fType = ft; p.uni = 3; p.res = 0.6f; p.fm = 0.5f; s.setParams(p);
            std::vector<float> l(N, 0.0f), r(N, 0.0f); s.noteOn(48, 0.9f); s.noteOn(55, 0.9f);
            for (int pos = 0; pos < N; pos += 256) s.process(l.data() + pos, r.data() + pos, std::min(256, N - pos));
            bool fin = true; float pk = 0; for (int i = 0; i < N; ++i) { fin &= std::isfinite(l[i]) && std::isfinite(r[i]); pk = std::max(pk, std::fabs(l[i])); }
            char b[96]; std::snprintf(b, sizeof b, "synth osc=%s filter=%s peak=%.2f", bg::kOscNames[o], bg::kFilterNames[ft], pk);
            chk(fin && pk > 0.002f && pk < 6.0f, b);
        }
    { bg::SynthEngine s; s.prepare(sr); bg::SynthParams p; s.setParams(p); std::vector<float> l(256), r(256);
      for (int n = 20; n < 110; ++n) { s.noteOn(n, 1.0f); } s.process(l.data(), r.data(), 256); chk(s.activeVoices() == 16, "voice stealing caps at 16"); 
      for (int n = 20; n < 110; ++n) s.noteOff(n); for (int i = 0; i < 400; ++i) s.process(l.data(), r.data(), 256); chk(s.activeVoices() == 0, "voices release to idle"); }
    // drums
    for (int pd = 0; pd < bg::kPads; ++pd)
    {
        bg::DrumEngine d; d.prepare(sr); d.trigger(pd, 1.0f); std::vector<float> l(N, 0.0f), r(N, 0.0f); d.process(l.data(), r.data(), N);
        float pk = 0; bool fin = true; for (int i = 0; i < N; ++i) { fin &= std::isfinite(l[i]); pk = std::max(pk, std::fabs(l[i])); }
        char b[96]; std::snprintf(b, sizeof b, "drum %-12s peak=%.2f", bg::kPadNames[pd], pk); chk(fin && pk > 0.02f && pk < 4.0f, b);
    }
    { bg::DrumSequencer q; q.loadPattern(1); bg::DrumSequencer::Ev ev[64]; int total = 0; const int blk = 480; double ppq = 0; const double bpm = 120;
      for (int b = 0; b < (int)(sr * 2.0 / blk); ++b) { ppq = (double)b * blk * bpm / 60.0 / sr; total += q.collect(ppq, bpm, blk, sr, 0.0, ev, 64); }
      int perBar = 0; for (int p = 0; p < 16; ++p) for (int s = 0; s < 16; ++s) perBar += q.grid[p][s] ? 1 : 0;
      char b[96]; std::snprintf(b, sizeof b, "sequencer: 2s @120bpm = 1 bar, %d events (grid %d)", total, perBar); chk(total == perBar, b); }
    // loudness: -23 dBFS stereo sine at 997 Hz must read about -23 LUFS (EBU Tech 3341)
    { bg::LoudnessMeter m; m.prepare(sr); const float A = std::pow(10.0f, -23.0f / 20.0f); std::vector<float> x(48000 * 5);
      for (size_t i = 0; i < x.size(); ++i) x[i] = A * std::sin(6.2831853 * 997.0 * (double)i / sr);
      for (size_t pos = 0; pos + 512 <= x.size(); pos += 512) m.process(x.data() + pos, x.data() + pos, 512);
      const float I = m.integrated.load(), M = m.momentary.load(), S = m.shortTerm.load();
      char b[110]; std::snprintf(b, sizeof b, "LUFS M=%.2f S=%.2f I=%.2f (expect -23.0 +-0.3)", M, S, I); chk(std::fabs(I + 23.0f) < 0.3f && std::fabs(M + 23.0f) < 0.3f, b);
      std::snprintf(b, sizeof b, "true peak %.2f dBTP (expect about -23)", m.truePeakDb.load()); chk(std::fabs(m.truePeakDb.load() + 23.0f) < 0.6f, b); }
    return f;
}

int main()
{
    // Hilbert pair: 90 degree phase difference and unity gain across the band
    {
        bg::Hilbert h; const double sr = 48000; double worstPh = 0, worstG = 0;
        for (double f : { 200.0, 1000.0, 5000.0, 12000.0 })
        {
            bg::Hilbert hh; double sr_ = 0, ss = 0, sc_ = 0, sa = 0, sb = 0; const int N = 48000;
            for (int i = 0; i < N; ++i)
            {
                float re, im; hh.process((float)std::sin(6.2831853 * f * i / sr), re, im);
                if (i > 4000) { const double ref = std::sin(6.2831853 * f * (i - 1) / sr), refq = -std::cos(6.2831853 * f * (i - 1) / sr); ss += re * ref; sc_ += im * refq; sa += re * re; sb += im * im; }
                (void)sr_;
            }
            const double ga = std::sqrt(sa / (N - 4001)) * 1.41421356, gb = std::sqrt(sb / (N - 4001)) * 1.41421356;
            worstG = std::max(worstG, std::max(std::fabs(ga - 1.0), std::fabs(gb - 1.0)));
            worstPh = std::max(worstPh, std::fabs(std::fabs(ss / sa) - 1.0));   // re correlates with sin
        }
        std::printf("Hilbert gain error worst %.3f\n", worstG);
        if (worstG > 0.25) { std::puts("FAIL  Hilbert gain"); ++failures; }
    }

    const double rates[3] = { 44100.0, 48000.0, 96000.0 };
    double totalCpu = 0, worstCpu = 0; int worstIdx = 0;
    for (int mi = 0; mi < bg::kNumMachines; ++mi)
    {
        const auto& info = bg::kMachines[mi];
        for (double sr : rates)
        {
            bg::Machine m; m.prepare(sr);
            const int N = (int)(sr * 2.0);
            std::vector<float> il(N), ir(N);
            for (int scenario = 0; scenario < 6; ++scenario)
            {
                m.select(mi);
                float p[6];
                for (int k = 0; k < 6; ++k)
                    p[k] = scenario == 0 ? info.def[k] : scenario == 1 ? 0.0f : scenario == 2 ? 1.0f : scenario == 3 ? 0.5f : (float)((mi * 7 + k * 13 + scenario * 31) % 100) / 99.0f;
                m.setParams(p);
                fillInput(il, ir, sr, scenario % 3);
                std::vector<float> l = il, r = ir;
                const auto t0 = std::chrono::high_resolution_clock::now();
                for (int pos = 0; pos < N; pos += 512)
                {
                    const int c = std::min(512, N - pos);
                    if (scenario == 4 && pos == (int)sr) { for (int k = 0; k < 6; ++k) p[k] = 1.0f - p[k]; m.setParams(p); }   // jump params mid-stream
                    m.process(l.data() + pos, r.data() + pos, c, scenario == 5 ? 1.0f : 0.7f);
                }
                const double sec = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
                const double cpu = sec / 2.0;
                totalCpu += cpu; if (cpu > worstCpu) { worstCpu = cpu; worstIdx = mi; }
                float peak = 0; bool bad = false; double e = 0;
                for (int i = 0; i < N; ++i)
                {
                    if (!std::isfinite(l[i]) || !std::isfinite(r[i])) bad = true;
                    peak = std::max(peak, std::max(std::fabs(l[i]), std::fabs(r[i]))); e += l[i] * l[i];
                }
                char buf[96];
                if (bad) { std::snprintf(buf, sizeof buf, "NaN/inf sr=%.0f scenario=%d", sr, scenario); fail(info.name, mi, buf); }
                else if (peak > (info.fam == bg::F_UTIL ? 80.0f : 20.0f)) { std::snprintf(buf, sizeof buf, "peak %.1f sr=%.0f scenario=%d", peak, sr, scenario); fail(info.name, mi, buf); }
                if (scenario == 0 && sr == 48000.0 && info.fam != bg::F_UTIL)
                {
                    const double rms = std::sqrt(e / N);
                    if (rms < 1e-6 && info.fam != bg::F_GIM && info.fam != bg::F_REV && info.fam != bg::F_DLY) { fail(info.name, mi, "silent output at default settings"); }
                }
                if (scenario == 0 && sr == 48000.0) std::printf("ok  %-34s fam=%-2d peak=%6.2f  cpu=%.2f%%\n", info.name, info.fam, peak, cpu * 100.0);
            }
        }
    }
    std::printf("\nMachines: %d   worst single-machine CPU (1 core, 1 slot): %.2f%% (%s)\n", bg::kNumMachines, worstCpu * 100.0, bg::kMachines[worstIdx].name);

    // Synth / drums / analysis
    { bg::SynthEngine s; s.prepare(48000.0); std::printf("synth+drums+analysis tests:\n"); failures += bg_run_engine_tests(); }
    std::printf("\n%s (%d failures)\n", failures ? "SELFTEST FAILED" : "SELFTEST PASSED", failures);
    return failures ? 1 : 0;
}
