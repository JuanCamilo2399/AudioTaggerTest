"""Rutas del proyecto. Todo es relativo a esta carpeta, sin importar desde dónde se corra."""
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent
DATA_DIR = PROJECT_DIR / "data"
AUDIOS_DIR = DATA_DIR / "audios"       # data/audios/<fuente>/<video>.wav
RESULTS_DIR = PROJECT_DIR / "results"  # results/<fuente>/<audio>/...

# Una carpeta por fuente de videos dentro de data/
VIDEO_SOURCES = ["videos_generados_ia", "videos_dobot"]
VIDEO_EXTENSIONS = {".mp4", ".mov", ".mkv", ".avi"}


def list_audios(source=None):
    """Lista los .wav de data/audios/ (de una fuente o de todas), ordenados."""
    sources = [source] if source else VIDEO_SOURCES
    audios = []
    for src in sources:
        audios += sorted((AUDIOS_DIR / src).glob("*.wav"))
    return audios


def results_dir_for(audio_path):
    """results/<fuente>/<nombre_audio>/ para un wav de data/audios/<fuente>/."""
    audio_path = Path(audio_path)
    out_dir = RESULTS_DIR / audio_path.parent.name / audio_path.stem
    out_dir.mkdir(parents=True, exist_ok=True)
    return out_dir
