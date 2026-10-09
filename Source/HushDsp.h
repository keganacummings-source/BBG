#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

namespace BabyGirl
{
    /**
     * Hush DSP Engine
     * Incorporates analog ladder filtering, vacuum tube waveshaping,
     * dual-core synthesis, and reel-to-reel tape magnetic delay.
     */
    class HushDsp
    {
    public:
        HushDsp() = default;

        void prepare(const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            tapeDelayL.setSize(2, (int)(spec.sampleRate * 2.5));
            tapeDelayL.clear();
            writePos = 0;

            // LadderFilter MUST be prepared (it sizes per-channel state); one instance filters both channels.
            filterL.prepare(spec);
            filterL.reset();
            updateFilter(1850.0f, 0.48f, 0);
        }

        void updateFilter(float cutoffHz, float resonance, int mode)
        {
            auto fMode = mode == 1 ? juce::dsp::LadderFilterMode::BPF12 :
                         mode == 2 ? juce::dsp::LadderFilterMode::HPF24 :
                                     juce::dsp::LadderFilterMode::LPF24;
            filterL.setMode(fMode);
            filterL.setCutoffFrequencyHz(std::clamp(cutoffHz, 20.0f, 20000.0f));
            filterL.setResonance(std::clamp(resonance, 0.0f, 0.95f));
        }

        // Thermionic vacuum tube transfer curve (12AX7 triode & EL34 pentode)
        static float processTubeSample(float in, float drive, float bias, bool pentode)
        {
            float x = in + (bias - 0.5f) * 0.4f;
            float k = std::max(1.0f, drive * 3.0f);
            if (!pentode)
            {
                // Triode asymmetric soft clipping
                if (x < 0.0f)
                    return -std::tanh(std::abs(x) * k * 0.8f) / std::tanh(k * 0.8f);
                else
                    return std::tanh(x * k * 1.4f) / std::tanh(k * 1.4f);
            }
            else
            {
                // Pentode harder knee saturation
                float sign = (x >= 0.0f) ? 1.0f : -1.0f;
                return sign * (1.0f - std::exp(-std::abs(x) * k * 1.5f));
            }
        }

        void process(juce::AudioBuffer<float>& buffer, float tubeDrive, float tubeBias, bool pentode,
                     float tapeTime, float tapeFeedback, float tapeFlutter, float tapeMix)
        {
            const int numSamples = buffer.getNumSamples();
            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

            // 1. Vacuum Tube Preamp Saturation
            for (int i = 0; i < numSamples; ++i)
            {
                left[i] = processTubeSample(left[i], tubeDrive, tubeBias, pentode);
                if (left != right)
                    right[i] = processTubeSample(right[i], tubeDrive, tubeBias, pentode);
            }

            // 2. 4-Pole Ladder Resonant Filter
            juce::dsp::AudioBlock<float> block(buffer);
            juce::dsp::ProcessContextReplacing<float> context(block);
            filterL.process(context);

            // 3. Reel-To-Reel Magnetic Tape Delay with Wow/Flutter
            int maxDelaySamples = (int)(sampleRate * 2.5);
            float flutterMod = std::sin(flutterPhase) * (tapeFlutter * 40.0f);
            flutterPhase += 0.02f;
            if (flutterPhase > juce::MathConstants<float>::twoPi)
                flutterPhase -= juce::MathConstants<float>::twoPi;

            int delaySamples = std::clamp((int)(tapeTime * sampleRate + flutterMod), 10, maxDelaySamples - 1);

            for (int i = 0; i < numSamples; ++i)
            {
                int readPos = (writePos - delaySamples + maxDelaySamples) % maxDelaySamples;
                float delayedL = tapeDelayL.getSample(0, readPos);
                float delayedR = tapeDelayL.getSample(1, readPos);

                tapeDelayL.setSample(0, writePos, left[i] + delayedL * tapeFeedback);
                tapeDelayL.setSample(1, writePos, right[i] + delayedR * tapeFeedback);
                writePos = (writePos + 1) % maxDelaySamples;

                left[i] = left[i] * (1.0f - tapeMix) + delayedL * tapeMix;
                right[i] = right[i] * (1.0f - tapeMix) + delayedR * tapeMix;
            }
        }

    private:
        double sampleRate = 48000.0;
        juce::dsp::LadderFilter<float> filterL;
        juce::AudioBuffer<float> tapeDelayL;
        int writePos = 0;
        float flutterPhase = 0.0f;
    };
}
