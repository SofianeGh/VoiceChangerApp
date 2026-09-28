// "IEffect.h"
#pragma once
#include <cstdint>

class IEffect {
public:
    virtual ~IEffect() = default;

    // Fonction critique appelée depuis le thread audio
    // buffer : tableau audio entrelacé (L, R, L, R...)
    virtual void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) = 0;

    // Modification des paramètres en temps réel, thread-safe (lock-free)
    virtual void SetParameter(int paramId, float value) = 0;
};