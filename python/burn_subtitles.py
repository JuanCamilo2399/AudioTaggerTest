"""Funciones opcionales para quemar el SRT de PANNs sobre el video para revisar las detecciones.

Los resultados quedan en el siguiente path:

data/<fuente>/<video>.mp4 + results/<fuente>/<video>/panns_events.srt
    -> results/<fuente>/<video>/<video>_subtitulado.mp4

Uso:
    python burn_subtitles.py                         # todos los videos que ya tengan SRT
    python burn_subtitles.py --source videos_dobot   # carpeta de videos del dobot CR 20
"""
import argparse
import subprocess

import imageio_ffmpeg  # trae su propio ffmpeg (viene con moviepy)

from config import DATA_DIR, PROJECT_DIR, RESULTS_DIR, VIDEO_EXTENSIONS, VIDEO_SOURCES

SRT_NAME = "panns_events.srt"
STYLE = "FontSize=16,Outline=2,MarginV=20"


def burn_subtitles(video, srt, out):
    # El filtro "subtitles" no acepta bien rutas de Windows ("D:" choca con su sintaxis),
    # por eso ffmpeg se corre dentro de la carpeta del SRT y se le pasa solo el nombre.
    cmd = [
        imageio_ffmpeg.get_ffmpeg_exe(), "-y", "-loglevel", "error",
        "-i", str(video),
        "-vf", f"subtitles={srt.name}:force_style='{STYLE}'",
        "-c:a", "copy",
        str(out),
    ]
    subprocess.run(cmd, cwd=srt.parent, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", choices=VIDEO_SOURCES, help="procesar solo esta carpeta de videos")
    parser.add_argument("--srt", default=SRT_NAME,
                        help=f"nombre del SRT dentro de results/<fuente>/<video>/ (por defecto {SRT_NAME})")
    args = parser.parse_args()

    for source in [args.source] if args.source else VIDEO_SOURCES:
        for video in sorted(p for p in (DATA_DIR / source).glob("*") if p.suffix.lower() in VIDEO_EXTENSIONS):
            srt = RESULTS_DIR / source / video.stem / args.srt
            if not srt.exists():
                print(f"  = {video.name}: no hay {args.srt}")
                continue
            suffix = "subtitulado" if args.srt == SRT_NAME else srt.stem
            out = srt.parent / f"{video.stem}_{suffix}.mp4"
            burn_subtitles(video, srt, out)
            print(f"  + {out.relative_to(PROJECT_DIR)}")


if __name__ == "__main__":
    main()
