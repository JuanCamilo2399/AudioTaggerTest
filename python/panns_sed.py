"""Paso 2: audio tagging + sound event detection (SED) con PANNs (CNN14).

https://github.com/qiuqiangkong/panns_inference

Por cada wav en data/audios/<fuente>/ guarda en results/<fuente>/<audio>/:
    panns_tagging.csv   top 10 etiquetas del clip completo (AudioTagging)
    panns_events.csv    eventos detectados: start, end, label, score (SoundEventDetection)
    panns_events.srt    los mismos eventos como subtítulos, para ponerlos sobre el video
    panns_sed.png       probabilidad por frame de las 5 clases principales

Uso:
    python panns_sed.py                                   # todos los audios
    python panns_sed.py --source videos_dobot             # solo una fuente
    python panns_sed.py --audio data/audios/videos_generados_ia/video_3.wav
    python panns_sed.py --threshold 0.2 --device cuda
"""
import argparse
import csv
from pathlib import Path

import librosa
import matplotlib

matplotlib.use("Agg")  # solo guardar figuras, sin abrir ventanas
import matplotlib.pyplot as plt
import numpy as np
from panns_inference import AudioTagging, SoundEventDetection, labels

from audio_video_utils import second_to_srt_time
from config import PROJECT_DIR, VIDEO_SOURCES, list_audios, results_dir_for

SAMPLE_RATE = 32000  # PANNs CNN14 se entrenó a 32 kHz


def top_tags(clipwise_output, k=10):
    """Top k (etiqueta, probabilidad) del audio tagging. clipwise_output: (classes_num,)"""
    idxs = np.argsort(clipwise_output)[::-1][:k]
    return [(labels[i], float(clipwise_output[i])) for i in idxs]


def plot_sound_event_detection_result(framewise_output, audio_duration, out_fig_path, top_n=5):
    """Grafica la probabilidad por frame de las top_n clases. framewise_output: (time_steps, classes_num)"""
    classwise_output = np.max(framewise_output, axis=0)  # (classes_num,)
    idxes = np.argsort(classwise_output)[::-1][:top_n]
    times = np.linspace(0, audio_duration, framewise_output.shape[0])

    fig, ax = plt.subplots(figsize=(10, 4))
    for idx in idxes:
        ax.plot(times, framewise_output[:, idx], label=labels[idx])
    ax.legend()
    ax.set_xlabel("Tiempo (s)")
    ax.set_ylabel("Probabilidad")
    ax.set_ylim(0, 1.0)
    ax.set_title(Path(out_fig_path).parent.name)
    fig.tight_layout()
    fig.savefig(out_fig_path)
    plt.close(fig)


def framewise_out_to_events(
    framewise_output,
    labels,
    audio_duration,
    threshold=0.25,
    min_event_duration=0.2,
    merge_gap=0.15,
    max_classes=10,
):
    """Convierte la salida por frame (time_steps, classes_num) en eventos con inicio y fin.

    Para las max_classes clases con mayor pico: marca los frames con prob >= threshold,
    agrupa frames consecutivos en eventos, descarta los más cortos que min_event_duration
    y une eventos de la misma clase separados por menos de merge_gap segundos.
    """
    time_steps, classes_num = framewise_output.shape
    frame_times = np.linspace(0, audio_duration, time_steps)

    classwise_max = np.max(framewise_output, axis=0)
    top_class_indexes = np.argsort(classwise_max)[::-1][:max_classes]

    events = []
    for class_idx in top_class_indexes:
        probs = framewise_output[:, class_idx]
        active_class_in_frames = probs >= threshold
        total_frames = len(active_class_in_frames) - 1
        start_idx = None
        for i, active in enumerate(active_class_in_frames):
            if active and start_idx is None:
                start_idx = i

            prob_frame_inactive = not active or i == total_frames
            if prob_frame_inactive and start_idx is not None:
                end_idx = i if not active else i + 1

                # guardar el evento en segundos
                start_time = frame_times[start_idx]
                end_time = frame_times[min(end_idx, total_frames)]
                duration = end_time - start_time

                if duration >= min_event_duration:
                    score = float(np.max(probs[start_idx:end_idx]))
                    events.append({
                        "label": labels[class_idx],
                        "start": float(start_time),
                        "end": float(end_time),
                        "score": score,
                    })

                start_idx = None

    # Ordenar por tiempo
    events = sorted(events, key=lambda x: (x["start"], -x["score"]))

    # Unir eventos cercanos de la misma clase
    merged_events = []
    for event in events:
        if not merged_events:
            merged_events.append(event)
            continue

        last = merged_events[-1]
        same_label = event["label"] == last["label"]
        close_enough = event["start"] - last["end"] <= merge_gap

        if same_label and close_enough:
            last["end"] = max(last["end"], event["end"])
            last["score"] = max(last["score"], event["score"])
        else:
            merged_events.append(event)

    return merged_events


