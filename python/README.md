# AudioTaggerTest: parte en Python

Pruebas de audio tagging y sound event detection (SED) sobre videos, con
[PANNs](https://github.com/qiuqiangkong/panns_inference) (CNN14) y YAMNet como comparación.

## Instalación

Python 3.12. Desde esta carpeta:

```bash
python -m venv ../.venv
../.venv/Scripts/python -m pip install -r requirements.txt
```

## Flujo

1. `python extract_audio.py`: extrae el audio de cada video y lo guarda en `data/audios/<fuente>/<video>.wav`.
2. `python panns_sed.py`: corre PANNs (etiquetado del clip y eventos por frame) y guarda los resultados en `results/<fuente>/<audio>/`.
3. `python burn_subtitles.py` (opcional): quema el SRT de PANNs sobre el video y lo guarda como `<video>_subtitulado.mp4`.
4. `python yamnet_tagging.py` (opcional): genera la figura de YAMNet en la misma carpeta, para comparar.

Los scripts aceptan `--source <fuente>`; los de modelos aceptan además `--audio <wav>`. Todos tienen `--help`.

## Estructura

```
data/
  videos_generados_ia/   videos generados con IA (Sora 2 Pro vía ElevenLabs)
  videos_dobot/          videos propios del Dobot
  audios/<fuente>/       wav extraídos (mismo nombre que el video)
results/<fuente>/<audio>/
  panns_tagging.csv  panns_events.csv  panns_events.srt  panns_sed.png  yamnet.png
config.py                rutas del proyecto
audio_video_utils.py     funciones de lectura y escritura de audio
```

Los videos subtitulados no se suben al repositorio; se regeneran con `burn_subtitles.py`.
