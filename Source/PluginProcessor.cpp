#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace BabyGirl;

BabyGirlAudioProcessor::BabyGirlAudioProcessor()
    : AudioProcessor(BusesProperties()
#if BG_FX_BUILD
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
#else
                     .withInput("Input", juce::AudioChannelSet::stereo(), false)   // instrument: audio input optional (sidechain/FX use)
#endif
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    machines = std::make_unique<std::array<bg::Machine, kRackSlots>>();
    for (int s = 0; s < kRackSlots; ++s)
    {
        rMach[s] = rawS(rackId(s, "MACH")); rOn[s] = rawS(rackId(s, "ON")); rMix[s] = rawS(rackId(s, "MIX"));
        for (int k = 0; k < 6; ++k) rP[s][k] = rawS(rackId(s, "P" + juce::String(k + 1)));
        slotMeter[s].store(0.0f); slotMachine[s].store(0);
    }
    for (int p = 0; p < bg::kPads; ++p) { dTune[p] = rawS(padId(p, "TUNE")); dDec[p] = rawS(padId(p, "DEC")); dLvl[p] = rawS(padId(p, "LVL")); dPan[p] = rawS(padId(p, "PAN")); }
    sequencer.loadPattern(1);
}

juce::AudioProcessorValueTreeState::ParameterLayout BabyGirlAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // --- Hush Parameters (original IDs preserved) ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TUBE_DRIVE", "Tube Drive", juce::NormalisableRange<float>(1.0f, 10.0f, 0.1f), 3.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TUBE_BIAS", "Tube Bias", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.42f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "TUBE_PENTODE", "Pentode Mode", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "FILTER_CUTOFF", "Filter Cutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 1850.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "FILTER_RESO", "Filter Resonance", juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.48f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "FILTER_MODE", "Filter Mode", juce::StringArray{"Lowpass 24dB", "Bandpass", "Highpass"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TAPE_TIME", "Tape Delay Time", juce::NormalisableRange<float>(0.05f, 1.2f, 0.01f), 0.36f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TAPE_FEEDBACK", "Tape Feedback", juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.46f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TAPE_FLUTTER", "Tape Wow & Flutter", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.32f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TAPE_MIX", "Tape Wet Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.38f));

    // --- KyotoSpxrit Parameters ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "VINYL_CRACKLE", "Vinyl Crackle & Dust", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TAPE_WARP", "Tape Warp Drift", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.28f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "LOFI_BITS", "Lofi Bits Resolution", juce::NormalisableRange<float>(4.0f, 16.0f, 0.5f), 16.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "SHIMMER_MIX", "Kyoto Shrine Shimmer", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    // --- Master Parameters ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "STEREO_WIDTH", "Stereo Width", juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "IRON_DRIVE", "Transformer Iron Drive", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "CEILING_DB", "True-Peak Ceiling", juce::NormalisableRange<float>(-6.0f, 0.0f, 0.1f), -0.2f));

    // --- Modeled Modules (the original 1176 + Pultec, now off by default: the rack has far more) ---
    params.push_back(std::make_unique<juce::AudioParameterBool>("FET_1176_ON", "1176 Limiter Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("FET_1176_IN", "1176 Input", 0.0f, 10.0f, 6.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("FET_1176_OUT", "1176 Output", 0.0f, 10.0f, 4.8f));

    params.push_back(std::make_unique<juce::AudioParameterBool>("PULTEC_ON", "Pultec EQ Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("PULTEC_BOOST", "Pultec 60Hz Boost", 0.0f, 10.0f, 4.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("PULTEC_ATTEN", "Pultec 60Hz Atten", 0.0f, 10.0f, 3.8f));

    // --- new: global, 16-slot machine rack, synth, drums ---
    BabyGirl::addParams(params);
    return { params.begin(), params.end() };
}

void BabyGirlAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels = 2;

    hushEngine.prepare(spec);
    kyotoEngine.prepare(spec);
    masterEngine.prepare(spec);
    modulesEngine.prepare(sampleRate);
    for (int s = 0; s < kRackSlots; ++s)
    {
        (*machines)[(size_t)s].prepare(sampleRate);
        (*machines)[(size_t)s].select((int)rMach[s]->load());
    }
    synth.prepare(sampleRate);
    drums.prepare(sampleRate);
    loudness.prepare(sampleRate);
    keyboardState.reset();
    freePpq = 0.0; expectedNextPpq = -1.0;
}

void BabyGirlAudioProcessor::releaseResources() {}

bool BabyGirlAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in.isDisabled();
}

bg::SynthParams BabyGirlAudioProcessor::readSynthParams()
{
    bg::SynthParams p;
    p.osc1 = (int)raw("SYN_OSC1")->load(); p.osc2 = (int)raw("SYN_OSC2")->load();
    p.osc2Semi = raw("SYN_O2SEMI")->load(); p.osc2Fine = raw("SYN_O2FINE")->load();
    p.mix = raw("SYN_MIX")->load(); p.sub = raw("SYN_SUB")->load(); p.noise = raw("SYN_NOISE")->load();
    p.uni = (int)raw("SYN_UNI")->load(); p.uniDetune = raw("SYN_UDET")->load();
    p.morph = raw("SYN_MORPH")->load(); p.fm = raw("SYN_FM")->load(); p.pwm = raw("SYN_PWM")->load();
    p.fType = (int)raw("SYN_FTYPE")->load(); p.cutoff = raw("SYN_CUT")->load(); p.res = raw("SYN_RES")->load();
    p.drive = raw("SYN_DRIVE")->load(); p.fEnv = raw("SYN_FENV")->load(); p.keyTrack = raw("SYN_KEY")->load();
    p.a = raw("SYN_A")->load(); p.d = raw("SYN_D")->load(); p.s = raw("SYN_S")->load(); p.r = raw("SYN_R")->load();
    p.fa = raw("SYN_FA")->load(); p.fd = raw("SYN_FD")->load(); p.fs = raw("SYN_FS")->load(); p.fr = raw("SYN_FR")->load();
    p.lfoRate = raw("SYN_LFORATE")->load(); p.lfoShape = (int)raw("SYN_LFOSHAPE")->load();
    p.lfoPitch = raw("SYN_LFOPITCH")->load(); p.lfoFilter = raw("SYN_LFOFILT")->load(); p.lfoAmp = raw("SYN_LFOAMP")->load();
    p.glide = raw("SYN_GLIDE")->load(); p.level = raw("SYN_LEVEL")->load(); p.velSens = raw("SYN_VEL")->load();
    return p;
}

void BabyGirlAudioProcessor::runRack(float* l, float* r, int n, double bpm)
{
    for (int s = 0; s < kRackSlots; ++s)
    {
        auto& m = (*machines)[(size_t)s];
        const int want = juce::jlimit(0, bg::kNumMachines - 1, (int)rMach[s]->load());
        if (want != m.current()) m.select(want);
        slotMachine[s].store(want, std::memory_order_relaxed);
        if (want == 0 || rOn[s]->load() < 0.5f) { slotMeter[s].store(0.0f, std::memory_order_relaxed); continue; }
        float p[6];
        for (int k = 0; k < 6; ++k) p[k] = rP[s][k]->load();
        m.setParams(p);
        m.setBpm(bpm);
        m.process(l, r, n, rMix[s]->load());
        slotMeter[s].store(m.meter.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
}

void BabyGirlAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n <= 0) return;

    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, n);
    if (getTotalNumInputChannels() == 0) buffer.clear();

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    // ---- input trim (audio input only; instruments are added afterwards) ----
    const float inG = juce::Decibels::decibelsToGain(raw("IN_TRIM")->load());
    if (inG != 1.0f) buffer.applyGain(inG);

    // ---- transport / tempo ----
    double bpm = 120.0, hostPpq = 0.0; bool hostPlaying = false, haveHost = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) { hostPpq = *p; haveHost = true; }
            hostPlaying = pos->getIsPlaying();
        }

    // ---- gather timed events: MIDI + drum sequencer ----
    keyboardState.processNextMidiBuffer(midi, 0, n, true);
    struct Ev { int pos; int kind; int a; float b; };
    enum { SynOn, SynOff, DrumOn, Bend, Mod, Sustain, AllOff };
    Ev evs[640]; int ne = 0;
    auto push = [&](int pos, int kind, int a, float b) { if (ne < 640) evs[ne++] = { juce::jlimit(0, n - 1, pos), kind, a, b }; };

    const bool synOn = raw("SYN_ON")->load() > 0.5f, drmOn = raw("DRM_ON")->load() > 0.5f;
    const int drmMap = (int)raw("DRM_MAP")->load();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage(); const int pos = meta.samplePosition;
        if (m.isNoteOn() || m.isNoteOff())
        {
            const int note = m.getNoteNumber(), ch = m.getChannel();
            const bool inPad = note >= bg::kDrumBaseNote && note < bg::kDrumBaseNote + bg::kPads;
            bool toDrum = false;                       // routing: who gets this note?
            if (drmMap == 0) toDrum = (ch == 10);
            else if (drmMap == 1) toDrum = inPad;
            else toDrum = (ch == 10) || inPad;
            if (toDrum)
            {
                if (m.isNoteOn() && inPad) push(pos, DrumOn, note - bg::kDrumBaseNote, m.getFloatVelocity());
            }
            else if (m.isNoteOn()) push(pos, SynOn, note, m.getFloatVelocity());
            else push(pos, SynOff, note, 0.0f);
        }
        else if (m.isPitchWheel()) push(pos, Bend, 0, ((float)m.getPitchWheelValue() - 8192.0f) / 8192.0f * 2.0f);
        else if (m.isController() && m.getControllerNumber() == 1) push(pos, Mod, 0, (float)m.getControllerValue() / 127.0f);
        else if (m.isController() && m.getControllerNumber() == 64) push(pos, Sustain, m.getControllerValue() >= 64 ? 1 : 0, 0.0f);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) push(pos, AllOff, 0, 0.0f);
    }
    midi.clear();

    // sequencer
    const int seqMode = (int)raw("DRM_SEQ")->load();
    if (drmOn && seqMode != 0)
    {
        const double swing = raw("DRM_SWING")->load();
        double useBpm = bpm, start = 0.0; bool run = false;
        if (seqMode == 1 && haveHost && hostPlaying) { start = hostPpq; run = true; }
        else if (seqMode == 2) { useBpm = raw("DRM_BPM")->load(); start = freePpq; run = true; }
        if (run)
        {
            if (expectedNextPpq >= 0.0 && std::abs(start - expectedNextPpq) < 1.0e-3) start = expectedNextPpq;   // kill float drift between blocks
            bg::DrumSequencer::Ev sev[256];
            const int c = sequencer.collect(start, useBpm, n, sampleRateHz, swing, sev, 256);
            for (int i = 0; i < c; ++i) push(sev[i].offset, DrumOn, sev[i].pad, sev[i].vel);
            expectedNextPpq = start + (double)n * (useBpm / 60.0 / sampleRateHz);
            if (seqMode == 2) freePpq = expectedNextPpq;
            currentStep.store((int)(((long)std::floor(start * 4.0) % 16 + 16) % 16));
        }
        else { expectedNextPpq = -1.0; currentStep.store(-1); }
    }
    else { currentStep.store(-1); expectedNextPpq = -1.0; }

    // stable insertion sort by position
    for (int i = 1; i < ne; ++i) { const Ev e = evs[i]; int j = i - 1; while (j >= 0 && evs[j].pos > e.pos) { evs[j + 1] = evs[j]; --j; } evs[j + 1] = e; }

    // ---- instruments ----
    synth.setParams(readSynthParams());
    for (int p = 0; p < bg::kPads; ++p) drums.setPad(p, { dTune[p]->load(), dDec[p]->load(), dLvl[p]->load(), dPan[p]->load() });
    drums.setMaster(raw("DRM_LEVEL")->load());

    int cur = 0;
    auto render = [&](int start, int len)
    {
        if (len <= 0) return;
        if (synOn) synth.process(L + start, R + start, len);
        if (drmOn) drums.process(L + start, R + start, len);
    };
    for (int i = 0; i < ne; ++i)
    {
        render(cur, evs[i].pos - cur); cur = evs[i].pos;
        const auto& e = evs[i];
        switch (e.kind)
        {
            case SynOn: if (synOn) synth.noteOn(e.a, e.b); break;
            case SynOff: synth.noteOff(e.a); break;
            case DrumOn: if (drmOn) drums.trigger(e.a, e.b); break;
            case Bend: synth.pitchBend(e.b); break;
            case Mod: synth.modWheel(e.b); break;
            case Sustain: synth.sustain(e.a != 0); break;
            case AllOff: synth.allOff(); break;
            default: break;
        }
    }
    render(cur, n - cur);

    // ---- effects chain: [rack?] Hush -> [rack?] Kyoto -> modules -> Master ----
    const bool rackPre = (int)raw("RACK_POS")->load() == 0;
    if (rackPre) runRack(L, R, n, bpm);

    if (raw("HUSH_ON")->load() > 0.5f)
    {
        hushEngine.updateFilter(raw("FILTER_CUTOFF")->load(), raw("FILTER_RESO")->load(), (int)raw("FILTER_MODE")->load());
        hushEngine.process(buffer, raw("TUBE_DRIVE")->load(), raw("TUBE_BIAS")->load(), raw("TUBE_PENTODE")->load() > 0.5f,
                           raw("TAPE_TIME")->load(), raw("TAPE_FEEDBACK")->load(), raw("TAPE_FLUTTER")->load(), raw("TAPE_MIX")->load());
    }
    if (!rackPre) { /* rack runs after Kyoto, below */ }

    if (raw("KYOTO_ON")->load() > 0.5f)
        kyotoEngine.process(buffer, raw("VINYL_CRACKLE")->load(), raw("TAPE_WARP")->load(), raw("LOFI_BITS")->load(), raw("SHIMMER_MIX")->load());

    if (!rackPre) runRack(L, R, n, bpm);

    if (raw("FET_1176_ON")->load() > 0.5f)
    {
        const float inGain = raw("FET_1176_IN")->load(), outGain = raw("FET_1176_OUT")->load();
        for (int i = 0; i < n; ++i)
        {
            modulesEngine.process1176(L[i], inGain, outGain, 4.0f, fet1176Env);
            modulesEngine.process1176(R[i], inGain, outGain, 4.0f, fet1176Env);
        }
    }
    if (raw("PULTEC_ON")->load() > 0.5f)
    {
        const float pBoost = raw("PULTEC_BOOST")->load(), pAtten = raw("PULTEC_ATTEN")->load();
        for (int i = 0; i < n; ++i)
        {
            modulesEngine.processPultec(L[i], pBoost, pAtten, 5.0f);
            modulesEngine.processPultec(R[i], pBoost, pAtten, 5.0f);
        }
    }

    float legacyLufs = currentLufs, legacyPeak = currentPeak;
    if (raw("MASTER_ON")->load() > 0.5f)
        masterEngine.process(buffer, raw("STEREO_WIDTH")->load(), raw("IRON_DRIVE")->load(), raw("CEILING_DB")->load(), -18.0f, legacyLufs, legacyPeak);

    const float outG = juce::Decibels::decibelsToGain(raw("OUT_TRIM")->load());
    if (outG != 1.0f) buffer.applyGain(outG);

    // final safety: never hand the host NaN/inf
    for (int i = 0; i < n; ++i)
    {
        if (!std::isfinite(L[i])) L[i] = 0.0f;
        if (!std::isfinite(R[i])) R[i] = 0.0f;
    }

    // ---- metering / visualiser feed ----
    loudness.process(L, R, n);
    scope.push(L, R, n);
    currentLufs = loudness.momentary.load();
    currentPeak = juce::Decibels::decibelsToGain(loudness.truePeakDb.load());

    const double used = juce::Time::getMillisecondCounterHiRes() - t0, avail = 1000.0 * n / sampleRateHz;
    cpuLoad.store(cpuLoad.load() * 0.9f + 0.1f * (float)(used / std::max(avail, 1e-6)));
}

