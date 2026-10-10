using System;

namespace VoiceChangerApp.VoiceClone
{
    // Détection de hauteur par autocorrélation normalisée (même méthode que les plugins de voix)
    public static class PitchTracker
    {
        private const float MinPitch = 65.0f;
        private const float MaxPitch = 500.0f;

        // Renvoie la hauteur (Hz) de chaque trame, 0 pour les trames non voisées.
        // La trame i est centrée sur l'échantillon firstCenter + i * hop.
        public static float[] Estimate(float[] audio, int sampleRate, int frameCount, int hop, int firstCenter)
        {
            int minLag = (int)(sampleRate / MaxPitch);
            int maxLag = (int)(sampleRate / MinPitch);
            int window = sampleRate * 32 / 1000;
            int span = window + maxLag + 1;

            float[] pitch = new float[frameCount];
            float[] nsdf = new float[maxLag + 2];

            for (int frame = 0; frame < frameCount; frame++)
            {
                int start = firstCenter + frame * hop - span / 2;
                start = Math.Clamp(start, 0, Math.Max(0, audio.Length - span));
                if (start + span > audio.Length) break;

                float energy = 0.0f;
                for (int i = 0; i < window; i++) energy += audio[start + i] * audio[start + i];
                if (energy / window < 1e-6f) continue; // Silence

                float best = 0.0f;
                for (int lag = minLag - 1; lag <= maxLag; lag++)
                {
                    float acf = 0.0f, norm = 0.0f;
                    for (int i = 0; i < window; i++)
                    {
                        float a = audio[start + i], b = audio[start + i + lag];
                        acf += a * b;
                        norm += a * a + b * b;
                    }
                    nsdf[lag] = 2.0f * acf / (norm + 1e-12f);
                    if (lag >= minLag && nsdf[lag] > best) best = nsdf[lag];
                }
                if (best < 0.5f) continue; // Son non voisé

                // Premier pic assez proche du maximum : évite de se tromper d'une octave
                for (int lag = minLag; lag < maxLag; lag++)
                {
                    if (nsdf[lag] >= 0.85f * best && nsdf[lag] >= nsdf[lag - 1] && nsdf[lag] >= nsdf[lag + 1])
                    {
                        float a = nsdf[lag - 1], b = nsdf[lag], c = nsdf[lag + 1];
                        float denom = a - 2.0f * b + c;
                        float offset = Math.Abs(denom) > 1e-9f ? 0.5f * (a - c) / denom : 0.0f;
                        pitch[frame] = sampleRate / (lag + offset);
                        break;
                    }
                }
            }

            return MedianFilter(pitch);
        }

        // Filtre médian sur 3 trames : supprime les erreurs isolées
        private static float[] MedianFilter(float[] pitch)
        {
            float[] result = new float[pitch.Length];
            for (int i = 0; i < pitch.Length; i++)
            {
                float a = pitch[Math.Max(i - 1, 0)], b = pitch[i], c = pitch[Math.Min(i + 1, pitch.Length - 1)];
                result[i] = Math.Max(Math.Min(a, b), Math.Min(Math.Max(a, b), c));
            }
            return result;
        }
    }
}
