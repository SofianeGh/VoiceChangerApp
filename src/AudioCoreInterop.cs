// "AudioCoreInterop.cs"
using System;
using System.Runtime.InteropServices;

namespace VoiceChangerApp.Interop
{
    public static class AudioCoreInterop
    {
        private const string DllName = "CoreAudio.dll";

        // Énumération des périphériques (isCapture : 1 = micro, 0 = sortie)
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void DeviceDiscoveredCallback(int isCapture, [MarshalAs(UnmanagedType.LPUTF8Str)] string name);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void VC_EnumerateDevices(DeviceDiscoveredCallback callback);

        // Cycle de vie du moteur (null = périphérique par défaut, sinon nom du périphérique)
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool VC_Initialize(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string inputDeviceId,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string outputDeviceId);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool VC_Start();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void VC_Stop();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void VC_SetEffectParameter(int effectIndex, int paramId, float value);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void VC_SetEffectEnabled(int effectIndex, [MarshalAs(UnmanagedType.I1)] bool enabled);

        // Déclaration du Delegate (Le pointeur de fonction managé)
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void PluginDiscoveredCallback(int id, [MarshalAs(UnmanagedType.LPStr)] string name);

        // Importation de la fonction de scan
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern void VC_ScanPlugins(string directoryPath, PluginDiscoveredCallback callback);
    }
}