juce::AudioProcessorEditor* BabyGirlAudioProcessor::createEditor()
{
    return new BabyGirlAudioProcessorEditor(*this);
}

// ------------------------------------------------------------------ state / presets
static juce::String gridToHex(const bg::DrumSequencer& s)
{
    juce::String h;
    for (int p = 0; p < bg::kPads; ++p) for (int k = 0; k < bg::kSteps; ++k) h << juce::String::toHexString((int)s.grid[p][k]).paddedLeft('0', 2);
    return h;
}
static void hexToGrid(const juce::String& h, bg::DrumSequencer& s)
{
    if (h.length() < bg::kPads * bg::kSteps * 2) return;
    for (int p = 0; p < bg::kPads; ++p) for (int k = 0; k < bg::kSteps; ++k)
        s.grid[p][k] = (uint8_t)juce::jlimit(0, 127, h.substring((p * bg::kSteps + k) * 2, (p * bg::kSteps + k) * 2 + 2).getHexValue32());
}

void BabyGirlAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("DRUMGRID", gridToHex(sequencer), nullptr);
    state.setProperty("BG_VERSION", "2.0.0", nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BabyGirlAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto vt = juce::ValueTree::fromXml(*xmlState);
        hexToGrid(vt.getProperty("DRUMGRID").toString(), sequencer);
        apvts.replaceState(vt);
    }
}

