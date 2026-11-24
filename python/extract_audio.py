"""Paso 1: extrae el audio (.wav) de cada video.

data/<fuente>/<video>.mp4  ->  data/audios/<fuente>/<video>.wav

Uso:
    python extract_audio.py                         # todas las fuentes
    python extract_audio.py --source videos_dobot   # solo una
    python extract_audio.py --overwrite             # re-extraer aunque ya exista
"""
import argparse

from audio_video_utils import extract_audio_from_mp4
from config import AUDIOS_DIR, DATA_DIR, PROJECT_DIR, VIDEO_EXTENSIONS, VIDEO_SOURCES


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", choices=VIDEO_SOURCES, help="procesar solo esta carpeta de videos")
    parser.add_argument("--overwrite", action="store_true", help="re-extraer aunque el .wav ya exista")
    args = parser.parse_args()

    for source in [args.source] if args.source else VIDEO_SOURCES:
        videos = sorted(p for p in (DATA_DIR / source).glob("*") if p.suffix.lower() in VIDEO_EXTENSIONS)
        out_dir = AUDIOS_DIR / source
        out_dir.mkdir(parents=True, exist_ok=True)
        print(f"[{source}] {len(videos)} videos")

        for video in videos:
            wav = out_dir / f"{video.stem}.wav"
            if wav.exists() and not args.overwrite:
                print(f"  = {wav.name} ya existe, se omite")
                continue
            try:
                extract_audio_from_mp4(video, wav)
                print(f"  + {video.name} -> {wav.relative_to(PROJECT_DIR)}")
            except ValueError as e:
                print(f"  ! {video.name}: {e}")


if __name__ == "__main__":
    main()
