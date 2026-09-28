// "PitchShiftEffect.h"
#pragma once
#include "IEffect.h"
#include <SoundTouch.h> 
#include <atomic>
#include <algorithm>

class PitchShiftEffect : public IEffect {
public:
    PitchShiftEffect() {
        m_soundTouch.setChannels(2);
        m_soundTouch.setSampleRate(48000);
        m_soundTouch.setPitchSemiTones(0.0f);
        m_soundTouch.setSetting(SETTING_USE_QUICKSEEK, 1);
        m_soundTouch.setSetting(SETTING_USE_AA_FILTER, 1);
    }

    void SetParameter(int paramId, float value) override {
        if (paramId == 0) { // Demi-tons (-12 à +12)
            m_pitch.store(value, std::memory_order_relaxed);
            m_needsUpdate.store(true, std::memory_order_relaxed);
        }
    }

    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) override {
        if (m_needsUpdate.exchange(false, std::memory_order_relaxed)) {
            m_soundTouch.setPitchSemiTones(m_pitch.load(std::memory_order_relaxed));
        }

        m_soundTouch.putSamples(buffer, frameCount);
        uint32_t samplesReceived = m_soundTouch.receiveSamples(buffer, frameCount);
        
        // Zéro-padding si SoundTouch accumule des échantillons (latence algorithmique)
        if (samplesReceived < frameCount) {
            std::fill(buffer + (samplesReceived * channels), buffer + (frameCount * channels), 0.0f);
        }
    }

private:
    soundtouch::SoundTouch m_soundTouch;
    std::atomic<float> m_pitch{0.0f};
    std::atomic<bool> m_needsUpdate{false};
};