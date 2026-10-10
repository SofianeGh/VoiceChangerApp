// "AudioEngine.cpp"
#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.h"
#include "EffectChain.h"
#include "Soundboard.h"
#include <iostream>
#include <algorithm>

#ifdef _WIN32
#include <avrt.h>
#pragma comment(lib, "Avrt.lib")
#endif

std::unique_ptr<AudioEngine> g_AudioEngine;

AudioEngine::AudioEngine()
    : m_effectChain(std::make_shared<EffectChain>()),
      m_soundboard(std::make_shared<Soundboard>()) 
{
}

AudioEngine::~AudioEngine() {
    Stop();
    if (m_isInitialized) {
        ma_device_uninit(&m_device);
        ma_context_uninit(&m_context);
    }
}

bool AudioEngine::Initialize(const std::string& inputDeviceId, const std::string& outputDeviceId, AudioEngineConfig config) {
    m_config = config;
    m_mixBuffer.resize(m_config.bufferSize * m_config.channels, 0.0f);

    if (ma_context_init(NULL, 0, NULL, &m_context) != MA_SUCCESS) {
        return false;
    }

    // Résolution des noms de périphériques en identifiants miniaudio
    ma_device_info* playbackInfos = NULL;
    ma_device_info* captureInfos = NULL;
    ma_uint32 playbackCount = 0;
    ma_uint32 captureCount = 0;
    if (ma_context_get_devices(&m_context, &playbackInfos, &playbackCount, &captureInfos, &captureCount) != MA_SUCCESS) {
        ma_context_uninit(&m_context);
        return false;
    }

    auto findDevice = [](const std::string& name, const ma_device_info* infos, ma_uint32 count, ma_device_id& outId) {
        for (ma_uint32 i = 0; i < count; ++i) {
            if (name == infos[i].name) {
                outId = infos[i].id;
                return true;
            }
        }
        return false;
    };

    if ((!inputDeviceId.empty() && !findDevice(inputDeviceId, captureInfos, captureCount, m_captureId)) ||
        (!outputDeviceId.empty() && !findDevice(outputDeviceId, playbackInfos, playbackCount, m_playbackId))) {
        ma_context_uninit(&m_context);
        return false;
    }

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_duplex);

    deviceConfig.capture.pDeviceID = inputDeviceId.empty() ? NULL : &m_captureId;
    deviceConfig.capture.format    = ma_format_f32; 
    deviceConfig.capture.channels  = m_config.channels;
    deviceConfig.capture.shareMode = ma_share_mode_shared;

    deviceConfig.playback.pDeviceID = outputDeviceId.empty() ? NULL : &m_playbackId;
    deviceConfig.playback.format    = ma_format_f32;
    deviceConfig.playback.channels  = m_config.channels;
    deviceConfig.playback.shareMode = ma_share_mode_shared;

    deviceConfig.sampleRate = m_config.sampleRate;
    deviceConfig.dataCallback = DataCallback;
    deviceConfig.pUserData = this;
    
    deviceConfig.periodSizeInFrames = m_config.bufferSize;
    deviceConfig.performanceProfile = ma_performance_profile_low_latency;

    if (ma_device_init(&m_context, &deviceConfig, &m_device) != MA_SUCCESS) {
        ma_context_uninit(&m_context);
        return false;
    }

    m_isInitialized = true;
    return true;
}

bool AudioEngine::Start() {
    if (!m_isInitialized) return false;
    
    if (ma_device_start(&m_device) != MA_SUCCESS) {
        return false;
    }
    
#ifdef _WIN32
    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);
    if (hTask) {
        AvSetMmThreadPriority(hTask, AVRT_PRIORITY_CRITICAL);
    }
#endif

    return true;
}

void AudioEngine::Stop() {
    if (m_isInitialized) {
        ma_device_stop(&m_device);
    }
}

void AudioEngine::DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    AudioEngine* engine = static_cast<AudioEngine*>(pDevice->pUserData);
    if (engine) {
        engine->ProcessAudio(static_cast<const float*>(pInput), static_cast<float*>(pOutput), frameCount);
    }
}

void AudioEngine::ProcessVoiceConversion(float* buffer, uint32_t frameCount) {
    const uint32_t channels = m_config.channels;

    // La voix convertie n'est lue qu'une fois la réserve constituée (et à nouveau après un trou)
    size_t available = m_conversionOutput.Available();
    if (m_conversionBuffering && available >= kConversionPrebuffer) m_conversionBuffering = false;

    for (uint32_t i = 0; i < frameCount; ++i) {
        float mono = 0.0f;
        for (uint32_t c = 0; c < channels; ++c) mono += buffer[i * channels + c];
        mono /= static_cast<float>(channels);
        m_conversionInput.Push(&mono, 1); // Si l'application ne lit plus, la file est pleine et le son est ignoré

        float converted = 0.0f;
        if (!m_conversionBuffering && m_conversionOutput.Pop(&converted, 1) == 0) {
            m_conversionBuffering = true;
        }
        for (uint32_t c = 0; c < channels; ++c) buffer[i * channels + c] = converted;
    }
}

void AudioEngine::ProcessAudio(const float* input, float* output, uint32_t frameCount) {
    if (input == nullptr) {
        std::fill_n(output, frameCount * m_config.channels, 0.0f);
        return;
    }

    std::copy_n(input, frameCount * m_config.channels, output);

    if (m_conversionEnabled.load(std::memory_order_relaxed)) {
        ProcessVoiceConversion(output, frameCount);
    }

    if (m_effectChain) {
        m_effectChain->Process(output, frameCount, m_config.channels, m_config.sampleRate);
    }

    if (m_soundboard) {
        m_soundboard->MixInto(output, frameCount, m_config.channels);
    }

    // Hard Clipper
    for (uint32_t i = 0; i < frameCount * m_config.channels; ++i) {
        if (output[i] > 1.0f) output[i] = 1.0f;
        else if (output[i] < -1.0f) output[i] = -1.0f;
    }
}