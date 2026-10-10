using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using Microsoft.ML.OnnxRuntime;
using Microsoft.ML.OnnxRuntime.Tensors;
using VoiceChangerApp.Interop;

namespace VoiceChangerApp.VoiceClone
{
    // Conversion de voix en temps réel avec un modèle RVC exporté en ONNX.
    //
    // Chaîne de traitement, par blocs de 250 ms :
    //   micro 48 kHz -> 16 kHz -> ContentVec (ce qui est dit) + hauteur de la voix
    //   -> générateur RVC (la voix du personnage) -> raccord avec le bloc précédent -> 48 kHz -> moteur audio
    public sealed class VoiceCloneEngine : IDisposable
    {
        private const int DeviceRate = 48000;
        private const int ContentRate = 16000;
        private const int Decimation = DeviceRate / ContentRate;
        private const int BlockDevice = DeviceRate / 4;             // 250 ms de nouveau son par calcul
        private const int BlockContent = BlockDevice / Decimation;
        private const int WindowContent = ContentRate;              // 1 s analysée : 750 ms de contexte + le bloc
        private const int PitchHop = ContentRate / 100;             // Le générateur travaille à 100 trames par seconde
        private const float SilenceLevel = 0.003f;

        private InferenceSession _generator;
        private InferenceSession _content;
        private string _generatorPath;
        private string _contentPath;
        private bool _useGpu = true;
        private int _modelRate;

        private Thread _thread;
        private volatile bool _running;
        private volatile float _transpose;

        private readonly float[] _window = new float[WindowContent];
        private readonly float[] _decimatorTaps = BuildLowpass(63, 7200.0f / DeviceRate);
        private float[] _decimatorHistory;
        private float[] _previousTail = Array.Empty<float>();
        private double _resamplePosition;
        private float _resampleLast;
        private int _silentBlocks;
        private int _joinStart;
        private float _loudnessGain = 1.0f;
        private readonly Random _random = new Random();

        public event Action<string> StatusChanged;

        // Taille des vecteurs attendus par le générateur : 768 (RVC v2) ou 256 (RVC v1)
        public int FeatureSize { get; private set; }

        public bool IsLoaded => _generator != null && _content != null;

        // Décalage de hauteur appliqué à la voix convertie, en demi-tons
        public float Transpose
        {
            get => _transpose;
            set => _transpose = value;
        }

        // Ouvre le modèle du personnage et lit la taille des vecteurs qu'il attend
        public void LoadGenerator(string path)
        {
            Stop();
            _generator?.Dispose();
            _generator = null;
            _modelRate = 0;

            _generatorPath = path;
            _generator = CreateSession(path);

            if (!_generator.InputMetadata.TryGetValue("phone", out NodeMetadata phone))
            {
                throw new NotSupportedException("Ce fichier n'est pas un modèle RVC exporté en ONNX (entrée « phone » absente).");
            }
            FeatureSize = phone.Dimensions.Length == 3 && phone.Dimensions[2] > 0 ? phone.Dimensions[2] : 768;
        }

        // Ouvre le modèle de base ContentVec correspondant à FeatureSize
        public void LoadContent(string path)
        {
            _content?.Dispose();
            _content = null;
            _contentPath = path;
            _content = CreateSession(path);
        }

        public void Start()
        {
            if (!IsLoaded || _running) return;
            _running = true;
            _thread = new Thread(Run) { IsBackground = true, Name = "VoiceClone", Priority = ThreadPriority.AboveNormal };
            _thread.Start();
        }

        public void Stop()
        {
            _running = false;
            _thread?.Join();
            _thread = null;
        }

        public void Dispose()
        {
            Stop();
            _generator?.Dispose();
            _content?.Dispose();
        }

        private InferenceSession CreateSession(string path)
        {
            if (_useGpu)
            {
                try
                {
                    using var options = new SessionOptions
                    {
                        // Réglages exigés par DirectML
                        EnableMemoryPattern = false,
                        ExecutionMode = ExecutionMode.ORT_SEQUENTIAL,
                    };
                    options.AppendExecutionProvider_DML(0);
                    return new InferenceSession(path, options);
                }
                catch (OnnxRuntimeException)
                {
                    _useGpu = false;
                }
            }

            return new InferenceSession(path);
        }