void BabyGirlAudioProcessor::resetAllToDefault()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p)) { rp->beginChangeGesture(); rp->setValueNotifyingHost(rp->getDefaultValue()); rp->endChangeGesture(); }
}

void BabyGirlAudioProcessor::applyPreset(const DreamPreset& preset)
{
    auto setReal = [&](const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    resetAllToDefault();

    if (preset.legacyOnly)
    {
        setReal("TUBE_DRIVE", preset.tubeDrive); setReal("FILTER_CUTOFF", preset.filterCutoff); setReal("FILTER_RESO", preset.filterReso);
        setReal("TAPE_TIME", preset.tapeTime); setReal("TAPE_FLUTTER", preset.tapeFlutter); setReal("VINYL_CRACKLE", preset.vinylCrackle);
        setReal("TAPE_WARP", preset.tapeWarp); setReal("SHIMMER_MIX", preset.shimmerMix); setReal("STEREO_WIDTH", preset.stereoWidth);
        setReal("IRON_DRIVE", preset.ironDrive);
        setReal("HUSH_ON", 1.0f); setReal("KYOTO_ON", 1.0f); setReal("MASTER_ON", 1.0f);
        return;
    }
    for (const auto& s : preset.settings)
    {
        if (s.id == "DRM_PATTERN") { sequencer.loadPattern(s.text.getIntValue()); continue; }
        if (s.id == "DRUMGRID") { hexToGrid(s.text, sequencer); continue; }
        auto* p = apvts.getParameter(s.id);
        if (p == nullptr) continue;
        if (s.text.isNotEmpty())
        {
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(p))
            {
                const int idx = c->choices.indexOf(s.text);
                if (idx >= 0) p->setValueNotifyingHost(p->convertTo0to1((float)idx));
            }
        }
        else p->setValueNotifyingHost(p->convertTo0to1(s.value));
    }
}

