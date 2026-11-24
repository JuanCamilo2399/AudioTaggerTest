# PANNs Live: detección de eventos sonoros en tiempo real con PANNs (JUCE 8 + ONNX Runtime).
#
# Compilar (Windows x64, Visual Studio 2022). La carpeta de compilación debe ir sin tildes:
#   cmake -S cpp/PannsLive -B C:/build/PannsLive -G "Visual Studio 17 2022" -A x64
#   cmake --build C:/build/PannsLive --config Release
#
# Con ASIO, agregar al primer comando la carpeta del ASIO SDK (versión GPLv3 de Steinberg):
#   -DPANNS_ASIO_SDK_DIR=C:/SDKs/asiosdk
#
# Al compilar en Release, cpp/PannsLive/bin/ queda autocontenido: PannsLive.exe, las DLL de ONNX
# Runtime y de Visual C++ y Models/ (modelo + etiquetas). JUCE y ONNX Runtime se descargan solos
# al configurar; no hace falta Projucer.

cmake_minimum_required(VERSION 3.22)
project(PannsLive VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# juceaide (la herramienta de JUCE que genera los recursos) falla si la carpeta de compilación
# tiene tildes u otros caracteres no ASCII, por ejemplo "Investigación".
string(REGEX MATCH "[^\t -~]" PANNS_NON_ASCII "${CMAKE_BINARY_DIR}")
if(PANNS_NON_ASCII)
    message(FATAL_ERROR "La carpeta de compilación tiene caracteres no ASCII: ${CMAKE_BINARY_DIR}\n"
                        "Usa una sin tildes, por ejemplo:  cmake -S <esta carpeta> -B C:/build/PannsLive")
endif()

# --- JUCE --------------------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.17-security
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(JUCE)

# --- ONNX Runtime (paquete oficial de NuGet, CPU, Windows x64) ---------------------------------
set(ORT_VERSION 1.23.2)
set(ORT_DIR "${CMAKE_BINARY_DIR}/_deps/onnxruntime-${ORT_VERSION}")
set(ORT_NATIVE "${ORT_DIR}/runtimes/win-x64/native")

if(NOT EXISTS "${ORT_NATIVE}/onnxruntime.dll")
    message(STATUS "Descargando ONNX Runtime ${ORT_VERSION} desde NuGet")
    file(DOWNLOAD "https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime/${ORT_VERSION}"
         "${CMAKE_BINARY_DIR}/_deps/onnxruntime-${ORT_VERSION}.zip"
         SHOW_PROGRESS TLS_VERIFY ON STATUS ORT_STATUS)
    list(GET ORT_STATUS 0 ORT_CODE)
    if(NOT ORT_CODE EQUAL 0)
        message(FATAL_ERROR "No se pudo descargar ONNX Runtime: ${ORT_STATUS}")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${CMAKE_BINARY_DIR}/_deps/onnxruntime-${ORT_VERSION}.zip" DESTINATION "${ORT_DIR}")
endif()

add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION             "${ORT_NATIVE}/onnxruntime.dll"
    IMPORTED_IMPLIB               "${ORT_NATIVE}/onnxruntime.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${ORT_DIR}/build/native/include")
set(ORT_DLLS "${ORT_NATIVE}/onnxruntime.dll" "${ORT_NATIVE}/onnxruntime_providers_shared.dll")

# --- ASIO (opcional) ---------------------------------------------------------------------------
set(PANNS_ASIO_SDK_DIR "" CACHE PATH "Carpeta del ASIO SDK de Steinberg (contiene common/iasiodrv.h). Vacío = sin ASIO")
set(PANNS_USE_ASIO 0)
set(PANNS_ASIO_INCLUDE_DIR "")

