import tensorflow as tf
import tensorflow_hub as hub
import numpy as np
import csv
import os
import matplotlib.pyplot as plt
from IPython.display import Audio
from scipy.io import wavfile
from pathlib import Path
from audio_video_utils import ensure_sample_rate, load_wav_16k_mono
# Load the model.
model = hub.load('https://tfhub.dev/google/yamnet/1')

def class_names_from_csv(class_map_csv_text):
  """Returns list of class names corresponding to score vector."""
  class_names = []
  with tf.io.gfile.GFile(class_map_csv_text) as csvfile:
    reader = csv.DictReader(csvfile)
    for row in reader:
      class_names.append(row['display_name'])

  return class_names



class_map_path = model.class_map_path().numpy()
class_names = class_names_from_csv(class_map_path)

CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
audios_dir = Path(CURRENT_DIR) / "Video_Audios"
file_names = os.listdir(audios_dir)
audio_file = file_names[0]
audio_path = audios_dir / audio_file
fs, wav_data = load_wav_16k_mono(audio_path)
# Show some basic information about the audio.
duration = len(wav_data)/fs
print(f'Sample rate: {fs} Hz')
print(f'Total duration: {duration:.2f}s')
print(f'Size of the input: {len(wav_data)}')


scores, embeddings, spectrogram = model(wav_data)
scores_np = scores.numpy()
spectrogram_np = spectrogram.numpy()
infered_class = class_names[scores_np.mean(axis=0).argmax()]
print(f'The main sound is: {infered_class}')

plt.figure(figsize=(12, 8))

# Scores promedio
mean_scores = np.mean(scores_np, axis=0)

top_n = 10
top_class_indices = np.argsort(mean_scores)[::-1][:top_n]
top_class_names = [class_names[i] for i in top_class_indices]

# -------------------------
# 1. Waveform
# -------------------------
plt.subplot(3, 1, 1)
plt.plot(wav_data)
plt.xlim([0, len(wav_data)])
plt.title("Waveform")
plt.xlabel("Samples")
plt.ylabel("Amplitude")

# -------------------------
# 2. Log-mel spectrogram
# -------------------------
plt.subplot(3, 1, 2)
plt.imshow(
    spectrogram_np.T,
    aspect='auto',
    interpolation='nearest',
    origin='lower'
)
plt.title("YAMNet Log-Mel Spectrogram")
plt.xlabel("Time frames")
plt.ylabel("Mel bands")

# -------------------------
# 3. YAMNet scores
# -------------------------
plt.subplot(3, 1, 3)
plt.imshow(
    scores_np[:, top_class_indices].T,
    aspect='auto',
    interpolation='nearest',
    cmap='gray_r'
)

plt.title("YAMNet Scores - Top Classes")
plt.xlabel("Time frames")
plt.ylabel("Classes")

yticks = np.arange(top_n)
plt.yticks(yticks, top_class_names)

plt.tight_layout()
plt.show()