DreamPreset BabyGirlAudioProcessor::captureCurrent(const juce::String& name, const juce::String& author) const
{
    DreamPreset d;
    d.id = "user-" + juce::String(juce::Time::currentTimeMillis());
    d.name = name; d.author = author; d.category = "Community"; d.tags = "rack, community"; d.legacyOnly = false;
    auto get = [&](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    d.tubeDrive = get("TUBE_DRIVE"); d.filterCutoff = get("FILTER_CUTOFF"); d.filterReso = get("FILTER_RESO"); d.tapeTime = get("TAPE_TIME");
    d.tapeFlutter = get("TAPE_FLUTTER"); d.vinylCrackle = get("VINYL_CRACKLE"); d.tapeWarp = get("TAPE_WARP"); d.shimmerMix = get("SHIMMER_MIX");
    d.stereoWidth = get("STEREO_WIDTH"); d.ironDrive = get("IRON_DRIVE");
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p);
        if (rp == nullptr) continue;
        if (std::abs(rp->getValue() - rp->getDefaultValue()) < 1.0e-5f) continue;   // only non-default values travel
        PresetSetting s; s.id = rp->paramID;
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(p); c != nullptr && s.id.endsWith("_MACH")) s.text = c->getCurrentChoiceName();
        else s.value = rp->convertFrom0to1(rp->getValue());
        d.settings.push_back(s);
    }
    d.settings.push_back({ "DRUMGRID", 0.0f, gridToHex(sequencer) });
    return d;
}

