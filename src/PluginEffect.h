// "PluginEffect.h"
#pragma once
#include "IEffect.h"
#include "PluginAPI.h"
#include <windows.h>
#include <string>

class PluginEffect : public IEffect {
public:
    // Initialisé par le PluginManager après le LoadLibrary
    PluginEffect(HMODULE hModule, PFN_Plugin_Create createFn, PFN_Plugin_Destroy destroyFn, 
                 PFN_Plugin_Process processFn, PFN_Plugin_SetParameter setParamFn);
    
    ~PluginEffect() override;

    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) override;
    void SetParameter(int paramId, float value) override;

private:
    HMODULE m_hModule; // Garde la DLL chargée en mémoire
    PluginHandle m_handle;
    
    PFN_Plugin_Destroy m_destroyFn;
    PFN_Plugin_Process m_processFn;
    PFN_Plugin_SetParameter m_setParamFn;
};