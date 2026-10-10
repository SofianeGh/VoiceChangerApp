// "MainViewModel.cs"
using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using VoiceChangerApp.Interop;

namespace VoiceChangerApp.ViewModels
{
    public class MainViewModel : INotifyPropertyChanged
    {
        private float _pitchValue;
        private float _robotFrequency = 50.0f;
        private float _robotMix;
        private bool _isEngineRunning;

        // Les plugins sont ajoutés à la chaîne après les effets standards (0 = Pitch, 1 = Robot)
        private const int FirstPluginIndex = 2;

        public event PropertyChangedEventHandler PropertyChanged;

        public ObservableCollection<PluginItem> Plugins { get; } = new ObservableCollection<PluginItem>();

        // Dossier "plugins" situé à côté de l'exécutable
        public static string PluginDirectory => Path.Combine(AppContext.BaseDirectory, "plugins");

        public string PluginHint => Plugins.Count > 0
            ? "Coche un plugin pour l'activer."
            : "Aucun plugin trouvé. Dépose une DLL de plugin dans le dossier, puis relance Voice Changer.";

        public const string DefaultDevice = "(Par défaut)";

        // Câbles virtuels connus : (sortie où envoyer la voix, micro associé à choisir dans Discord, OBS...)
        private static readonly (string Output, string Mic)[] VirtualCables =
        {
            ("CABLE Input", "CABLE Output"),
            ("Steam Streaming Microphone", "Steam Streaming Microphone"),
        };

        private string _selectedInput = DefaultDevice;
        private string _selectedOutput = DefaultDevice;
        private string _virtualMicName;
        private bool _hasDedicatedCable;
        private bool _outputChosenByUser;

        public ObservableCollection<string> InputDevices { get; } = new ObservableCollection<string>();
        public ObservableCollection<string> OutputDevices { get; } = new ObservableCollection<string>();

        public MainViewModel()
        {
            RefreshDevices();
        }

        // Nom du micro virtuel à sélectionner dans les autres logiciels (null si aucun câble virtuel)
        public string VirtualMicName
        {
            get => _virtualMicName;
            private set
            {
                _virtualMicName = value;
                OnPropertyChanged();
                OnPropertyChanged(nameof(HasVirtualCable));
                OnPropertyChanged(nameof(VirtualMicHint));
            }
        }

        public bool HasVirtualCable => _virtualMicName != null;

        // Vrai si le micro virtuel dédié (VB-CABLE, premier de la liste) est installé
        public bool HasDedicatedCable
        {
            get => _hasDedicatedCable;
            private set { _hasDedicatedCable = value; OnPropertyChanged(); }
        }

        public string VirtualMicHint => HasVirtualCable
            ? $"Micro virtuel actif. Dans Discord (ou un autre logiciel), choisis comme micro :\n{_virtualMicName}"
            : "Aucun micro virtuel sur cette machine. Clique sur « Créer le micro virtuel » pour l'ajouter à Windows.";

        // Relit la liste des périphériques et redémarre le moteur
        public void RefreshDevices()
        {
            // Pendant le vidage des listes, les ComboBox remettent la sélection à null (ignoré par les setters)
            InputDevices.Clear();
            OutputDevices.Clear();
            InputDevices.Add(DefaultDevice);
            OutputDevices.Add(DefaultDevice);

            AudioCoreInterop.VC_EnumerateDevices((isCapture, name) =>
            {
                (isCapture != 0 ? InputDevices : OutputDevices).Add(name);
            });

            string cableOutput = null;
            string cableMic = null;
            foreach (var cable in VirtualCables)
            {
                cableOutput = OutputDevices.FirstOrDefault(d => d.Contains(cable.Output, StringComparison.OrdinalIgnoreCase));
                cableMic = InputDevices.FirstOrDefault(d => d.Contains(cable.Mic, StringComparison.OrdinalIgnoreCase));
                if (cableOutput != null && cableMic != null) break;
                cableOutput = cableMic = null;
            }
            VirtualMicName = cableMic;
            HasDedicatedCable = cableMic != null && cableMic.Contains(VirtualCables[0].Mic, StringComparison.OrdinalIgnoreCase);

            if (!InputDevices.Contains(_selectedInput)) _selectedInput = DefaultDevice;
            if (!OutputDevices.Contains(_selectedOutput))
            {
                _selectedOutput = DefaultDevice;
                _outputChosenByUser = false;
            }

            // Si un câble virtuel est installé, on y envoie la voix transformée
            if (cableOutput != null && !_outputChosenByUser) _selectedOutput = cableOutput;

            OnPropertyChanged(nameof(SelectedInput));
            OnPropertyChanged(nameof(SelectedOutput));
            RestartEngine();
        }

