"""Comparación: audio tagging con YAMNet (TensorFlow Hub, 16 kHz).

Por cada wav en data/audios/<fuente>/ guarda en results/<fuente>/<audio>/:
    yamnet.png   forma de onda, espectrograma log-mel y scores de las 10 clases principales

Uso:
    python yamnet_tagging.py                         # todos los audios
    python yamnet_tagging.py --source videos_dobot
    python yamnet_tagging.py --audio data/audios/videos_generados_ia/video_0.wav --show
"""
import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import tensorflow as tf
import tensorflow_hub as hub

from audio_video_utils import load_wav_16k_mono
from config import PROJECT_DIR, VIDEO_SOURCES, list_audios, results_dir_for

TOP_N = 10


def class_names_from_csv(class_map_csv_text):
    """Lista de nombres de clase en el mismo orden que el vector de scores."""
    with tf.io.gfile.GFile(class_map_csv_text) as csvfile:
        return [row["display_name"] for row in csv.DictReader(csvfile)]


def plot_yamnet(wav_data, spectrogram_np, scores_np, class_names, title, out_path, show=False):
    mean_scores = np.mean(scores_np, axis=0)
    top_class_indices = np.argsort(mean_scores)[::-1][:TOP_N]
    top_class_names = [class_names[i] for i in top_class_indices]

    fig = plt.figure(figsize=(12, 8))
    fig.suptitle(title)

    # 1. Forma de onda
    plt.subplot(3, 1, 1)
    plt.plot(wav_data)
    plt.xlim([0, len(wav_data)])
    plt.title("Waveform")
    plt.xlabel("Samples")
    plt.ylabel("Amplitude")

    # 2. Espectrograma log-mel
    plt.subplot(3, 1, 2)
    plt.imshow(spectrogram_np.T, aspect="auto", interpolation="nearest", origin="lower")
    plt.title("YAMNet Log-Mel Spectrogram")
    plt.xlabel("Time frames")
    plt.ylabel("Mel bands")

    # 3. Scores de YAMNet
    plt.subplot(3, 1, 3)
    plt.imshow(scores_np[:, top_class_indices].T, aspect="auto", interpolation="nearest", cmap="gray_r")
    plt.title("YAMNet Scores - Top Classes")
    plt.xlabel("Time frames")
    plt.ylabel("Classes")
    plt.yticks(np.arange(TOP_N), top_class_names)

    plt.tight_layout()
    fig.savefig(out_path)
    if show:
        plt.show()
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", choices=VIDEO_SOURCES, help="procesar solo los audios de esta fuente")
    parser.add_argument("--audio", type=Path, help="procesar un solo wav (dentro de data/audios/<fuente>/)")
    parser.add_argument("--show", action="store_true", help="además de guardar, mostrar la figura")
    args = parser.parse_args()

    audios = [args.audio.resolve()] if args.audio else list_audios(args.source)
    if not audios:
        print("No hay audios. Corre primero extract_audio.py")
        return

    model = hub.load("https://tfhub.dev/google/yamnet/1")
    class_names = class_names_from_csv(model.class_map_path().numpy())

    for audio_path in audios:
        fs, wav_data = load_wav_16k_mono(audio_path)
        print(f"\n===== {audio_path.relative_to(PROJECT_DIR)} =====")
        print(f"Sample rate: {fs} Hz | duración: {len(wav_data) / fs:.2f}s | muestras: {len(wav_data)}")

        scores, _embeddings, spectrogram = model(wav_data)
        scores_np = scores.numpy()
        print(f"Sonido principal: {class_names[scores_np.mean(axis=0).argmax()]}")

        out_path = results_dir_for(audio_path) / "yamnet.png"
        plot_yamnet(wav_data, spectrogram.numpy(), scores_np, class_names, audio_path.stem, out_path, args.show)
        print(f"Figura en {out_path.relative_to(PROJECT_DIR)}")


if __name__ == "__main__":
    main()
