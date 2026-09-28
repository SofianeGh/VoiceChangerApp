// "PluginAPI.h"
#pragma once
#include <cstdint>

// Opaque handle pour représenter l'instance du plugin
typedef void* PluginHandle;

extern "C" {
    // Création et destruction
    __declspec(dllexport) PluginHandle Plugin_Create();
    __declspec(dllexport) void Plugin_Destroy(PluginHandle handle);

    // Métadonnées
    __declspec(dllexport) const char* Plugin_GetName();

    // Traitement audio (même signature que IEffect)
    __declspec(dllexport) void Plugin_Process(PluginHandle handle, float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate);
    
    // Modification des paramètres
    __declspec(dllexport) void Plugin_SetParameter(PluginHandle handle, int paramId, float value);
}

// Signatures des pointeurs de fonctions pour l'hôte (LoadLibrary)
typedef PluginHandle (*PFN_Plugin_Create)();
typedef void (*PFN_Plugin_Destroy)(PluginHandle);
typedef const char* (*PFN_Plugin_GetName)();
typedef void (*PFN_Plugin_Process)(PluginHandle, float*, uint32_t, uint32_t, uint32_t);
typedef void (*PFN_Plugin_SetParameter)(PluginHandle, int, float);