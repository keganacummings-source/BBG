#pragma once
#include <juce_dsp/juce_dsp.h>
#include <random>
#include <cmath>
#include <algorithm>

namespace BabyGirl
{
    /**
     * KyotoSpxrit DSP Engine
     * Incorporates Japanese lofi cassette degradation, vinyl dust/crackle,
     * tape warp, sample-rate crunch, and shrine shimmer reverb.
     */
    class KyotoDsp
    {
    public:
        KyotoDsp() : dist(0.0f, 1.0f) {}

        void prepare(const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            shimmerReverb.prepare(spec);   // required: sets the reverb's sample rate
            reverbParams.roomSize = 0.85f;
            reverbParams.damping = 0.35f;
            reverbParams.wetLevel = 0.4f;
            reverbParams.dryLevel = 1.0f;
            reverbParams.width = 1.0f;
            shimmerReverb.setParameters(reverbParams);
            shimmerReverb.reset();
        }

        void process(juce::AudioBuffer<float>& buffer, float vinylCrackle, float tapeWarp,
                     float lofiBits, float shimmerMix)
        {
            const int numSamples = buffer.getNumSamples();
            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

            // 1. Vinyl Dust & Random Pops
            if (vinylCrackle > 0.01f)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    if (dist(rng) < (vinylCrackle * 0.008f))
                    {
                        float pop = (dist(rng) * 2.0f - 1.0f) * vinylCrackle * 0.35f;
                        left[i] += pop;
                        right[i] += pop;
                    }
                    float hiss = (dist(rng) * 2.0f - 1.0f) * vinylCrackle * 0.015f;
                    left[i] += hiss;
                    right[i] += hiss;
                }
            }

            // 2. Lofi Bitcrush & Sample Rate Reduction
            if (lofiBits < 15.5f)
            {
                float steps = std::pow(2.0f, std::clamp(lofiBits, 4.0f, 16.0f));
                for (int i = 0; i < numSamples; ++i)
                {
                    left[i] = std::round(left[i] * steps) / steps;
                    right[i] = std::round(right[i] * steps) / steps;
                }
            }

            // 3. Tape Warp Drift
            if (tapeWarp > 0.01f)
            {
                warpPhase += 0.005f * (1.0f + tapeWarp * 2.0f);
                if (warpPhase > juce::MathConstants<float>::twoPi)
                    warpPhase -= juce::MathConstants<float>::twoPi;
                float warpGain = 1.0f - (std::sin(warpPhase) * tapeWarp * 0.18f);
                for (int i = 0; i < numSamples; ++i)
                {
                    left[i] *= warpGain;
                    right[i] *= warpGain;
                }
            }

            // 4. Kyoto Shrine Shimmer Reverb
            if (shimmerMix > 0.01f)
            {
                // the Shimmer knob now actually sets the wet amount (it used to be an on/off switch)
                reverbParams.wetLevel = std::clamp(shimmerMix, 0.0f, 1.0f) * 0.55f;
                reverbParams.dryLevel = 1.0f - std::clamp(shimmerMix, 0.0f, 1.0f) * 0.35f;
                shimmerReverb.setParameters(reverbParams);
                juce::dsp::AudioBlock<float> block(buffer);
                juce::dsp::ProcessContextReplacing<float> context(block);
                shimmerReverb.process(context);
            }
        }

    private:
        double sampleRate = 48000.0;
        juce::dsp::Reverb shimmerReverb;
        juce::dsp::Reverb::Parameters reverbParams;
        std::mt19937 rng{1337};
        std::uniform_real_distribution<float> dist;
        float warpPhase = 0.0f;
    };
}
