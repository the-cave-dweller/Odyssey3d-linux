cmake_minimum_required(VERSION 3.16)
project(odysseyvk C CXX)
file(READ "${CMAKE_SOURCE_DIR}/VERSION" ODYSSEYVK_VERSION_STRING)
string(STRIP "${ODYSSEYVK_VERSION_STRING}" ODYSSEYVK_VERSION_STRING)
# Si cambia el archivo VERSION, hay que volver a configurar: si no, el binario sigue llevando la
# version vieja (lo vimos: con VERSION en 0.26 el instalado decia 0.25).
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(CRYPTO REQUIRED libcrypto)
pkg_check_modules(MPV mpv)          # opcional: reproducción de video SBS
pkg_check_modules(PNG libpng)      # opcional: mapas de corrección del panel
pkg_check_modules(FREETYPE freetype2)  # opcional: atlas de fuentes para la interfaz
pkg_check_modules(WAYLAND_CLIENT wayland-client)   # vigilancia de ventanas / captura
pkg_check_modules(PIPEWIRE libpipewire-0.3)        # opcional: fotogramas de la captura
pkg_check_modules(LIBSYSTEMD libsystemd)           # opcional: aviso por portal (sd-bus)
pkg_check_modules(XKBCOMMON xkbcommon)             # opcional: que simbolo da Mayus+numero (atajos globales)
pkg_check_modules(VULKAN REQUIRED vulkan)          # todo el dibujado va por Vulkan
pkg_check_modules(OPENCV opencv5)   # opcional: rastreo de cabeza (cara, ojos, triangulacion)

# --- conversor 2D a 3D (opcional): ONNX Runtime + un modelo de profundidad --------
# Con ONNX Runtime para CUDA (paquete onnxruntime-cuda) un cuadro de 518x518 sale en unos
# 7 milisegundos en una tarjeta NVIDIA; sin tarjeta se hace con la CPU y va mucho mas lento.
find_path(ORT_DIR NAMES onnxruntime/onnxruntime_cxx_api.h)
find_library(ORT_LIB NAMES onnxruntime)

# --- shaders: GLSL -> SPIR-V -> cabecera C++ -------------------------------------
# Se compilan durante la propia compilación y se incrustan como bytes: el programa no
# lleva ficheros de shaders encima ni tiene que buscar rutas.
find_program(GLSLANG glslangValidator)
if(NOT GLSLANG)
  message(FATAL_ERROR "Falta glslangValidator (paquetes glslang o shaderc): sin el no se compilan los shaders Vulkan")
endif()
set(SP_DIR ${CMAKE_BINARY_DIR}/shadersp)
file(MAKE_DIRECTORY ${SP_DIR})
set(SP_FILES
  "fs.vert|fsVertSpv"
  "probe.frag|probeFragSpv"
  "weave.vert|weaveVertSpv"
  "weave.frag|weaveFragSpv"
  "hud.vert|hudVertSpv"
  "hud.frag|hudFragSpv"
)
set(SP_GEN "")
foreach(entry ${SP_FILES})
  string(REPLACE "|" ";" partes "${entry}")
  list(GET partes 0 archivo)
  list(GET partes 1 variable)
  get_filename_component(nombre "${archivo}" NAME_WE)
  get_filename_component(extension "${archivo}" EXT)
  string(SUBSTRING "${extension}" 1 -1 extension)
  set(spv "${SP_DIR}/${nombre}_${extension}.spv")
  set(hdr "${SP_DIR}/${nombre}_${extension}_sp.h")
  add_custom_command(
    OUTPUT "${hdr}"
    COMMAND ${GLSLANG} -V "${CMAKE_SOURCE_DIR}/shaders/${archivo}" -o "${spv}"
    COMMAND ${CMAKE_COMMAND} -DSRC=${spv} -DNAME=${variable} -DDST=${hdr} -P "${CMAKE_SOURCE_DIR}/cmake/embed.cmake"
    DEPENDS "${CMAKE_SOURCE_DIR}/shaders/${archivo}" "${CMAKE_SOURCE_DIR}/cmake/embed.cmake"
    COMMENT "compilando shaders/${archivo} a SPIR-V")
  list(APPEND SP_GEN "${hdr}")
endforeach()
add_custom_target(shaders ALL DEPENDS ${SP_GEN})