        private void Run()
        {
            float[] readBuffer = new float[4096];
            float[] block = new float[BlockDevice];
            int filled = 0;

            Array.Clear(_window);
            _decimatorHistory = new float[_decimatorTaps.Length - 1];
            _previousTail = Array.Empty<float>();
            _resamplePosition = 0.0;
            _resampleLast = 0.0f;
            _loudnessGain = 1.0f;

            // Le son resté en attente avant le démarrage est jeté
            while (AudioCoreInterop.VC_ReadVoiceConversionInput(readBuffer, readBuffer.Length) > 0) { }

            var timer = new Stopwatch();
            double averageMs = 0.0;
            int blocks = 0;
            int measured = 0;

            while (_running)
            {
                int read = AudioCoreInterop.VC_ReadVoiceConversionInput(readBuffer, Math.Min(readBuffer.Length, BlockDevice - filled));
                if (read <= 0)
                {
                    Thread.Sleep(5);
                    continue;
                }
                Array.Copy(readBuffer, 0, block, filled, read);
                filled += read;
                if (filled < BlockDevice) continue;
                filled = 0;

                try
                {
                    timer.Restart();
                    float[] output = ProcessBlock(block);
                    AudioCoreInterop.VC_WriteVoiceConversionOutput(output, output.Length);
                    timer.Stop();

                    // Les blocs de silence ne passent pas par le modèle : ils ne comptent pas dans la mesure
                    if (_silentBlocks <= 2)
                    {
                        averageMs = measured == 0 ? timer.Elapsed.TotalMilliseconds : averageMs * 0.9 + timer.Elapsed.TotalMilliseconds * 0.1;
                        measured++;
                    }
                    if (++blocks % 4 == 1)
                    {
                        string device = _useGpu ? "carte graphique" : "processeur";
                        string speed = averageMs > 230.0 ? " — trop lent, le son va se couper" : "";
                        StatusChanged?.Invoke(measured == 0
                            ? $"Voix active ({device}) : en attente que tu parles."
                            : $"Voix active ({device}) : {averageMs:0} ms de calcul par bloc de 250 ms{speed}");
                    }
                }
                catch (OnnxRuntimeException ex) when (_useGpu)
                {
                    // Certaines cartes ou certains modèles ne passent pas en DirectML : on repasse sur le processeur
                    _useGpu = false;
                    try
                    {
                        _generator.Dispose();
                        _content.Dispose();
                        _generator = CreateSession(_generatorPath);
                        _content = CreateSession(_contentPath);
                        StatusChanged?.Invoke("Carte graphique indisponible (" + FirstLine(ex.Message) + "), calcul sur le processeur.");
                    }
                    catch (Exception inner)
                    {
                        StatusChanged?.Invoke("Erreur du modèle : " + FirstLine(inner.Message));
                        _running = false;
                    }
                }
                catch (Exception ex)
                {
                    StatusChanged?.Invoke("Erreur du modèle : " + FirstLine(ex.Message));
                    _running = false;
                }
            }
        }

        private static string FirstLine(string message)
        {
            int end = message.IndexOfAny(new[] { '\r', '\n' });
            return end < 0 ? message : message.Substring(0, end);
        }

        // Convertit un bloc de micro (48 kHz) en un bloc de voix du personnage (48 kHz)
        private float[] ProcessBlock(float[] block)
        {
            float[] block16 = Decimate(block);
            Array.Copy(_window, BlockContent, _window, 0, WindowContent - BlockContent);
            Array.Copy(block16, 0, _window, WindowContent - BlockContent, BlockContent);

            // Pendant les silences, le modèle n'est pas appelé : il produirait du souffle
            double energy = 0.0;
            foreach (float sample in block16) energy += sample * sample;
            _silentBlocks = Math.Sqrt(energy / BlockContent) < SilenceLevel ? _silentBlocks + 1 : 0;
            if (_silentBlocks > 2)
            {
                _previousTail = Array.Empty<float>();
                return new float[BlockDevice];
            }

            float[] audio = Infer();
            float[] joined = JoinWithPrevious(audio);
            MatchLoudness(joined);
            return ResampleToDevice(joined);
        }

