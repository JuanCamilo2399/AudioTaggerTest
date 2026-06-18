
from moviepy import VideoFileClip
import numpy as np
from numpy.typing import NDArray
from scipy.io import wavfile
from scipy.signal import resample

def extract_audio_from_mp4(video_path : str, audio_path : str):
    video = VideoFileClip(video_path)

    if video.audio is None:
        raise ValueError("These video doesn't have audio")
    else:
       video.audio.write_audiofile(audio_path)
    
    video.close()

def from_data_to_float32(x : NDArray) -> NDArray:

    if x.dtype == np.float32:
        return x
    
    elif x.dtype == np.int32:
        return x.astype(np.float32) / 2147483648.0
    elif x.dtype == np.int16:
        return x.astype(np.float32) / 32768.0
    elif x.dtype == np.uint8:
        return 2.0 * x.astype(np.float32) / 255.0 - 1.0
    else:
        raise ValueError("Unknow data type.")


def get_data_from_wav(path_array:  str):
    fs, data = wavfile.read(path_array, 'rb')
    
    return fs, data

def to_wav_file(data : np.ndarray, fs : float, out_path : str):
    wavfile.write(out_path, fs, data)


def get_channel_with_higher_energy(data: np.ndarray) -> np.ndarray:
    """
    Returns the channel with the highest energy.

    Expected input shape:
        data.shape = (channels, samples)
    """

    if data.ndim == 1:
        return data  # already mono

    ch, _ = data.shape

    if ch == 1:
        return data[0]

    # Energy per channel: sum of squared samples
    energies = np.sum(data ** 2, axis=1)

    # Index of channel with maximum energy
    max_energy_channel = int(np.argmax(energies))

    return data[max_energy_channel]
    
def load_wav_16k_mono(filepath: str) -> np.ndarray:
    """Read a WAV file and return a float32 mono waveform at 16 kHz in [-1, 1]."""
    sr, wav = wavfile.read(filepath)
    if wav.ndim > 1:                        # stereo → mono
        wav = wav.mean(axis=1)
    wav = wav.astype(np.float32)
    desired_sample_rate = 16_000


    if sr != desired_sample_rate:                        # resample if needed
        n_samples = int(len(wav) * desired_sample_rate / sr)
        wav = resample(wav, n_samples).astype(np.float32)
    if np.abs(wav).max() > 1.0:            # int16 → float normalisation
        wav /= 32_768.0
    return desired_sample_rate, wav    

def ensure_sample_rate(original_sample_rate, waveform,
                       desired_sample_rate=16000):
  """Resample waveform if required."""
  if original_sample_rate != desired_sample_rate:
    desired_length = int(round(float(len(waveform)) /
                               original_sample_rate * desired_sample_rate))
    waveform = resample(waveform, desired_length)
  return desired_sample_rate, waveform