# --- protocolos de KWin (ventanas y captura) -------------------------------------
# XMLs del paquete plasma-wayland-protocols. LGPL-2.1-or-later: los generamos en el
# arranque de la compilación y no los repartimos con el programa.
set(PROTO_PROTOCOLS plasma-window-management zkde-screencast-unstable-v1)
set(PROTO_DIR ${CMAKE_BINARY_DIR}/proto)
set(PROTO_GEN "")
if(WAYLAND_CLIENT_FOUND)
  find_program(WAYLAND_SCANNER wayland-scanner)
  find_file(PLASMA_WM_XML NAMES plasma-window-management.xml
            PATHS /usr/share/plasma-wayland-protocols /usr/local/share/plasma-wayland-protocols)
  if(NOT WAYLAND_SCANNER OR NOT PLASMA_WM_XML)
    message(WARNING "Faltan wayland-scanner o plasma-wayland-protocols: el servicio de ventanas a pantalla completa no se compilara")
    set(WAYLAND_CLIENT_FOUND OFF)
  else()
    get_filename_component(PLASMA_PROTO_DIR ${PLASMA_WM_XML} DIRECTORY)
    set(PROTO_XML_plasma-window-management ${PLASMA_PROTO_DIR}/plasma-window-management.xml)
    set(PROTO_XML_zkde-screencast-unstable-v1 ${PLASMA_PROTO_DIR}/zkde-screencast-unstable-v1.xml)
    # wlr-layer-shell no esta en el paquete wayland-protocols: lo llevamos nosotros
    # (la copia es la de swaybg, MIT; ver protocols/LEEME.txt).
    set(PROTO_XML_wlr-layer-shell-unstable-v1 ${CMAKE_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml)
    list(APPEND PROTO_PROTOCOLS wlr-layer-shell-unstable-v1)
    # layer-shell menciona xdg_popup en get_popup, así que el código generado
    # enlaza contra la interfaz de xdg-shell aunque nunca usemos ese pedido
    find_file(XDG_SHELL_XML NAMES xdg-shell.xml
              PATHS /usr/share/wayland-protocols/stable/xdg-shell /usr/local/share/wayland-protocols/stable/xdg-shell)
    if(XDG_SHELL_XML)
      set(PROTO_XML_xdg-shell ${XDG_SHELL_XML})
      list(APPEND PROTO_PROTOCOLS xdg-shell)
    else()
      message(STATUS "sin xdg-shell.xml: layer-shell no se puede generar")
    endif()
    # escala fraccionaria: sirve para que la interfaz se vea nitida cuando el escritorio
    # esta por ejemplo al 150 %. Si la distribucion no la trae, usamos la escala entera.
    find_file(WP_FRACTIONAL_XML NAMES fractional-scale-v1.xml
              PATHS /usr/share/wayland-protocols/staging/fractional-scale)
    # el tamano logico del monitor (zxdg_output) es la unica forma fiable de saber la
    # escala real: con el escritorio al 150 % el wl_output anuncia escala entera 2
    find_file(XDG_OUTPUT_XML NAMES xdg-output-unstable-v1.xml
              PATHS /usr/share/wayland-protocols/unstable/xdg-output)
    if(XDG_OUTPUT_XML)
      set(PROTO_XML_xdg-output-unstable-v1 ${XDG_OUTPUT_XML})
      list(APPEND PROTO_PROTOCOLS xdg-output-unstable-v1)
    endif()
    find_file(WP_VIEWPORTER_XML NAMES viewporter.xml
              PATHS /usr/share/wayland-protocols/stable/viewporter)
    if(WP_FRACTIONAL_XML AND WP_VIEWPORTER_XML)
      set(PROTO_XML_fractional-scale-v1 ${WP_FRACTIONAL_XML})
      set(PROTO_XML_viewporter ${WP_VIEWPORTER_XML})
      list(APPEND PROTO_PROTOCOLS fractional-scale-v1 viewporter)
      add_compile_definitions(HAVE_FRACTIONAL_SCALE=1)
    else()
      message(STATUS "sin fractional-scale/viewporter: la superposicion usara la escala entera del monitor")
    endif()
    foreach(p ${PROTO_PROTOCOLS})
      add_custom_command(
        OUTPUT ${PROTO_DIR}/${p}-client-protocol.h ${PROTO_DIR}/${p}-protocol.c
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_DIR}
        COMMAND ${WAYLAND_SCANNER} client-header ${PROTO_XML_${p}} ${PROTO_DIR}/${p}-client-protocol.h
        COMMAND ${WAYLAND_SCANNER} private-code ${PROTO_XML_${p}} ${PROTO_DIR}/${p}-protocol.c
        DEPENDS ${PROTO_XML_${p}}
        COMMENT "generando ${p} (wayland-scanner)")
      list(APPEND PROTO_GEN ${PROTO_DIR}/${p}-protocol.c)
    endforeach()
  endif()
