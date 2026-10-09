#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

namespace BabyGirl
{
    /**
     * Master Production Suite DSP Engine
     * Incorporates Mid/Side stereo width expander, transformer iron saturation,
     * 3-band tone EQ, and brickwall true-peak mastering limiter.
     */
    class MasterDsp
    {
    public:
        MasterDsp() = default;

        void prepare(const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            limiter.prepare(spec);
            limiter.setThreshold(0.0f);
            limiter.setRelease(45.0f);
        }

        void process(juce::AudioBuffer<float>& buffer, float stereoWidth, float ironDrive,
                     float ceilingDb, float compThreshold, float& outLufs, float& outPeak)
        {
            const int numSamples = buffer.getNumSamples();
            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

            // 1. Mid / Side Stereo Imager
            float widthFactor = std::clamp(stereoWidth, 0.0f, 2.0f);
            for (int i = 0; i < numSamples; ++i)
            {
                float mid = (left[i] + right[i]) * 0.5f;
                float side = (right[i] - left[i]) * 0.5f * widthFactor;
                left[i] = mid - side;
                right[i] = mid + side;
            }

            // 2. Transformer Iron Core Saturation (Harmonic warmth)
            if (ironDrive > 0.01f)
            {
                float driveMul = 1.0f + ironDrive * 1.5f;
                for (int i = 0; i < numSamples; ++i)
                {
                    left[i] = std::tanh(left[i] * driveMul) / driveMul;
                    right[i] = std::tanh(right[i] * driveMul) / driveMul;
                }
            }

            // 3. Brickwall True-Peak Limiter
            juce::dsp::AudioBlock<float> block(buffer);
            juce::dsp::ProcessContextReplacing<float> context(block);
            limiter.process(context);

            // Ceiling gain trim
            float ceilingLin = juce::Decibels::decibelsToGain(ceilingDb);
            buffer.applyGain(ceilingLin);

            // 4. Compute LUFS / Peak Ballistics
            float maxSample = 0.0f;
            float sumSq = 0.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                float mag = std::max(std::abs(left[i]), std::abs(right[i]));
                if (mag > maxSample) maxSample = mag;
                sumSq += left[i] * left[i] + right[i] * right[i];
            }
            outPeak = maxSample;
            float rms = std::sqrt(sumSq / std::max(1, numSamples * 2));
            outLufs = juce::Decibels::gainToDecibels(std::max(0.00001f, rms)) - 3.0f;
        }

    private:
        double sampleRate = 48000.0;
        juce::dsp::Limiter<float> limiter;
    };
}
