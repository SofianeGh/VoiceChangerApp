using System;
using System.IO;
using System.Net.Http;
using System.Threading.Tasks;

namespace VoiceChangerApp.VoiceClone
{
    // Modèles de base ContentVec, communs à tous les personnages, téléchargés une seule fois
    public static class BaseModelStore
    {
        private const string BaseUrl = "https://huggingface.co/NaruseMioShirakana/MoeSS-SUBModel/resolve/main/";

        // Dossier "models" situé à côté de l'exécutable : il suit le logiciel quand on copie son dossier
        public static string Directory => Path.Combine(AppContext.BaseDirectory, "models");

        // 768 = modèles RVC v2, 256 = modèles RVC v1
        public static string FileName(int featureSize) => featureSize == 256 ? "vec-256-layer-9.onnx" : "vec-768-layer-12.onnx";

        public static int SizeMb(int featureSize) => featureSize == 256 ? 280 : 360;

        public static string PathFor(int featureSize) => Path.Combine(Directory, FileName(featureSize));

        public static string UrlFor(int featureSize) => BaseUrl + FileName(featureSize);

        public static bool IsAvailable(int featureSize) => File.Exists(PathFor(featureSize));

        // progress reçoit l'avancement entre 0 et 1
        public static async Task DownloadAsync(int featureSize, Action<double> progress)
        {
            System.IO.Directory.CreateDirectory(Directory);
            string target = PathFor(featureSize);
            string partial = target + ".part";

            using var http = new HttpClient { Timeout = TimeSpan.FromHours(2) };
            using (HttpResponseMessage response = await http.GetAsync(UrlFor(featureSize), HttpCompletionOption.ResponseHeadersRead))
            {
                response.EnsureSuccessStatusCode();
                long total = response.Content.Headers.ContentLength ?? -1;

                using Stream source = await response.Content.ReadAsStreamAsync();
                using var file = new FileStream(partial, FileMode.Create, FileAccess.Write, FileShare.None);
                byte[] buffer = new byte[1 << 16];
                long done = 0;
                int read;
                while ((read = await source.ReadAsync(buffer)) > 0)
                {
                    await file.WriteAsync(buffer.AsMemory(0, read));
                    done += read;
                    if (total > 0) progress?.Invoke((double)done / total);
                }
            }

            // Le fichier ne prend son nom définitif qu'une fois complet
            File.Move(partial, target, true);
        }
    }
}