        // Filtre passe-bas puis garde un échantillon sur trois (48 kHz -> 16 kHz)
        private float[] Decimate(float[] block)
        {
            int taps = _decimatorTaps.Length;
            float[] extended = new float[taps - 1 + block.Length];
            Array.Copy(_decimatorHistory, extended, taps - 1);
            Array.Copy(block, 0, extended, taps - 1, block.Length);
            Array.Copy(extended, extended.Length - (taps - 1), _decimatorHistory, 0, taps - 1);

            float[] output = new float[block.Length / Decimation];
            for (int k = 0; k < output.Length; k++)
            {
                float sum = 0.0f;
                int start = k * Decimation;
                for (int j = 0; j < taps; j++) sum += _decimatorTaps[j] * extended[start + j];
                output[k] = sum;
            }
            return output;
        }

        // Sinus cardinal fenêtré (Hamming) ; cutoff exprimé en fraction de la fréquence d'échantillonnage
        private static float[] BuildLowpass(int taps, float cutoff)
        {
            float[] h = new float[taps];
            float sum = 0.0f;
            for (int i = 0; i < taps; i++)
            {
                double x = i - (taps - 1) / 2.0;
                double sinc = x == 0.0 ? 2.0 * cutoff : Math.Sin(2.0 * Math.PI * cutoff * x) / (Math.PI * x);
                double window = 0.54 - 0.46 * Math.Cos(2.0 * Math.PI * i / (taps - 1));
                h[i] = (float)(sinc * window);
                sum += h[i];
            }
            for (int i = 0; i < taps; i++) h[i] /= sum;
            return h;
        }

