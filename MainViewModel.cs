// "MainViewModel.cs"
using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using VoiceChangerApp.Interop;

namespace VoiceChangerApp.ViewModels
{
    public class MainViewModel : INotifyPropertyChanged
    {
        private float _pitchValue;
        private float _robotFrequency = 50.0f;
        private bool _isEngineRunning;

        public event PropertyChangedEventHandler PropertyChanged;

        public MainViewModel()
        {
            // Initialisation avec les périphériques WASAPI par défaut
            bool initSuccess = AudioCoreInterop.VC_Initialize(null, null);
            if (initSuccess)
            {
                IsEngineRunning = AudioCoreInterop.VC_Start();
            }
        }

        public bool IsEngineRunning
        {
            get => _isEngineRunning;
            set { _isEngineRunning = value; OnPropertyChanged(); }
        }

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

        protected void OnPropertyChanged([CallerMemberName] string propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        ~MainViewModel()
        {
            AudioCoreInterop.VC_Stop();
        }
    }
}