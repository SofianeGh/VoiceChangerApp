// "EffectChain.h"
#pragma once
#include "IEffect.h"
#include <vector>
#include <memory>
#include <mutex>

class EffectChain {
public:
    EffectChain() = default;
    ~EffectChain() = default;

    void AddEffect(std::unique_ptr<IEffect> effect);
    IEffect* GetEffect(size_t index);
    
    // Fonction appelée par le callback audio
    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate);

private:
    std::vector<std::unique_ptr<IEffect>> m_effects;
    std::mutex m_mutex;
};