        // Lance ContentVec puis le générateur sur la fenêtre courante ; renvoie le son à la fréquence du modèle
        private float[] Infer()
        {
            // 1. Contenu : ce qui est dit, indépendamment de la voix
            var contentInput = _content.InputMetadata.First();
            int[] contentShape = contentInput.Value.Dimensions.Length == 3
                ? new[] { 1, 1, WindowContent }
                : new[] { 1, WindowContent };
            var source = new DenseTensor<float>((float[])_window.Clone(), contentShape);

            float[] features;
            int contentFrames;
            using (var results = _content.Run(new[] { NamedOnnxValue.CreateFromTensor(contentInput.Key, source) }))
            {
                Tensor<float> tensor = results.First().AsTensor<float>();
                DenseTensor<float> dense = tensor as DenseTensor<float> ?? tensor.ToDenseTensor();
                ReadOnlySpan<int> dims = dense.Dimensions;
                if (dims.Length != 3) throw new NotSupportedException("Sortie inattendue du modèle de base.");

                ReadOnlySpan<float> data = dense.Buffer.Span;
                if (dims[2] == FeatureSize)
                {
                    contentFrames = dims[1];
                    features = data.ToArray();
                }
                else if (dims[1] == FeatureSize)
                {
                    // Sortie rangée [1, taille, trames] : on la remet en [trames, taille]
                    contentFrames = dims[2];
                    features = new float[contentFrames * FeatureSize];
                    for (int f = 0; f < contentFrames; f++)
                        for (int d = 0; d < FeatureSize; d++)
                            features[f * FeatureSize + d] = data[d * contentFrames + f];
                }
                else
                {
                    throw new NotSupportedException($"Le modèle de base produit des vecteurs de taille {dims[2]}, le personnage en attend {FeatureSize}.");
                }
            }

            // Le générateur attend deux fois plus de trames que ContentVec n'en produit
            int frames = contentFrames * 2;
            float[] phone = new float[frames * FeatureSize];
            for (int f = 0; f < frames; f++)
            {
                Array.Copy(features, (f / 2) * FeatureSize, phone, f * FeatureSize, FeatureSize);
            }

            // 2. Hauteur de la voix, décalée du nombre de demi-tons demandé
            float[] pitchHz = PitchTracker.Estimate(_window, ContentRate, frames, PitchHop, 120);
            float shift = MathF.Pow(2.0f, _transpose / 12.0f);
            long[] pitchCoarse = new long[frames];
            const float melMin = 69.07f;   // 1127 * ln(1 + 50 / 700)
            const float melMax = 1064.4f;  // 1127 * ln(1 + 1100 / 700)
            for (int f = 0; f < frames; f++)
            {
                pitchHz[f] *= shift;
                float mel = 1127.0f * MathF.Log(1.0f + pitchHz[f] / 700.0f);
                float coarse = pitchHz[f] > 0.0f ? (mel - melMin) * 254.0f / (melMax - melMin) + 1.0f : 1.0f;
                pitchCoarse[f] = (long)Math.Clamp(MathF.Round(coarse), 1.0f, 255.0f);
            }

            // 3. Générateur : assemble les entrées d'après ce que le modèle déclare
            var inputs = new List<NamedOnnxValue>();
            foreach (var input in _generator.InputMetadata)
            {
                switch (input.Key)
                {
                    case "phone":
                        inputs.Add(NamedOnnxValue.CreateFromTensor(input.Key, new DenseTensor<float>(phone, new[] { 1, frames, FeatureSize })));
                        break;
                    case "phone_lengths":
                        inputs.Add(CreateInteger(input, new long[] { frames }, new[] { 1 }));
                        break;
                    case "pitch":
                        inputs.Add(CreateInteger(input, pitchCoarse, new[] { 1, frames }));
                        break;
                    case "pitchf":
                        inputs.Add(NamedOnnxValue.CreateFromTensor(input.Key, new DenseTensor<float>(pitchHz, new[] { 1, frames })));
                        break;
                    case "ds":
                    case "sid":
                        inputs.Add(CreateInteger(input, new long[] { 0 }, new[] { 1 }));
                        break;
                    case "rnd":
                    {
                        int channels = input.Value.Dimensions.Length == 3 && input.Value.Dimensions[1] > 0 ? input.Value.Dimensions[1] : 192;
                        float[] noise = new float[channels * frames];
                        for (int i = 0; i < noise.Length; i++) noise[i] = NextGaussian();
                        inputs.Add(NamedOnnxValue.CreateFromTensor(input.Key, new DenseTensor<float>(noise, new[] { 1, channels, frames })));
                        break;
                    }
                    default:
                        throw new NotSupportedException($"Entrée de modèle non reconnue : « {input.Key} ».");
                }
            }

            float[] audio;
            using (var results = _generator.Run(inputs))
            {
                Tensor<float> tensor = results.First().AsTensor<float>();
                DenseTensor<float> dense = tensor as DenseTensor<float> ?? tensor.ToDenseTensor();
                audio = dense.Buffer.Span.ToArray();
            }

            if (_modelRate == 0)
            {
                // Fréquence du modèle déduite du nombre d'échantillons produits par trame
                double perFrame = (double)audio.Length / frames;
                _modelRate = new[] { 32000, 40000, 48000 }.OrderBy(rate => Math.Abs(rate / 100.0 - perFrame)).First();
            }

            // Certains exports sortent des entiers 16 bits plutôt que des valeurs entre -1 et 1
            float peak = 0.0f;
            foreach (float sample in audio) peak = Math.Max(peak, Math.Abs(sample));
            if (peak > 8.0f)
            {
                for (int i = 0; i < audio.Length; i++) audio[i] /= 32768.0f;
            }

            return audio;
        }

        private static NamedOnnxValue CreateInteger(KeyValuePair<string, NodeMetadata> input, long[] values, int[] shape)
        {
            if (input.Value.ElementType == typeof(int))
            {
                return NamedOnnxValue.CreateFromTensor(input.Key, new DenseTensor<int>(values.Select(v => (int)v).ToArray(), shape));
            }
            return NamedOnnxValue.CreateFromTensor(input.Key, new DenseTensor<long>(values, shape));
        }

        private float NextGaussian()
        {
            double u1 = 1.0 - _random.NextDouble();
            double u2 = _random.NextDouble();
            return (float)(Math.Sqrt(-2.0 * Math.Log(u1)) * Math.Cos(2.0 * Math.PI * u2));
        }

