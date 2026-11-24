"""Funciones de apoyo para leer/escribir audio y extraerlo de videos."""
import numpy as np
from moviepy import VideoFileClip
from numpy.typing import NDArray
from scipy.io import wavfile
from scipy.signal import resample


def extract_audio_from_mp4(video_path, audio_path):
    """Guarda la pista de audio de un video como .wav (PCM 16 bits)."""
    video = VideoFileClip(str(video_path))
    try:
        if video.audio is None:
            raise ValueError("el video no tiene audio")
        video.audio.write_audiofile(str(audio_path), logger=None)
    finally:
        video.close()


def from_data_to_float32(x: NDArray) -> NDArray:
    """Convierte muestras enteras/flotantes de un wav a float32 en [-1, 1]."""
    if x.dtype == np.float32:
        return x
    elif x.dtype == np.float64:
        return x.astype(np.float32)
    elif x.dtype == np.int32:
        return x.astype(np.float32) / 2147483648.0
    elif x.dtype == np.int16:
        return x.astype(np.float32) / 32768.0
    elif x.dtype == np.uint8:
        return 2.0 * x.astype(np.float32) / 255.0 - 1.0
    else:
        raise ValueError(f"Tipo de dato no soportado: {x.dtype}")


def get_data_from_wav(path):
    """Lee un wav. Devuelve (fs, data) con data de forma (muestras,) o (muestras, canales)."""
    fs, data = wavfile.read(path)
    return fs, data


def to_wav_file(data: np.ndarray, fs: int, out_path):
    wavfile.write(out_path, fs, data)


def get_channel_with_higher_energy(data: np.ndarray) -> np.ndarray:
    """Devuelve el canal con mayor energía.

    Espera la forma que entrega scipy.io.wavfile: (muestras, canales).
    """
    if data.ndim == 1:
        return data  # ya es mono

    # float64 para que int16**2 no se desborde
    energies = np.sum(data.astype(np.float64) ** 2, axis=0)
    return data[:, int(np.argmax(energies))]


def load_wav_16k_mono(filepath, desired_sample_rate=16_000):
    """Lee un wav y lo devuelve como (16000, forma de onda float32 mono en [-1, 1])."""
    sr, wav = wavfile.read(filepath)
    wav = from_data_to_float32(wav)
    if wav.ndim > 1:  # estéreo -> mono
        wav = wav.mean(axis=1)
    if sr != desired_sample_rate:
        n_samples = int(len(wav) * desired_sample_rate / sr)
        wav = resample(wav, n_samples).astype(np.float32)
    return desired_sample_rate, wav


def ensure_sample_rate(original_sample_rate, waveform, desired_sample_rate=16000):
    """Remuestrea la forma de onda si hace falta."""
    if original_sample_rate != desired_sample_rate:
        desired_length = int(round(float(len(waveform)) / original_sample_rate * desired_sample_rate))
        waveform = resample(waveform, desired_length)
    return desired_sample_rate, waveform


def second_to_srt_time(seconds):
    """Convierte segundos al formato de tiempo SRT (HH:MM:SS,mmm)."""
    ms = int((seconds - int(seconds)) * 1e3)
    total_seconds = int(seconds)

    h = total_seconds // 3600
    m = (total_seconds % 3600) // 60
    s = total_seconds % 60

    return f"{h:02d}:{m:02d}:{s:02d},{ms:03d}"
