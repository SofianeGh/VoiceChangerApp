using System;
using System.Diagnostics;
using System.Windows;
using Microsoft.Win32;
using VoiceChangerApp.ViewModels;
using VoiceChangerApp.VoiceClone;

namespace VoiceChangerApp
{
    public partial class MainWindow : Window
    {
        private readonly MainViewModel _viewModel = new MainViewModel();
        private readonly VoiceCloneViewModel _cloneViewModel = new VoiceCloneViewModel();

        public MainWindow()
        {
            InitializeComponent();
            DataContext = _viewModel;
            ClonePanel.DataContext = _cloneViewModel;

            // Le moteur audio est recréé à chaque changement de périphérique : la conversion doit être réactivée
            _viewModel.EngineRestarted += _cloneViewModel.Apply;

            _cloneViewModel.ConfirmDownload = (fileName, sizeMb, url) =>
                MessageBox.Show(this,
                    "Pour utiliser une voix de personnage, il faut un modèle de base commun à tous les personnages.\n\n" +
                    $"Fichier : {fileName} (environ {sizeMb} Mo)\nSource : {url}\n\n" +
                    "Il est téléchargé une seule fois. Continuer ?",
                    "Modèle de base", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes;
        }

        private void OnAddCloneVoice(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Title = "Choisir le modèle de voix du personnage",
                Filter = "Modèle RVC exporté en ONNX (*.onnx)|*.onnx",
            };
            if (dialog.ShowDialog(this) != true) return;

            try
            {
                _cloneViewModel.AddVoice(dialog.FileName);
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, "Impossible d'ajouter cette voix : " + ex.Message, "Voix de personnage",
                    MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void OnOpenPluginFolder(object sender, RoutedEventArgs e)
        {
            Process.Start(new ProcessStartInfo(MainViewModel.PluginDirectory) { UseShellExecute = true });
        }

        private async void OnInstallCable(object sender, RoutedEventArgs e)
        {
            MessageBoxResult answer = MessageBox.Show(this,
                "Pour ajouter un nouveau micro à Windows (utilisable dans Discord), il faut installer un pilote audio virtuel.\n\n" +
                "Voice Changer va télécharger VB-CABLE (gratuit) depuis le site officiel vb-audio.com et lancer son installeur. " +
                "Windows demandera les droits administrateur ; clique ensuite sur « Install Driver ».\n\nContinuer ?",
                "Installer le micro virtuel", MessageBoxButton.YesNo, MessageBoxImage.Question);
            if (answer != MessageBoxResult.Yes) return;

            InstallCableButton.IsEnabled = false;
            try
            {
                await VirtualCableInstaller.InstallAsync();
            }
            catch (Exception ex)
            {
                MessageBox.Show(this,
                    "Le téléchargement automatique a échoué (" + ex.Message + ").\n\nLa page officielle va s'ouvrir : " +
                    "télécharge VB-CABLE, lance VBCABLE_Setup_x64.exe en administrateur, puis relance Voice Changer.",
                    "Installer le micro virtuel", MessageBoxButton.OK, MessageBoxImage.Warning);
                VirtualCableInstaller.OpenDownloadPage();
            }
            finally
            {
                InstallCableButton.IsEnabled = true;
            }

            _viewModel.RefreshDevices();
            if (!_viewModel.HasDedicatedCable)
            {
                MessageBox.Show(this,
                    "Le micro virtuel n'est pas encore détecté. Si l'installation s'est bien passée, redémarre Windows puis relance Voice Changer.",
                    "Installer le micro virtuel", MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }

        private void OnClosed(object sender, EventArgs e)
        {
            _cloneViewModel.Shutdown();
            _viewModel.Shutdown();
        }
    }
}
