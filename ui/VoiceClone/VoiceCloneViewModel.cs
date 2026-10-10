using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows;
using VoiceChangerApp.Interop;

namespace VoiceChangerApp.VoiceClone
{
    // Section « Voix de personnage (IA) » de la fenêtre principale
    public class VoiceCloneViewModel : INotifyPropertyChanged
    {
        public const string NoVoice = "(Aucune)";

        private readonly VoiceCloneEngine _engine = new VoiceCloneEngine();
        private string _status = "Choisis une voix dans la liste, ou ajoute un modèle .onnx.";
        private string _selectedVoice = NoVoice;
        private bool _isReady;
        private bool _isBusy;
        private bool _isEnabled;
        private float _transpose;

        public event PropertyChangedEventHandler PropertyChanged;

        public VoiceCloneViewModel()
        {
            _engine.StatusChanged += message => Application.Current?.Dispatcher.BeginInvoke(new Action(() =>
            {
                if (_isEnabled) Status = message;
            }));
            RefreshVoices();
        }

        // Dossier "voices" situé à côté de l'exécutable : un fichier .onnx par personnage
        public static string VoiceDirectory => Path.Combine(AppContext.BaseDirectory, "voices");

        public ObservableCollection<string> Voices { get; } = new ObservableCollection<string>();

        // Demande l'accord avant de télécharger le modèle de base : (nom du fichier, taille en Mo, adresse)
        public Func<string, int, string, bool> ConfirmDownload { get; set; }

        public string Status
        {
            get => _status;
            private set { _status = value; OnPropertyChanged(); }
        }

        // Vrai quand une voix est chargée et peut être activée
        public bool IsReady
        {
            get => _isReady;
            private set { _isReady = value; OnPropertyChanged(); }
        }

        public bool CanLoad => !_isBusy;

        public string SelectedVoice
        {
            get => _selectedVoice;
            set
            {
                if (value == null || _selectedVoice == value) return;
                _selectedVoice = value;
                OnPropertyChanged();
                _ = LoadSelectedVoiceAsync();
            }
        }

        public bool IsEnabled
        {
            get => _isEnabled;
            set
            {
                if (_isEnabled == value || (value && !_isReady)) return;
                _isEnabled = value;
                OnPropertyChanged();
                Apply();
                if (!value && _isReady) Status = $"Voix « {_selectedVoice} » prête.";
            }
        }

        // Décalage de hauteur de la voix du personnage (Slider de -12 à +12 demi-tons)
        public float Transpose
        {
            get => _transpose;
            set
            {
                if (_transpose != value)
                {
                    _transpose = value;
                    _engine.Transpose = value;
                    OnPropertyChanged();
                }
            }
        }

        // À rappeler quand le moteur audio est recréé (changement de périphérique)
        public void Apply()
        {
            AudioCoreInterop.VC_SetVoiceConversionEnabled(_isEnabled);
            if (_isEnabled) _engine.Start();
            else _engine.Stop();
        }

        // Copie un modèle .onnx dans le dossier des voix et le sélectionne
        public void AddVoice(string path)
        {
            Directory.CreateDirectory(VoiceDirectory);
            string target = Path.Combine(VoiceDirectory, Path.GetFileName(path));
            if (!string.Equals(Path.GetFullPath(path), Path.GetFullPath(target), StringComparison.OrdinalIgnoreCase))
            {
                File.Copy(path, target, true);
            }

            RefreshVoices();
            string name = Path.GetFileNameWithoutExtension(path);
            if (_selectedVoice == name) _ = LoadSelectedVoiceAsync();
            else SelectedVoice = name;
        }

        private void RefreshVoices()
        {
            Voices.Clear();
            Voices.Add(NoVoice);
            if (Directory.Exists(VoiceDirectory))
            {
                foreach (string file in Directory.GetFiles(VoiceDirectory, "*.onnx").OrderBy(f => f))
                {
                    Voices.Add(Path.GetFileNameWithoutExtension(file));
                }
            }
            OnPropertyChanged(nameof(SelectedVoice));
        }

        private async Task LoadSelectedVoiceAsync()
        {
            string voice = _selectedVoice;
            IsEnabled = false;
            IsReady = false;
            if (voice == NoVoice)
            {
                Status = "Choisis une voix dans la liste, ou ajoute un modèle .onnx.";
                return;
            }

            SetBusy(true);
            try
            {
                Status = $"Chargement de la voix « {voice} »…";
                string path = Path.Combine(VoiceDirectory, voice + ".onnx");
                await Task.Run(() => _engine.LoadGenerator(path));

                int featureSize = _engine.FeatureSize;
                if (!BaseModelStore.IsAvailable(featureSize))
                {
                    bool accepted = ConfirmDownload != null && ConfirmDownload(
                        BaseModelStore.FileName(featureSize), BaseModelStore.SizeMb(featureSize), BaseModelStore.UrlFor(featureSize));
                    if (!accepted)
                    {
                        Status = "Modèle de base manquant : la voix ne peut pas être utilisée sans lui.";
                        return;
                    }

                    var progress = new Progress<double>(p => Status = $"Téléchargement du modèle de base… {p:P0}");
                    await BaseModelStore.DownloadAsync(featureSize, ((IProgress<double>)progress).Report);
                }

                Status = "Chargement du modèle de base…";
                await Task.Run(() => _engine.LoadContent(BaseModelStore.PathFor(featureSize)));

                IsReady = true;
                Status = $"Voix « {voice} » prête.";
            }
            catch (Exception ex)
            {
                Status = "Impossible de charger la voix : " + ex.Message;
            }
            finally
            {
                SetBusy(false);
            }
        }

        private void SetBusy(bool busy)
        {
            _isBusy = busy;
            OnPropertyChanged(nameof(CanLoad));
        }

        public void Shutdown()
        {
            AudioCoreInterop.VC_SetVoiceConversionEnabled(false);
            _engine.Dispose();
        }

        protected void OnPropertyChanged([CallerMemberName] string propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}
