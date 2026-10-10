using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net.Http;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace VoiceChangerApp
{
    // Installe VB-CABLE (pilote de micro virtuel gratuit de VB-Audio) depuis le site officiel
    public static class VirtualCableInstaller
    {
        public const string PageUrl = "https://vb-audio.com/Cable/";

        private static readonly Regex PackLink = new Regex(
            @"https://download\.vb-audio\.com/Download_CABLE/VBCABLE_Driver_Pack(\d+)\.zip", RegexOptions.IgnoreCase);

        // Renvoie true si l'installeur a été lancé puis fermé, false si l'utilisateur a refusé l'élévation
        public static async Task<bool> InstallAsync()
        {
            using var http = new HttpClient { Timeout = TimeSpan.FromMinutes(2) };

            // Le numéro de version du pack change : on prend le plus récent listé sur la page officielle
            string page = await http.GetStringAsync(PageUrl);
            Match latest = PackLink.Matches(page).OrderByDescending(m => int.Parse(m.Groups[1].Value)).FirstOrDefault();
            if (latest == null) throw new InvalidOperationException("Lien de téléchargement VB-CABLE introuvable.");

            string workDir = Path.Combine(Path.GetTempPath(), "VoiceChangerApp_VBCable_" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(workDir);
            string zipPath = Path.Combine(workDir, "vbcable.zip");
            await File.WriteAllBytesAsync(zipPath, await http.GetByteArrayAsync(latest.Value));

            string extractDir = Path.Combine(workDir, "pack");
            ZipFile.ExtractToDirectory(zipPath, extractDir);

            string setup = Path.Combine(extractDir, Environment.Is64BitOperatingSystem ? "VBCABLE_Setup_x64.exe" : "VBCABLE_Setup.exe");
            if (!File.Exists(setup)) throw new FileNotFoundException("Installeur VB-CABLE introuvable dans l'archive.", setup);

            try
            {
                // L'installation d'un pilote exige les droits administrateur (fenêtre UAC)
                using Process process = Process.Start(new ProcessStartInfo(setup)
                {
                    UseShellExecute = true,
                    Verb = "runas",
                    WorkingDirectory = extractDir,
                });
                await process.WaitForExitAsync();
                return true;
            }
            catch (Win32Exception)
            {
                return false;
            }
        }

        public static void OpenDownloadPage()
        {
            Process.Start(new ProcessStartInfo(PageUrl) { UseShellExecute = true });
        }
    }
}