        public string SelectedInput
        {
            get => _selectedInput;
            set
            {
                if (value != null && _selectedInput != value)
                {
                    _selectedInput = value;
                    OnPropertyChanged();
                    RestartEngine();
                }
            }
        }

        public string SelectedOutput
        {
            get => _selectedOutput;
            set
            {
                if (value != null && _selectedOutput != value)
                {
                    _selectedOutput = value;
                    _outputChosenByUser = true;
                    OnPropertyChanged();
                    RestartEngine();
                }
            }
        }

        // (Re)crée le moteur sur les périphériques sélectionnés et réapplique les réglages
        private void RestartEngine()
        {
            AudioCoreInterop.VC_Stop();
            IsEngineRunning = false;

            // Les plugins cochés le restent après un redémarrage du moteur
            var enabledNames = Plugins.Where(p => p.IsEnabled).Select(p => p.Name).ToHashSet();
            Plugins.Clear();

            string input = _selectedInput == DefaultDevice ? null : _selectedInput;
            string output = _selectedOutput == DefaultDevice ? null : _selectedOutput;

            if (AudioCoreInterop.VC_Initialize(input, output))
            {
                // Chargés avant le démarrage, désactivés par défaut
                Directory.CreateDirectory(PluginDirectory);
                AudioCoreInterop.VC_ScanPlugins(PluginDirectory, (id, name) =>
                {
                    Plugins.Add(new PluginItem(FirstPluginIndex + Plugins.Count, name, enabledNames.Contains(name)));
                });

                AudioCoreInterop.VC_SetEffectParameter(0, 0, _pitchValue);
                AudioCoreInterop.VC_SetEffectParameter(1, 0, _robotFrequency);
                AudioCoreInterop.VC_SetEffectParameter(1, 1, _robotMix);
                IsEngineRunning = AudioCoreInterop.VC_Start();
            }

            OnPropertyChanged(nameof(PluginHint));
        }

        public bool IsEngineRunning
        {
            get => _isEngineRunning;
            set { _isEngineRunning = value; OnPropertyChanged(); OnPropertyChanged(nameof(StatusText)); }
        }

        public string StatusText => IsEngineRunning ? "Moteur audio actif" : "Moteur audio arrêté (micro ou sortie indisponible)";

        // Lié via Binding WPF / WinUI 3 (Slider de -12 à +12)
        public float PitchValue
        {
            get => _pitchValue;
            set
            {
                if (_pitchValue != value)
                {
                    _pitchValue = value;
                    OnPropertyChanged();
                    // Index 0 = PitchShiftEffect, Param 0 = Demi-tons
                    AudioCoreInterop.VC_SetEffectParameter(0, 0, _pitchValue);
                }
            }
        }

        // Lié via Binding WPF / WinUI 3 (Slider de 20 à 100)
        public float RobotFrequency
        {
            get => _robotFrequency;
            set
            {
                if (_robotFrequency != value)
                {
                    _robotFrequency = value;
                    OnPropertyChanged();
                    // Index 1 = RobotEffect, Param 0 = Fréquence
                    AudioCoreInterop.VC_SetEffectParameter(1, 0, _robotFrequency);
                }
            }
        }

        // Lié via Binding WPF / WinUI 3 (Slider de 0 à 1)
        public float RobotMix
        {
            get => _robotMix;
            set
            {
                if (_robotMix != value)
                {
                    _robotMix = value;
                    OnPropertyChanged();
                    // Index 1 = RobotEffect, Param 1 = Mix
                    AudioCoreInterop.VC_SetEffectParameter(1, 1, _robotMix);
                }
            }
        }

        public void Shutdown()
        {
            AudioCoreInterop.VC_Stop();
            IsEngineRunning = false;
        }

        protected void OnPropertyChanged([CallerMemberName] string propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}