        // Garde la fin du son produit et la raccorde au bloc précédent (SOLA) :
        // on cherche le décalage où les deux formes d'onde se ressemblent le plus, puis on fait un fondu.
        private float[] JoinWithPrevious(float[] audio)
        {
            int block = _modelRate / 4;
            int fade = _modelRate * 40 / 1000;
            int search = _modelRate * 12 / 1000;
            int needed = block + fade + search;
            if (audio.Length < needed) throw new InvalidOperationException("Le modèle a produit trop peu de son.");

            int start = audio.Length - needed;
            int offset = 0;
            if (_previousTail.Length == fade)
            {
                float bestScore = float.NegativeInfinity;
                for (int k = 0; k <= search; k++)
                {
                    float correlation = 0.0f, energy = 1e-9f;
                    for (int i = 0; i < fade; i++)
                    {
                        float sample = audio[start + k + i];
                        correlation += sample * _previousTail[i];
                        energy += sample * sample;
                    }
                    float score = correlation / MathF.Sqrt(energy);
                    if (score > bestScore)
                    {
                        bestScore = score;
                        offset = k;
                    }
                }
            }
            start += offset;

            float[] output = new float[block];
            Array.Copy(audio, start, output, 0, block);
            for (int i = 0; i < fade; i++)
            {
                float fadeIn = 0.5f - 0.5f * MathF.Cos(MathF.PI * i / fade);
                float previous = _previousTail.Length == fade ? _previousTail[i] : 0.0f;
                output[i] = output[i] * fadeIn + previous * (1.0f - fadeIn);
            }

            if (_previousTail.Length != fade) _previousTail = new float[fade];
            Array.Copy(audio, start + block, _previousTail, 0, fade);
            _joinStart = start;
            return output;
        }

        // Le modèle ne respecte pas le volume d'origine (il parle fort même quand on chuchote, et souffle
        // dans les silences) : on fait suivre à la voix convertie l'enveloppe de volume du micro.
        private void MatchLoudness(float[] output)
        {
            int frame = _modelRate / 50; // Tranches de 20 ms
            int inputFrame = ContentRate / 50;
            double toInput = (double)ContentRate / _modelRate;

            for (int position = 0; position < output.Length; position += frame)
            {
                int count = Math.Min(frame, output.Length - position);
                double outputEnergy = 0.0;
                for (int i = 0; i < count; i++) outputEnergy += output[position + i] * output[position + i];

                int inputStart = Math.Clamp((int)((_joinStart + position) * toInput), 0, WindowContent - inputFrame);
                double inputEnergy = 0.0;
                for (int i = 0; i < inputFrame; i++) inputEnergy += _window[inputStart + i] * _window[inputStart + i];

                double outputLevel = Math.Sqrt(outputEnergy / count);
                double inputLevel = Math.Sqrt(inputEnergy / inputFrame);
                float target = outputLevel > 1e-4
                    ? (float)Math.Clamp(Math.Pow(inputLevel / outputLevel, 0.6), 0.0, 1.5)
                    : _loudnessGain;
                target = 0.5f * _loudnessGain + 0.5f * target;

                // Rampe d'une tranche à l'autre pour ne pas créer de marches
                for (int i = 0; i < count; i++)
                {
                    output[position + i] *= _loudnessGain + (target - _loudnessGain) * i / count;
                }
                _loudnessGain = target;
            }
        }

        // Interpolation linéaire continue d'un bloc à l'autre (fréquence du modèle -> 48 kHz)
        private float[] ResampleToDevice(float[] input)
        {
            if (_modelRate == DeviceRate) return input;

            double step = (double)_modelRate / DeviceRate;
            var output = new List<float>((int)(input.Length / step) + 2);

            // _resamplePosition est mesurée depuis l'échantillon qui précède ce bloc (_resampleLast)
            while (_resamplePosition < input.Length)
            {
                int index = (int)_resamplePosition;
                float frac = (float)(_resamplePosition - index);
                float a = index == 0 ? _resampleLast : input[index - 1];
                float b = input[index];
                output.Add(a + (b - a) * frac);
                _resamplePosition += step;
            }
            _resamplePosition -= input.Length;
            _resampleLast = input[input.Length - 1];
            return output.ToArray();
        }
    }
}