endif()

find_package(OpenMP)

add_executable(odysseyvk
  src/main.cpp
  src/util.cpp
  src/ajuste.cpp
  src/sha3.c
  src/lens.cpp
  src/scaler.cpp
  src/ircam.cpp
  src/calib.cpp
  src/camera.cpp
  src/panel.cpp
  src/glutil.cpp
  src/weave.cpp
  src/testsrc.cpp
  src/mpvsrc.cpp
  src/glimport.cpp
  src/viewer.cpp
  src/vkdev.cpp
  src/vkprobe.cpp
  src/vkweave.cpp
  src/vkui.cpp
  src/session.cpp
  src/vkview.cpp
  src/mpris.cpp
  src/atajos.cpp
  src/barra.cpp
  src/preferencias.cpp
  src/depth.cpp
  src/hoyos.cpp
)

add_dependencies(odysseyvk shaders)

target_compile_definitions(odysseyvk PRIVATE ODYSSEYVK_VERSION="${ODYSSEYVK_VERSION_STRING}")

if(WAYLAND_CLIENT_FOUND)
  target_sources(odysseyvk PRIVATE src/wm.cpp src/overlay.cpp ${PROTO_GEN})
  if(PIPEWIRE_FOUND)
    target_sources(odysseyvk PRIVATE src/capture.cpp)
    target_compile_definitions(odysseyvk PRIVATE HAVE_CAPTURE=1)
  endif()
  target_compile_definitions(odysseyvk PRIVATE HAVE_WAYLAND_WM=1)
endif()

target_include_directories(odysseyvk PRIVATE src ${GLFW_INCLUDE_DIRS} ${CRYPTO_INCLUDE_DIRS} ${PROTO_DIR} ${SP_DIR} ${VULKAN_INCLUDE_DIRS})
if(OpenMP_CXX_FOUND)
  target_link_libraries(odysseyvk PRIVATE OpenMP::OpenMP_CXX)
endif()