def save_tags_to_csv(tags, out_path):
    with open(out_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["label", "score"])
        for label, score in tags:
            writer.writerow([label, round(score, 3)])


def save_events_to_csv(events, out_path):
    with open(out_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["start", "end", "label", "score"])
        writer.writeheader()
        for event in events:
            writer.writerow({
                "start": round(event["start"], 3),
                "end": round(event["end"], 3),
                "label": event["label"],
                "score": round(event["score"], 3),
            })


def save_events_to_srt(events, out_path):
    with open(out_path, "w", encoding="utf-8") as f:
        for i, event in enumerate(events, start=1):
            start = second_to_srt_time(event["start"])
            end = second_to_srt_time(event["end"])
            f.write(f"{i}\n{start} --> {end}\n{event['label']} ({event['score']:.2f})\n\n")


def process_audio(audio_path, at, sed, args):
    print(f"\n===== {audio_path.relative_to(PROJECT_DIR)} =====")
    out_dir = results_dir_for(audio_path)

    audio, _ = librosa.load(audio_path, sr=SAMPLE_RATE, mono=True)
    audio = audio[None, :]  # (batch_size, segment_samples)
    audio_duration = audio.shape[1] / SAMPLE_RATE

    # Audio tagging: una probabilidad por clase para todo el clip
    clipwise_output, _embedding = at.inference(audio)  # (batch_size, classes_num)
    tags = top_tags(clipwise_output[0])
    print("--- Audio tagging (top 10) ---")
    for label, score in tags:
        print(f"{label}: {score:.3f}")

    # Sound event detection: probabilidad por frame y por clase
    framewise_output = sed.inference(audio)[0]  # (time_steps, classes_num)
    events = framewise_out_to_events(
        framewise_output=framewise_output,
        labels=labels,
        audio_duration=audio_duration,
        threshold=args.threshold,
        min_event_duration=args.min_event_duration,
        merge_gap=args.merge_gap,
        max_classes=args.max_classes,
    )
    print(f"--- Eventos ({len(events)}) ---")
    for event in events:
        print(f"{event['start']:.2f}s - {event['end']:.2f}s | {event['label']} | {event['score']:.3f}")

    save_tags_to_csv(tags, out_dir / "panns_tagging.csv")
    save_events_to_csv(events, out_dir / "panns_events.csv")
    save_events_to_srt(events, out_dir / "panns_events.srt")
    plot_sound_event_detection_result(framewise_output, audio_duration, out_dir / "panns_sed.png")
    print(f"Resultados en {out_dir.relative_to(PROJECT_DIR)}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", choices=VIDEO_SOURCES, help="procesar solo los audios de esta fuente")
    parser.add_argument("--audio", type=Path, help="procesar un solo wav (dentro de data/audios/<fuente>/)")
    parser.add_argument("--device", default="cpu", choices=["cpu", "cuda"])
    parser.add_argument("--threshold", type=float, default=0.15)
    parser.add_argument("--min-event-duration", type=float, default=0.10)
    parser.add_argument("--merge-gap", type=float, default=0.05)
    parser.add_argument("--max-classes", type=int, default=10)
    args = parser.parse_args()

    audios = [args.audio.resolve()] if args.audio else list_audios(args.source)
    if not audios:
        print("No hay audios. Corre primero extract_audio.py")
        return

    # Los checkpoints se descargan solos a ~/panns_data la primera vez
    at = AudioTagging(checkpoint_path=None, device=args.device)
    sed = SoundEventDetection(checkpoint_path=None, device=args.device, interpolate_mode="nearest")

    for audio_path in audios:
        process_audio(audio_path, at, sed, args)


if __name__ == "__main__":
    main()
