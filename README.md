# Voice Changer

Changeur de voix en temps réel pour Windows. Il prend ton micro, transforme ta voix, et envoie le résultat dans un micro virtuel utilisable dans Discord, OBS ou n'importe quel autre logiciel.

- **Effets classiques** : hauteur de voix, effet robot, et 15 plugins à cocher (voix homme / femme / enfant, téléphone, mégaphone, réverbération…).
- **Voix de personnage par IA** : clonage de voix en temps réel à partir d'un modèle RVC (par exemple un personnage de jeu vidéo).
- **Plugins** : chaque effet est une DLL indépendante, et tu peux écrire les tiens.

## Sommaire

- [Fonctionnement](#fonctionnement)
- [Prérequis](#prérequis)
- [Installation et compilation](#installation-et-compilation)
- [Utilisation](#utilisation)
- [Plugins](#plugins)
- [Voix de personnage (IA)](#voix-de-personnage-ia)
- [Structure du projet](#structure-du-projet)
- [API native](#api-native)
- [Distribution sur un autre PC](#distribution-sur-un-autre-pc)
- [Limites connues](#limites-connues)
- [Usage responsable](#usage-responsable)
- [Dépendances](#dépendances)
- [Licence](#licence)

## Fonctionnement

```
Micro ──► Moteur audio (CoreAudio.dll, C++)
              │
              ├─ (option) Voix de personnage IA ◄──► application C# (ONNX Runtime)
              ├─ Pitch (SoundTouch)
              ├─ Effet robot
              └─ Plugins cochés (une DLL par effet)
              │
              ▼
        Micro virtuel (VB-CABLE) ──► Discord, OBS, jeu…
```

Le projet est en deux parties :

| Partie | Langage | Rôle |
|---|---|---|
| `CoreAudio.dll` | C++20 | Capture et restitution audio (miniaudio / WASAPI), chaîne d'effets, chargement des plugins |
| `VoiceChangerApp.exe` | C# / WPF (.NET 8) | Interface, choix des périphériques, clonage de voix par IA |

Windows ne permet pas à une application de créer un micro toute seule : il faut un pilote audio. Le logiciel s'appuie donc sur [VB-CABLE](https://vb-audio.com/Cable/), un pilote gratuit, qu'il propose d'installer en un clic s'il est absent.

## Prérequis

- Windows 10 ou 11, 64 bits
- [MSYS2](https://www.msys2.org/) (compilateur C++ et bibliothèques audio)
- [SDK .NET 8](https://dotnet.microsoft.com/download/dotnet/8.0)
- Pour la voix de personnage par IA : une carte graphique compatible DirectX 12 est recommandée (le calcul se fait sinon sur le processeur, plus lentement)
- Pour convertir un modèle RVC `.pth` : Python 3.12

Avec `winget` :

```bash
winget install MSYS2.MSYS2
```

```bash
winget install Microsoft.DotNet.SDK.8
```

Puis, dans le shell **MSYS2 UCRT64** (menu Démarrer) :

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-soundtouch mingw-w64-ucrt-x86_64-miniaudio make
```

## Installation et compilation

```bash
git clone https://github.com/SofianeGh/VoiceChangerApp
```

### En une commande

Depuis PowerShell, dans le dossier du projet :

```bash
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Run
```

Le script [build.ps1](build.ps1) compile le moteur natif et les plugins (via MSYS2), puis l'application, et la lance avec `-Run`. Autres options : `-Test` lance les tests des plugins, `-Publish` crée le dossier `dist\` copiable sur un autre PC, `-Msys2 <dossier>` indique où MSYS2 est installé s'il n'est pas dans `C:\msys64`.

### Étape par étape

**1. Le moteur natif et les plugins** — dans le shell MSYS2 UCRT64, depuis le dossier du projet :

```bash
make
```

Cela produit `bin/CoreAudio.dll` et une DLL par plugin dans `bin/plugins/`. Tout est lié statiquement : aucune DLL MinGW n'est nécessaire à l'exécution.

**2. L'application** — depuis PowerShell ou un terminal classique :

```bash
dotnet run --project ui -c Release
```

L'application copie automatiquement `CoreAudio.dll` et les plugins à côté de l'exécutable. Il faut donc lancer `make` avant, et le relancer après chaque modification du C++.

Autres cibles du Makefile : `make clean`, `make fclean`, `make re`.

### Tests

```bash
make test
```

Deux programmes, dans `tests/`, tournent sur des signaux de synthèse, sans micro :

- `plugin_smoke_test` charge chaque plugin et vérifie qu'il exporte toute l'API et produit un son valide (ni muet, ni saturé, ni valeurs invalides).
- `voice_type_test` fait passer une voix grave puis une voix aiguë dans « Voix homme / femme / enfant » et vérifie que la hauteur de sortie atteint celle visée, sans coupure.

Le clonage de voix par IA n'a pas de test automatisé : il dépend de modèles qui ne sont pas dans le dépôt.

## Utilisation

1. **Micro (entrée)** : choisis ton micro.
2. **Micro virtuel** : s'il n'y en a pas sur la machine, clique sur « Créer le micro virtuel ». Le logiciel télécharge VB-CABLE depuis le site officiel et lance son installeur (droits administrateur demandés).
3. Dans Discord : *Paramètres → Voix et vidéo → Périphérique d'entrée* → **CABLE Output (VB-Audio Virtual Cable)**. Le nom exact à choisir est affiché dans le logiciel.
4. Règle ta voix avec les sliders, les plugins ou une voix de personnage.

À savoir :

- Tu ne t'entends pas toi-même : le son part dans le micro virtuel. Utilise le test micro de Discord pour t'écouter.
- Dans « Avancé », tu peux envoyer la voix transformée vers un autre périphérique (par exemple un casque, pour t'écouter). Évite les haut-parleurs, qui créent du larsen.
- Un micro branché après le lancement n'apparaît qu'en relançant le logiciel.

## Plugins

Les plugins du dossier `plugins/` (à côté de l'exécutable) sont chargés au démarrage et apparaissent comme des cases à cocher, décochées par défaut. Plusieurs plugins cochés s'appliquent l'un après l'autre.

| Plugin | Effet |
|---|---|
| Voix homme | Ramène la voix vers une voix grave d'homme (100 Hz) |
| Voix femme | Ramène la voix vers une voix de femme (210 Hz) |
| Voix enfant | Ramène la voix vers une voix d'enfant (290 Hz) |
| Distorsion | Voix saturée |
| Megaphone | Mégaphone / talkie-walkie |
| Telephone | Voix au téléphone |
| Bitcrusher (8-bit) | Vieux jeu vidéo |
| Reverb (cathedrale) | Grande salle |
| Chorus (chorale) | Plusieurs voix en même temps |
| Vibrato | Voix chevrotante |
| Tremolo | Volume qui tremble |
| Sous l'eau | Voix étouffée qui flotte |
| Wah-wah | Filtre qui balaie les médiums |
| Noise gate (anti-bruit) | Coupe le bruit de fond entre les phrases |
| Simple Echo Plugin | Écho |

Les plugins « Voix homme / femme / enfant » détectent la hauteur de ta voix et s'y adaptent, en réglant le timbre séparément de la hauteur (méthode PSOLA). N'en coche qu'un à la fois.

### Écrire un plugin

Un plugin est une DLL qui exporte cinq fonctions C, déclarées dans [src/PluginAPI.h](src/PluginAPI.h). Le plus simple est de partir de [src/plugins/PluginHelper.h](src/plugins/PluginHelper.h), qui génère ces exports à partir d'une classe :

```cpp
// src/plugins/GainPlugin.cpp
#include "PluginHelper.h"

class GainDSP {
public:
    // buffer : échantillons entrelacés (L, R, L, R…), modifiés sur place
    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t) {
        for (uint32_t i = 0; i < frameCount * channels; ++i) buffer[i] *= 0.5f;
    }

    void SetParam(int, float) {}
};

VC_DEFINE_PLUGIN(GainDSP, "Gain -6 dB")
```

Tout fichier `.cpp` placé dans `src/plugins/` est compilé par `make` en `bin/plugins/<nom>.dll`. `PluginHelper.h` fournit aussi des briques prêtes à l'emploi : filtre du second ordre (`Biquad`), ligne à retard (`DelayLine`) et oscillateur basse fréquence (`Lfo`).

`Process` est appelé depuis le thread audio : pas d'allocation mémoire, pas de verrou, pas d'entrée-sortie dedans.

## Voix de personnage (IA)

Cette fonction remplace ta voix par celle d'un personnage, à partir d'un modèle [RVC](https://github.com/RVC-Project/Retrieval-based-Voice-Conversion-WebUI) au format ONNX.

### Utiliser une voix

1. Place le fichier `.onnx` du personnage dans le dossier `voices/` à côté de l'exécutable, ou utilise le bouton « Ajouter une voix (.onnx)… ».
2. Choisis la voix dans la liste.
3. Au premier chargement, le logiciel propose de télécharger le **modèle de base ContentVec** (environ 360 Mo, une seule fois, commun à toutes les voix). Il est rangé dans `models/` à côté de l'exécutable.
4. Coche « Activer la voix du personnage ».
5. Ajuste « Hauteur de la voix du personnage » si le résultat est trop grave ou trop aigu.

Le temps de calcul s'affiche dans le logiciel. Le délai total entre ta voix et la voix convertie est d'environ 0,5 à 0,7 seconde. Les sliders et les plugins s'appliquent après la voix du personnage.

### Convertir un modèle `.pth` en `.onnx`

La plupart des modèles RVC sont distribués en `.pth` (souvent dans un `.zip`). Le script [tools/convert_rvc.py](tools/convert_rvc.py) les convertit. Mise en place, une seule fois, depuis le dossier du projet :

```bash
python -m venv tools/.venv
```

```bash
tools/.venv/Scripts/python.exe -m pip install -r tools/requirements.txt
```

```bash
git clone --depth 1 --branch 2.2.231006 https://github.com/RVC-Project/Retrieval-based-Voice-Conversion-WebUI tools/rvc-src
```

Puis, pour chaque modèle :

```bash
tools/.venv/Scripts/python.exe tools/convert_rvc.py chemin/vers/modele.zip models/nom.onnx
```

Les fichiers `.onnx` placés dans `models/` sont copiés dans `voices/` à la compilation de l'application. Le modèle de base, s'il est placé dans `models/base/`, est copié lui aussi.

Points d'attention :

- Le script utilise la version **2.2.231006** du code RVC : c'est la dernière qui contient l'export ONNX. Il corrige à la volée deux défauts de cet export sans modifier les fichiers de RVC.
- Les modèles RVC v1 et v2 avec hauteur de voix (f0) sont pris en charge. Les fichiers `.index` ne sont pas utilisés.
- Un `.pth` est un fichier « pickle », qui peut contenir du code. Le script le charge en mode restreint (`weights_only=True`), qui refuse d'exécuter ce code. Ne convertis quand même que des modèles de sources en lesquelles tu as confiance.

### Sous le capot

Par blocs de 250 ms : micro 48 kHz → 16 kHz → ContentVec (ce qui est dit) + détection de la hauteur → générateur RVC (la voix du personnage) → raccord avec le bloc précédent (SOLA) → 48 kHz. Le calcul passe par ONNX Runtime avec DirectML (carte graphique), et bascule sur le processeur si DirectML échoue.

## Structure du projet

```
VoiceChangerApp/
├── Makefile                    Compilation de CoreAudio.dll et des plugins, tests
├── build.ps1                   Compilation complète en une commande
├── src/
│   ├── AudioEngine.h/.cpp      Moteur audio (miniaudio, périphériques, pont vers le clonage IA)
│   ├── EffectChain.h/.cpp      Chaîne d'effets, activation effet par effet
│   ├── VoiceAPI.cpp            Fonctions C exportées par CoreAudio.dll
│   ├── PitchShiftEffect.h      Effet de hauteur (SoundTouch)
│   ├── RobotEffect.h           Effet robot (modulation en anneau)
│   ├── PluginAPI.h             Interface C des plugins
│   ├── PluginManager.*         Chargement des DLL de plugins
│   ├── PluginEffect.*          Adaptateur plugin → effet
│   ├── SampleFifo.h            File sans verrou entre le thread audio et l'application
│   ├── Soundboard.h            Lecture d'échantillons (non relié à l'interface)
│   ├── MyEchoPlugin.cpp        Plugin d'exemple
│   ├── plugins/                Un fichier .cpp par plugin, plus PluginHelper.h et VoiceType.h
│   ├── AudioCoreInterop.cs     Déclarations P/Invoke vers CoreAudio.dll
│   ├── MainViewModel.cs        Logique de la fenêtre principale
│   └── PluginItem.cs           Un plugin dans la liste
├── ui/
│   ├── VoiceChangerApp.csproj  Projet WPF (.NET 8)
│   ├── MainWindow.xaml(.cs)    Fenêtre principale
│   ├── VirtualCableInstaller.cs  Installation de VB-CABLE
│   └── VoiceClone/             Clonage de voix : moteur, détection de hauteur, modèle de base, vue
├── tests/                      Tests des plugins (signaux de synthèse)
└── tools/
    ├── convert_rvc.py          Conversion des modèles RVC .pth en .onnx
    └── requirements.txt        Paquets Python nécessaires à la conversion
```

Dossiers générés ou locaux, exclus du dépôt : `bin/`, `obj/`, `dist/`, `models/`, `tools/.venv/`, `tools/rvc-src/`.

## API native

Fonctions exportées par `CoreAudio.dll` (convention `cdecl`), déclarées côté C# dans [src/AudioCoreInterop.cs](src/AudioCoreInterop.cs) :

| Fonction | Rôle |
|---|---|
| `VC_EnumerateDevices(callback)` | Liste les micros et les sorties |
| `VC_Initialize(entrée, sortie)` | Crée le moteur sur les périphériques nommés (`NULL` = par défaut) |
| `VC_Start()` / `VC_Stop()` | Démarre le traitement / détruit le moteur |
| `VC_SetEffectParameter(index, param, valeur)` | Règle un paramètre d'un effet |
| `VC_SetEffectEnabled(index, actif)` | Active ou court-circuite un effet |
| `VC_ScanPlugins(dossier, callback)` | Charge les plugins d'un dossier |
| `VC_SetVoiceConversionEnabled(actif)` | Active le pont vers le clonage IA |
| `VC_ReadVoiceConversionInput(tampon, n)` | Lit le micro (mono) pour le clonage |
| `VC_WriteVoiceConversionOutput(tampon, n)` | Renvoie la voix convertie (mono) |
| `VC_GetSampleRate()` | Fréquence d'échantillonnage du moteur |

Indices dans la chaîne d'effets : `0` = pitch (paramètre 0 : demi-tons), `1` = robot (paramètre 0 : fréquence en Hz, paramètre 1 : dosage de 0 à 1), puis les plugins à partir de `2`, dans l'ordre de chargement. Le moteur tourne en 48 kHz stéréo, par tampons de 256 échantillons.

## Distribution sur un autre PC

```bash
dotnet publish ui -c Release -r win-x64 --self-contained true -o dist
```

Le dossier `dist/` contient tout : .NET, le moteur natif, les plugins, et les modèles de voix présents dans `models/`. Il se copie tel quel sur un autre PC Windows 64 bits, sans rien installer d'autre. Sur la nouvelle machine, il faut seulement cliquer une fois sur « Créer le micro virtuel ».

## Limites connues

- **Micro virtuel** : il porte le nom de VB-CABLE (« CABLE Output »). Un micro au nom du logiciel demanderait d'écrire et de faire signer un pilote Windows.
- **Voix de personnage** : délai d'environ 0,5 à 0,7 seconde ; la qualité dépend entièrement du modèle utilisé ; trop lent sur un processeur seul peu puissant.
- **Plugins** : intensité fixe, pas de réglage par plugin dans l'interface ; les cases cochées ne sont pas mémorisées d'un lancement à l'autre.
- **Tests** : seuls les plugins sont testés automatiquement ; le moteur audio, l'interface et le clonage par IA ne le sont pas.

## Usage responsable

Le clonage de voix sert ici à s'amuser. Les voix de personnages sont celles de vrais comédiens, et les modèles faits par des fans s'appuient sur des enregistrements qui appartiennent aux éditeurs des jeux.

- N'utilise pas ce logiciel pour te faire passer pour une personne réelle ou pour tromper quelqu'un.
- Ne redistribue pas de modèles de voix sans en avoir le droit ; c'est pour cela que le dossier `models/` est exclu du dépôt.
- Pas d'usage commercial d'une voix clonée sans l'accord de son propriétaire.

## Dépendances

- [miniaudio](https://miniaud.io/) — capture et restitution audio (domaine public ou MIT-0)
- [SoundTouch](https://www.surina.net/soundtouch/) — changement de hauteur (LGPL 2.1)
- [ONNX Runtime](https://onnxruntime.ai/) avec DirectML — exécution des modèles de voix (MIT)
- [RVC](https://github.com/RVC-Project/Retrieval-based-Voice-Conversion-WebUI) — architecture des modèles de voix et export ONNX (MIT)
- [ContentVec](https://github.com/auspicious3000/contentvec) — modèle de base du clonage (MIT)
- [VB-CABLE](https://vb-audio.com/Cable/) — pilote de micro virtuel (gratuit, non redistribué ici : téléchargé depuis le site officiel)

## Licence

Le code de ce projet est sous licence MIT : voir [LICENSE](LICENSE).

Cette licence ne couvre que le code du dépôt. Les bibliothèques ci-dessus gardent leur propre licence, et les modèles de voix ne sont pas fournis. SoundTouch, sous LGPL 2.1, est lié statiquement dans `CoreAudio.dll` : si tu distribues une version compilée, fournis aussi le code source (ou les fichiers objets) pour que SoundTouch puisse être remplacé, comme la LGPL le demande.
