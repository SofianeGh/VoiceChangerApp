// "PluginItem.cs"
using System.ComponentModel;
using VoiceChangerApp.Interop;

namespace VoiceChangerApp.ViewModels
{
    // Un plugin chargé dans la chaîne d'effets, activable depuis une case à cocher
    public class PluginItem : INotifyPropertyChanged
    {
        private readonly int _effectIndex;
        private bool _isEnabled;

        public event PropertyChangedEventHandler PropertyChanged;

        public PluginItem(int effectIndex, string name, bool isEnabled)
        {
            _effectIndex = effectIndex;
            _isEnabled = isEnabled;
            Name = name;
            AudioCoreInterop.VC_SetEffectEnabled(_effectIndex, _isEnabled);
        }

        public string Name { get; }

        public bool IsEnabled
        {
            get => _isEnabled;
            set
            {
                if (_isEnabled != value)
                {
                    _isEnabled = value;
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsEnabled)));
                    AudioCoreInterop.VC_SetEffectEnabled(_effectIndex, _isEnabled);
                }
            }
        }
    }
}
