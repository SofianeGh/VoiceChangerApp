// "EffectChain.cpp"
#include "EffectChain.h"

void EffectChain::AddEffect(std::unique_ptr<IEffect> effect) {
    // Utilisation d'un lock classique ici car l'ajout se fait depuis le thread UI principal
    std::lock_guard<std::mutex> lock(m_mutex);
    m_effects.push_back(std::move(effect));
}

IEffect* EffectChain::GetEffect(size_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index < m_effects.size()) {
        return m_effects[index].get();
    }
    return nullptr;
}

void EffectChain::Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
    // Try_lock est fondamental ici (Real-Time Safe) : 
    // Si l'UI est en train d'ajouter un effet, on saute le traitement pour cette frame (pas de clic audio)
    if (m_mutex.try_lock()) {
        for (auto& effect : m_effects) {
            effect->Process(buffer, frameCount, channels, sampleRate);
        }
        m_mutex.unlock();
    }
}