target_link_libraries(odysseyvk PRIVATE ${GLFW_LIBRARIES} ${CRYPTO_LIBRARIES} ${VULKAN_LIBRARIES} GL EGL pthread m)
if(WAYLAND_CLIENT_FOUND)
  target_include_directories(odysseyvk PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
  target_link_libraries(odysseyvk PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
endif()
if(PIPEWIRE_FOUND)
  target_include_directories(odysseyvk PRIVATE ${PIPEWIRE_INCLUDE_DIRS} ${SPA_INCLUDE_DIRS})
  target_link_libraries(odysseyvk PRIVATE ${PIPEWIRE_LIBRARIES})
  target_compile_definitions(odysseyvk PRIVATE HAVE_PIPEWIRE=1)
endif()
if(LIBSYSTEMD_FOUND)
  target_sources(odysseyvk PRIVATE src/notify.cpp)
  target_include_directories(odysseyvk PRIVATE ${LIBSYSTEMD_INCLUDE_DIRS})
  target_link_libraries(odysseyvk PRIVATE ${LIBSYSTEMD_LIBRARIES})
  target_compile_definitions(odysseyvk PRIVATE HAVE_SDBUS=1)
endif()
if(XKBCOMMON_FOUND)
  target_include_directories(odysseyvk PRIVATE ${XKBCOMMON_INCLUDE_DIRS})
  target_link_libraries(odysseyvk PRIVATE ${XKBCOMMON_LIBRARIES})
  target_compile_definitions(odysseyvk PRIVATE HAVE_XKBCOMMON=1)
endif()

if(MPV_FOUND)
  target_include_directories(odysseyvk PRIVATE ${MPV_INCLUDE_DIRS})
  target_link_libraries(odysseyvk PRIVATE ${MPV_LIBRARIES})
  target_compile_definitions(odysseyvk PRIVATE HAVE_MPV=1)
endif()
if(FREETYPE_FOUND)
  target_include_directories(odysseyvk PRIVATE ${FREETYPE_INCLUDE_DIRS})
  target_link_libraries(odysseyvk PRIVATE ${FREETYPE_LIBRARIES})
  target_compile_definitions(odysseyvk PRIVATE HAVE_FREETYPE=1)
endif()

if(OPENCV_FOUND)
  target_sources(odysseyvk PRIVATE src/track.cpp src/trackfeed.cpp)
  target_include_directories(odysseyvk PRIVATE ${OPENCV_INCLUDE_DIRS})
  target_link_directories(odysseyvk PRIVATE ${OPENCV_LIBRARY_DIRS})
  target_link_libraries(odysseyvk PRIVATE opencv_objdetect opencv_dnn opencv_geometry opencv_imgcodecs opencv_imgproc opencv_photo opencv_core)
  target_compile_definitions(odysseyvk PRIVATE HAVE_OPENCV=1)
endif()

if(PNG_FOUND)
  target_include_directories(odysseyvk PRIVATE ${PNG_INCLUDE_DIRS})
  target_link_libraries(odysseyvk PRIVATE ${PNG_LIBRARIES})
  target_compile_definitions(odysseyvk PRIVATE HAVE_PNG=1)
endif()

if(ORT_DIR AND ORT_LIB)
  target_include_directories(odysseyvk PRIVATE ${ORT_DIR}/onnxruntime)
  target_link_libraries(odysseyvk PRIVATE ${ORT_LIB} dl)
  target_compile_definitions(odysseyvk PRIVATE HAVE_ORT=1)
  message(STATUS "ONNX Runtime encontrado (${ORT_LIB}): el conversor 2D a 3D se compilara")
else()
  message(STATUS "Sin ONNX Runtime: el conversor 2D a 3D no estara disponible (paquete onnxruntime-cuda)")
endif()

# --- la ventana de ajustes (opcional): Qt6, proceso APARTE --------------------------------
# Sirve para elegir la red de profundidad (.onnx) y mover los numeros de la trama con la
# superposicion en marcha: el visor se queda el teclado de la pantalla del panel y ahi no se
# puede escribir nada. No enlaza con el visor: habla con el por los tres ficheros de
# src/ajuste.h (~/.local/state/odysseyvk/ajuste, estado y preferencias.log), asi que no puede
# romper la trama ni colgar el 3D. Sin Qt6 el proyecto se configura y compila igual.
option(ODYSSEYVK_UI "Ventana de ajustes por Qt (proceso aparte, habla con el visor por ficheros)" ON)
if(ODYSSEYVK_UI)
  find_package(Qt6 COMPONENTS Widgets QUIET)
  if(Qt6_FOUND)
    add_executable(odysseyvk-ajustes src/ui_ajustes.cpp src/ajuste.cpp src/util.cpp src/camera.cpp)
    target_include_directories(odysseyvk-ajustes PRIVATE src)
    target_link_libraries(odysseyvk-ajustes PRIVATE Qt6::Widgets)
    add_dependencies(odysseyvk-ajustes odysseyvk)   # primero el visor: la ventana sin visor no tiene nada que manejar
    message(STATUS "Ventana de ajustes: con Qt6 ${Qt6_VERSION} -> odysseyvk-ajustes")
    install(TARGETS odysseyvk-ajustes RUNTIME DESTINATION bin)   # la ventana tambien va a /usr/local/bin
  else()
    message(STATUS "Ventana de ajustes: sin Qt6, se omite (el visor y el hub se construyen igual)")
  endif()
endif()

install(TARGETS odysseyvk RUNTIME DESTINATION bin)
install(FILES udev/70-odysseyvk.rules DESTINATION /etc/udev/rules.d)
install(FILES calib/3DStackCorrection_A.png calib/3DStackCorrection_B.png DESTINATION share/odysseyvk/calib)
if(EXISTS "${CMAKE_SOURCE_DIR}/modelos/face_detection_yunet_2023mar.onnx")
  install(FILES modelos/face_detection_yunet_2023mar.onnx DESTINATION share/odysseyvk/modelos)
endif()
