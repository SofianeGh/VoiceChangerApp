// "RobotEffect.h"
#pragma once
#include "IEffect.h"
#include <cmath>
#include <atomic>

class RobotEffect : public IEffect {
public:
    RobotEffect() : m_frequency(50.0f), m_phase(0.0f), m_mix(1.0f) {}

    void SetParameter(int paramId, float value) override {
        // paramId 0 = Frequence (Hz), paramId 1 = Mix (0.0 à 1.0)
        if (paramId == 0) m_frequency.store(value, std::memory_order_relaxed);
        else if (paramId == 1) m_mix.store(value, std::memory_order_relaxed);
    }

    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) override {
        float freq = m_frequency.load(std::memory_order_relaxed);
        float mix = m_mix.load(std::memory_order_relaxed);
        float phaseIncrement = (2.0f * 3.14159265359f * freq) / sampleRate;

        for (uint32_t i = 0; i < frameCount; ++i) {
            float modulator = std::sin(m_phase);
            
            for (uint32_t c = 0; c < channels; ++c) {
                uint32_t index = i * channels + c;
                float dry = buffer[index];
                float wet = dry * modulator;
                // Interpolation linéaire entre le signal sec et le signal traité
                buffer[index] = dry * (1.0f - mix) + wet * mix;
            }

            m_phase += phaseIncrement;
            if (m_phase >= 2.0f * 3.14159265359f) {
                m_phase -= 2.0f * 3.14159265359f;
            }
        }
    }

private:
    std::atomic<float> m_frequency;
    std::atomic<float> m_mix;
    float m_phase;
};