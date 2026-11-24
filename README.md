# AudioTaggerTest

Detección de eventos sonoros (SED) con [PANNs](https://github.com/qiuqiangkong/panns_inference) aplicada a monitoreo de brazos roboticos

El repositorio tiene dos partes:

| Carpeta | Qué hay | Para qué |
|---|---|---|
| [`python/`](python/) | Scripts en Python, datos y resultados | Pruebas offline: PANNs sobre videos generados con IA y grabaciones del Dobot CR20 |
| [`cpp/PannsLive/`](cpp/PannsLive/) | Aplicación de escritorio en C++ con JUCE 8 | Detección en tiempo real sobre la entrada de audio, con video de una cámara y registro de eventos en CSV |

## Probar la parte de Python

Requisitos: Python 3.12.

```bash
cd python
python -m venv ../.venv
../.venv/Scripts/python -m pip install -r requirements.txt
../.venv/Scripts/python panns_sed.py
```

Este comando usa el CNN PANNS Inference para clasificar a partir del audio de los videos generados o grabados ubicados en la carpeta data/audios.

## Modelo PANNS Inference en tiempo real compilado con ONNX

Modelo PANNS en tiempo real funciona como una aplicación de escritorio hecha en JUCE 8 corre el modelo en tiempo real con ONNX Runtime. Analiza la entrada de audio en ventanas de 2s, toma el audio de un microfono predeterminado del sistema y registra cada evento etiquetandolo en pantalla y toda esta información se guarda en la carpeta /Log que se crea despues de ejecutar el .exe de la aplicación.

## Estructura

```
python/
  *.py                 un script por paso (ver python/README.md)
  data/                videos y audios extraídos
  results/             CSV, SRT y gráficas por audio
cpp/PannsLive/
  bin/                 app lista para usar: PannsLive.exe, DLL de ONNX Runtime y Visual C++, y Models/
  Source/              código de la app (la detección está en Source/PannsLive/)
  CMakeLists.txt       compilación con CMake; descarga JUCE 8.0.17-security y ONNX Runtime 1.23.2
  Models/              panns_labels.txt; aquí se pone el .onnx para compilar

```

## Licencias de terceros

- JUCE: AGPLv3 o licencia comercial.
- ONNX Runtime: MIT.
- PANNs y `panns_inference`: MIT.
- ASIO SDK de Steinberg (opcional, no incluido en el repo): GPLv3 o licencia propietaria de Steinberg.

