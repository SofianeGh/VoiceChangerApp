using System;
using System.Diagnostics;
using System.Windows;
using VoiceChangerApp.ViewModels;

namespace VoiceChangerApp
{
    public partial class MainWindow : Window
    {
        private readonly MainViewModel _viewModel = new MainViewModel();

        public MainWindow()
        {
            InitializeComponent();
            DataContext = _viewModel;
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
            _viewModel.Shutdown();
        }
    }
}
