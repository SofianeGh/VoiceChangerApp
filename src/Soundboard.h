// "Soundboard.h"
#pragma once
#include <cstdint>
#include <atomic>
#include <vector>

// Stub simple pour le soundboard (lecture en RAM)
class Soundboard {
public:
    Soundboard() : m_isPlaying(false), m_readIndex(0) {}

    // Charge un fichier audio en mémoire (à appeler depuis l'UI)
    void LoadSample(const std::vector<float>& audioData) {
        m_audioData = audioData;
        m_readIndex.store(0);
    }

    void Play() { m_isPlaying.store(true); }
    void Stop() { m_isPlaying.store(false); m_readIndex.store(0); }

    void MixInto(float* buffer, uint32_t frameCount, uint32_t channels) {
        if (!m_isPlaying.load(std::memory_order_relaxed) || m_audioData.empty()) return;

        size_t currentIdx = m_readIndex.load(std::memory_order_relaxed);
        
        for (uint32_t i = 0; i < frameCount * channels; ++i) {
            if (currentIdx < m_audioData.size()) {
                // Mixage additif simple
                buffer[i] += m_audioData[currentIdx];
                currentIdx++;
            } else {
                m_isPlaying.store(false);
                break;
            }
        }
        
        m_readIndex.store(currentIdx, std::memory_order_relaxed);
    }

private:
    std::vector<float> m_audioData;
    std::atomic<bool> m_isPlaying;
    std::atomic<size_t> m_readIndex;
};