if(PANNS_ASIO_SDK_DIR)
    if(EXISTS "${PANNS_ASIO_SDK_DIR}/common/iasiodrv.h")
        set(PANNS_ASIO_INCLUDE_DIR "${PANNS_ASIO_SDK_DIR}/common")
    elseif(EXISTS "${PANNS_ASIO_SDK_DIR}/iasiodrv.h")
        set(PANNS_ASIO_INCLUDE_DIR "${PANNS_ASIO_SDK_DIR}")
    else()
        message(FATAL_ERROR "No se encontró iasiodrv.h en ${PANNS_ASIO_SDK_DIR} ni en ${PANNS_ASIO_SDK_DIR}/common")
    endif()
    set(PANNS_USE_ASIO 1)
    message(STATUS "ASIO activado con ${PANNS_ASIO_INCLUDE_DIR}")
else()
    message(STATUS "ASIO desactivado (usa -DPANNS_ASIO_SDK_DIR=<carpeta del SDK> para activarlo)")
endif()

# --- Aplicación --------------------------------------------------------------------------------
juce_add_gui_app(PannsLive
    PRODUCT_NAME "PANNs Live"
    COMPANY_NAME "JuanCamilo2399"
    VERSION      ${PROJECT_VERSION})
set_target_properties(PannsLive PROPERTIES OUTPUT_NAME "PannsLive")

juce_generate_juce_header(PannsLive)

target_sources(PannsLive PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/PannsLive/PannsLiveComponent.cpp
    Source/PannsLive/PannsSedModel.cpp
    Source/PannsLive/PannsWorker.cpp)

target_compile_definitions(PannsLive PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PannsLive,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PannsLive,JUCE_VERSION>"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=0
    JUCE_ASIO=${PANNS_USE_ASIO}
    JUCE_JACK=0
    JUCE_USE_CAMERA=1)

if(PANNS_USE_ASIO)
    target_include_directories(PannsLive PRIVATE "${PANNS_ASIO_INCLUDE_DIR}")
endif()

target_link_libraries(PannsLive
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_video
        onnxruntime
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# DLL de ONNX Runtime y modelo junto al ejecutable, para poder ejecutarlo desde la carpeta de compilación
add_custom_command(TARGET PannsLive POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ORT_DLLS} "$<TARGET_FILE_DIR:PannsLive>")

# --- Release: copiar todo a cpp/PannsLive/bin/ para que quede listo para usar ------------------
set(PANNS_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")

# DLL del runtime de Visual C++ (las necesitan la app y ONNX Runtime) para que funcione sin instalar nada
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)
include(InstallRequiredSystemLibraries)

file(GLOB PANNS_MODEL_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Models/*.onnx"
    "${CMAKE_CURRENT_SOURCE_DIR}/Models/*.txt")

set(PANNS_BIN_FILES_Q "")
foreach(f IN LISTS ORT_DLLS CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
    string(APPEND PANNS_BIN_FILES_Q " \"${f}\"")
endforeach()
set(PANNS_MODEL_FILES_Q "")
foreach(f IN LISTS PANNS_MODEL_FILES)
    string(APPEND PANNS_MODEL_FILES_Q " \"${f}\"")
endforeach()

file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/copy_to_bin_$<CONFIG>.cmake" CONTENT
"if(\"$<CONFIG>\" STREQUAL \"Release\")
    file(MAKE_DIRECTORY \"${PANNS_BIN_DIR}/Models\")
    foreach(f \"$<TARGET_FILE:PannsLive>\"${PANNS_BIN_FILES_Q})
        get_filename_component(n \"\${f}\" NAME)
        file(COPY_FILE \"\${f}\" \"${PANNS_BIN_DIR}/\${n}\" ONLY_IF_DIFFERENT)
    endforeach()
    foreach(f${PANNS_MODEL_FILES_Q})
        get_filename_component(n \"\${f}\" NAME)
        file(COPY_FILE \"\${f}\" \"${PANNS_BIN_DIR}/Models/\${n}\" ONLY_IF_DIFFERENT)
    endforeach()
    message(STATUS \"PANNs Live listo en ${PANNS_BIN_DIR}\")
endif()
")

add_custom_command(TARGET PannsLive POST_BUILD
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/copy_to_bin_$<CONFIG>.cmake")
