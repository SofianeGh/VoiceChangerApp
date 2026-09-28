// "AudioCoreInterop.cs"
using System;
using System.Runtime.InteropServices;

namespace VoiceChangerApp.Interop
{
    public static class AudioCoreInterop
    {
        private const string DllName = "CoreAudio.dll";

        // Déclaration du Delegate (Le pointeur de fonction managé)
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void PluginDiscoveredCallback(int id, [MarshalAs(UnmanagedType.LPStr)] string name);

        // Importation de la fonction de scan
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern void VC_ScanPlugins(string directoryPath, PluginDiscoveredCallback callback);
    }
}