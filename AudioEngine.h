// "AudioEngine.h"
#pragma once
#include <atomic>
#include <vector>
#include <memory>
#include <string>
#include "miniaudio.h"

class EffectChain;
class Soundboard;

struct AudioEngineConfig {
    uint32_t sampleRate = 48000;
    uint32_t bufferSize = 256; 
    uint32_t channels = 2;     
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool Initialize(const std::string& inputDeviceId, const std::string& outputDeviceId, AudioEngineConfig config);
    bool Start();
    void Stop();

    std::shared_ptr<EffectChain> GetEffectChain() const { return m_effectChain; }
    std::shared_ptr<Soundboard> GetSoundboard() const { return m_soundboard; }

private:
    static void DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void ProcessAudio(const float* input, float* output, uint32_t frameCount);

    ma_context m_context;
    ma_device m_device;
    AudioEngineConfig m_config;
    bool m_isInitialized = false;

    std::shared_ptr<EffectChain> m_effectChain;
    std::shared_ptr<Soundboard> m_soundboard;
    std::vector<float> m_mixBuffer; 
};