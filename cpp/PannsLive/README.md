# PANNs Live

Aplicación de escritorio en C++ (JUCE 8) que detecta eventos sonoros en tiempo real con PANNs. Muestra arriba el video de una cámara y abajo un registro de eventos con hora de inicio, hora de fin, etiqueta y score, estos eventos se guardan en la carpeta \Logs en formato .CSV.

## Requisitos

- Windows 10 u 11 de 64 bits.
- Un micrófono o interfaz de audio. La cámara es opcional.

## Compilar

Con Visual Studio 2022 y CMake 3.22 o más reciente. La carpeta de compilación debe ir sin tildes:

```bash
cmake -S cpp/PannsLive -B C:/build/PannsLive -G "Visual Studio 17 2022" -A x64
cmake --build C:/build/PannsLive --config Release
```

En Release, `bin/` queda listo para usar.

### Con ASIO (opcional)

Por defecto la app usa Windows Audio (WASAPI). Para usar el driver ASIO de una interfaz:

1. Descarga el ASIO SDK de Steinberg (versión con licencia GPLv3) y descomprímelo fuera del repositorio, por ejemplo en `C:/SDKs/asiosdk`. No lo subas al repo.
2. Agrega la ruta al configurar:

```bash
cmake -S cpp/PannsLive -B C:/build/PannsLive -G "Visual Studio 17 2022" -A x64 -DPANNS_ASIO_SDK_DIR=C:/SDKs/asiosdk
cmake --build C:/build/PannsLive --config Release
```

Al configurar debe aparecer `ASIO activado con ...`. En la pestaña **Settings**, el tipo de dispositivo muestra entonces "ASIO" además de "Windows Audio". Para volver a compilar sin ASIO, configura con `-DPANNS_ASIO_SDK_DIR=`.

## Cómo funciona

```
callback de audio ──FIFO──▶ hilo PannsWorker ──cola──▶ interfaz (10 Hz)
 mezcla a mono               pasa bajos 14.4 kHz,        registro + CSV
 + hora de inicio            remuestreo a 32 kHz,
                             ventana de 2 s cada 0.5 s,
                             ONNX Runtime, SedEventTracker
```

- **Modelo**: `Cnn14_DecisionLevelMax` de PANNs, el mismo que usa `python/panns_sed.py`, exportado a ONNX. Recibe audio crudo a 32 kHz de cualquier duración y devuelve la probabilidad por frame (100 frames/s) de las 527 clases de AudioSet.
- **Ventanas**: cada 0.5 s se analiza una ventana de 2 s y se conservan solo los frames 100 a 150, que tienen contexto a ambos lados. Un evento aparece cerca de 1 s después de terminar.
- **Eventos**: `SedEventTracker` reproduce frame a frame la función `framewise_out_to_events` de Python (umbral 0.15, duración mínima 0.10 s, unión de huecos menores a 0.05 s) sobre las 527 clases.
- **Hora de cada evento**: hora de la primera muestra más el número de frame dividido entre 100; no depende de cuánto tarde la inferencia.

| Archivo (`Source/PannsLive/`) | Función |
|---|---|
| `PannsAudioFifo.h` | FIFO compartida entre el callback de audio y el hilo de análisis |
| `PannsLiveInput.h` | Procesador de audio del modo en vivo |
| `PannsSedModel.h/.cpp` | Envoltorio de ONNX Runtime |
| `PannsWorker.h/.cpp` | Remuestreo, ventanas, inferencia y publicación de eventos |
| `SedEventTracker.h` | Extracción de eventos en streaming |
| `PannsLiveComponent.h/.cpp` | Pestaña de la interfaz: cámara, controles, registro y CSV |

