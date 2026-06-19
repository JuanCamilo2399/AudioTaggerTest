import os
import matplotlib.pyplot as plt
import numpy as np
import librosa
import panns_inference
import csv
from audio_video_utils import second_to_srt_time
from panns_inference import AudioTagging, SoundEventDetection, labels
from pathlib import Path

def print_audio_tagging_result(clipwise_output):
    """Visualization of audio tagging result.

    Args:
      clipwise_output: (classes_num,)
    """
    sorted_indexes = np.argsort(clipwise_output)[::-1]

    # Print audio tagging top probabilities
    for k in range(10):
        print('{}: {:.3f}'.format(np.array(labels)[sorted_indexes[k]], 
            clipwise_output[sorted_indexes[k]]))


def plot_sound_event_detection_result(framewise_output):
    """Visualization of sound event detection result. 

    Args:
      framewise_output: (time_steps, classes_num)
    """
    out_fig_path = 'results/sed_result.png'
    os.makedirs(os.path.dirname(out_fig_path), exist_ok=True)

    classwise_output = np.max(framewise_output, axis=0) # (classes_num,)

    idxes = np.argsort(classwise_output)[::-1]
    idxes = idxes[0:5]

    ix_to_lb = {i : label for i, label in enumerate(labels)}
    lines = []
    for idx in idxes:
        line, = plt.plot(framewise_output[:, idx], label=ix_to_lb[idx])
        lines.append(line)

    plt.legend(handles=lines)
    plt.xlabel('Frames')
    plt.ylabel('Probability')
    plt.ylim(0, 1.)
    plt.savefig(out_fig_path)
    print('Save fig to {}'.format(out_fig_path))

def framewise_out_to_events(
        framewise_output,
        labels,
        audio_duration,
        threshold=0.25,
        min_event_duration=0.2,
        merge_gap=0.15,
        max_classes = 10
):
    
    # obtaining size of frame-classes matrix
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
            
            prob_frame_inactive = True if not active or i == total_frames else False 
            if prob_frame_inactive and start_idx is not None:
                end_idx = i if not active else i + 1

                # save active class frame in seconds
                start_time = frame_times[start_idx]
                end_time = frame_times[min(end_idx, total_frames)]
                duration = end_time - start_time

                if duration >= min_event_duration:
                    score = float(np.max(probs[start_idx:end_idx]))
                
                    events.append({
                        "label": labels[class_idx],
                        "start" : float(start_time),
                        "end": float(end_time),
                        "score": score
                    })

                start_idx = None
    # Sort by time
    events = sorted(events, key=lambda x: (x["start"], -x["score"]))

    # Merge close events of the same class
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

def save_events_to_csv(events, out_path="results/sed_events.csv"):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    with open(out_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["start", "end", "label", "score"])
        writer.writeheader()

        for event in events:
            writer.writerow({
                "start": round(event["start"], 3),
                "end": round(event["end"], 3),
                "label": event["label"],
                "score": round(event["score"], 3)
            })

    print(f"Saved CSV markers to {out_path}")


def save_events_to_srt(events, out_path="results/sed_events.srt"):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    with open(out_path, "w", encoding="utf-8") as f:
        for i, event in enumerate(events, start=1):
            start = second_to_srt_time(event["start"])
            end = second_to_srt_time(event["end"])
            text = f"{event['label']} ({event['score']:.2f})"

            f.write(f"{i}\n")
            f.write(f"{start} --> {end}\n")
            f.write(f"{text}\n\n")

    print(f"Saved subtitles to {out_path}")

CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
audios_dir = Path(CURRENT_DIR) / "Video_Audios"
file_names = os.listdir(audios_dir)
audio_file = file_names[3]
audio_path = audios_dir / audio_file
device = 'cpu' # 'cuda' | 'cpu'

(audio, _) = librosa.core.load(audio_path, sr=32000, mono=True)
audio = audio[None, :]  # (batch_size, segment_samples)

print('------ Audio tagging ------')
at = AudioTagging(checkpoint_path=None, device=device)
(clipwise_output, embedding) = at.inference(audio)
"""clipwise_output: (batch_size, classes_num), embedding: (batch_size, embedding_size)"""

print_audio_tagging_result(clipwise_output[0])

print('------ Sound event detection ------')
sed = SoundEventDetection(
    checkpoint_path=None, 
    device=device, 
    interpolate_mode='nearest', # 'nearest'
)
framewise_output = sed.inference(audio)
"""(batch_size, time_steps, classes_num)"""

audio_duration = audio.shape[1] / 32000
events = framewise_out_to_events(
    framewise_output=framewise_output[0],
    labels=labels,
    audio_duration=audio_duration,
    threshold=0.15,
    min_event_duration=0.10,
    merge_gap=0.05,
    max_classes=10
)

print('------ framewise events ------')
for event in events:
    print(
        f"{event['start']:.2f}s - {event['end']:.2f}s | "
        f"{event['label']} | {event['score']:.3f}"
    )

save_events_to_csv(events)
save_events_to_srt(events)

plot_sound_event_detection_result(framewise_output[0])