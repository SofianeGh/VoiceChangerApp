// "PluginEffect.cpp"
#include "PluginEffect.h"

PluginEffect::PluginEffect(HMODULE hModule, PFN_Plugin_Create createFn, PFN_Plugin_Destroy destroyFn, 
                           PFN_Plugin_Process processFn, PFN_Plugin_SetParameter setParamFn)
    : m_hModule(hModule), m_destroyFn(destroyFn), m_processFn(processFn), m_setParamFn(setParamFn) 
{
    // Instancie l'effet à l'intérieur de la DLL
    m_handle = createFn();
}

PluginEffect::~PluginEffect() {
    if (m_handle && m_destroyFn) {
        m_destroyFn(m_handle);
    }
    // Note : FreeLibrary(m_hModule) pourrait être appelé ici, 
    // mais il est souvent géré par un PluginManager pour éviter de décharger une DLL encore utilisée.
}

void PluginEffect::Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
    if (m_handle && m_processFn) {
        m_processFn(m_handle, buffer, frameCount, channels, sampleRate);
    }
}

void PluginEffect::SetParameter(int paramId, float value) {
    if (m_handle && m_setParamFn) {
        m_setParamFn(m_handle, paramId, value);
    }
}