void BabyGirlAudioProcessor::clearRack()
{
    for (int s = 0; s < kRackSlots; ++s)
        if (auto* p = apvts.getParameter(rackId(s, "MACH"))) p->setValueNotifyingHost(0.0f);
}

void BabyGirlAudioProcessor::randomizeRack(bool /*keepLocked*/, juce::Random& rng)
{
    // Pick 3..6 random machines (never the empty slot), randomise their knobs, set a sensible mix.
    clearRack();
    const int count = 3 + rng.nextInt(4);
    for (int s = 0; s < count; ++s)
    {
        const int m = 1 + rng.nextInt(bg::kNumMachines - 1);
        const auto& info = bg::kMachines[m];
        if (auto* p = apvts.getParameter(rackId(s, "MACH"))) p->setValueNotifyingHost(p->convertTo0to1((float)m));
        if (auto* p = apvts.getParameter(rackId(s, "ON"))) p->setValueNotifyingHost(1.0f);
        if (auto* p = apvts.getParameter(rackId(s, "MIX"))) p->setValueNotifyingHost(defaultMixForFamily(info.fam));
        for (int k = 0; k < 6; ++k)
            if (auto* p = apvts.getParameter(rackId(s, "P" + juce::String(k + 1))))
                p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, info.def[k] + (rng.nextFloat() - 0.5f) * 0.6f));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BabyGirlAudioProcessor();
}
