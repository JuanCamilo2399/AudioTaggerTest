from audio_video_utils import extract_audio_from_mp4, get_data_from_wav, from_data_to_float32
import os
import numpy as np
from pathlib import Path

CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
# PROJECT_ROOT = os.path.dirname(CURRENT_DIR)

audios_dir = Path(CURRENT_DIR) / "Video_Audios"
audios_dir.mkdir(parents=True, exist_ok=True)
videos_path = Path(CURRENT_DIR) / "Videos"
file_names = os.listdir(videos_path)

# Commented out audios have been extracted already 
for i, name in enumerate(file_names):
    print(f"Number of videos : {len(file_names)}")
    video_path = videos_path / name
    audio_name = "audio_from_video" + "_" + str(i) + ".wav"
    audio_path = audios_dir / audio_name
    extract_audio_from_mp4(video_path=video_path, audio_path=audio_path)

# audio_files = os.listdir(audios_dir)

# for audio_file in audio_files:
#     audio_path = audios_dir / audio_file
#     fs, audio = get_data_from_wav(audio_path)
#     ch, samples = audio.shape

#     print(f"Audio fs : {fs}, Audio number of channels : {ch}, audio samples : {samples}")

    