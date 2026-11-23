cmake_minimum_required(VERSION 3.16)
project(odyssey3d C CXX)
file(READ "${CMAKE_SOURCE_DIR}/VERSION" ODYSSEY3D_VERSION_STRING)
string(STRIP "${ODYSSEY3D_VERSION_STRING}" ODYSSEY3D_VERSION_STRING)

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
pkg_check_modules(VULKAN REQUIRED vulkan)          # todo el dibujado va por Vulkan
pkg_check_modules(OPENCV opencv5)   # opcional: rastreo de cabeza (cara, ojos, triangulacion)

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
    foreach(p ${PROTO_PROTOCOLS})
      add_custom_command(
        OUTPUT ${PROTO_DIR}/${p}-client-protocol.h ${PROTO_DIR}/${p}-protocol.c
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_DIR}
        COMMAND ${WAYLAND_SCANNER} client-header ${PLASMA_PROTO_DIR}/${p}.xml ${PROTO_DIR}/${p}-client-protocol.h
        COMMAND ${WAYLAND_SCANNER} private-code ${PLASMA_PROTO_DIR}/${p}.xml ${PROTO_DIR}/${p}-protocol.c
        DEPENDS ${PLASMA_PROTO_DIR}/${p}.xml
        COMMENT "generando ${p} (wayland-scanner)")
      list(APPEND PROTO_GEN ${PROTO_DIR}/${p}-protocol.c)
    endforeach()
  endif()
endif()

add_executable(odyssey3d
  src/main.cpp
  src/util.cpp
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
)

add_dependencies(odyssey3d shaders)

target_compile_definitions(odyssey3d PRIVATE ODYSSEY3D_VERSION="${ODYSSEY3D_VERSION_STRING}")

if(WAYLAND_CLIENT_FOUND)
  target_sources(odyssey3d PRIVATE src/wm.cpp ${PROTO_GEN})
  if(PIPEWIRE_FOUND)
    target_sources(odyssey3d PRIVATE src/capture.cpp)
    target_compile_definitions(odyssey3d PRIVATE HAVE_CAPTURE=1)
  endif()
  target_compile_definitions(odyssey3d PRIVATE HAVE_WAYLAND_WM=1)
endif()

target_include_directories(odyssey3d PRIVATE src ${GLFW_INCLUDE_DIRS} ${CRYPTO_INCLUDE_DIRS} ${PROTO_DIR} ${SP_DIR} ${VULKAN_INCLUDE_DIRS})
target_link_libraries(odyssey3d PRIVATE ${GLFW_LIBRARIES} ${CRYPTO_LIBRARIES} ${VULKAN_LIBRARIES} GL EGL pthread m)
if(WAYLAND_CLIENT_FOUND)
  target_include_directories(odyssey3d PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
  target_link_libraries(odyssey3d PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
endif()
if(PIPEWIRE_FOUND)
  target_include_directories(odyssey3d PRIVATE ${PIPEWIRE_INCLUDE_DIRS} ${SPA_INCLUDE_DIRS})
  target_link_libraries(odyssey3d PRIVATE ${PIPEWIRE_LIBRARIES})
  target_compile_definitions(odyssey3d PRIVATE HAVE_PIPEWIRE=1)
endif()
if(LIBSYSTEMD_FOUND)
  target_sources(odyssey3d PRIVATE src/notify.cpp)
  target_include_directories(odyssey3d PRIVATE ${LIBSYSTEMD_INCLUDE_DIRS})
  target_link_libraries(odyssey3d PRIVATE ${LIBSYSTEMD_LIBRARIES})
  target_compile_definitions(odyssey3d PRIVATE HAVE_SDBUS=1)
endif()

if(MPV_FOUND)
  target_include_directories(odyssey3d PRIVATE ${MPV_INCLUDE_DIRS})
  target_link_libraries(odyssey3d PRIVATE ${MPV_LIBRARIES})
  target_compile_definitions(odyssey3d PRIVATE HAVE_MPV=1)
endif()
if(FREETYPE_FOUND)
  target_include_directories(odyssey3d PRIVATE ${FREETYPE_INCLUDE_DIRS})
  target_link_libraries(odyssey3d PRIVATE ${FREETYPE_LIBRARIES})
  target_compile_definitions(odyssey3d PRIVATE HAVE_FREETYPE=1)
endif()

if(OPENCV_FOUND)
  target_sources(odyssey3d PRIVATE src/track.cpp src/trackfeed.cpp)
  target_include_directories(odyssey3d PRIVATE ${OPENCV_INCLUDE_DIRS})
  target_link_directories(odyssey3d PRIVATE ${OPENCV_LIBRARY_DIRS})
  target_link_libraries(odyssey3d PRIVATE opencv_objdetect opencv_dnn opencv_geometry opencv_imgcodecs opencv_imgproc opencv_core)
  target_compile_definitions(odyssey3d PRIVATE HAVE_OPENCV=1)
endif()

if(PNG_FOUND)
  target_include_directories(odyssey3d PRIVATE ${PNG_INCLUDE_DIRS})
  target_link_libraries(odyssey3d PRIVATE ${PNG_LIBRARIES})
  target_compile_definitions(odyssey3d PRIVATE HAVE_PNG=1)
endif()

install(TARGETS odyssey3d RUNTIME DESTINATION bin)
install(FILES udev/70-odyssey-3d.rules DESTINATION /etc/udev/rules.d)
install(FILES calib/3DStackCorrection_A.png calib/3DStackCorrection_B.png DESTINATION share/odyssey3d/calib)
if(EXISTS "${CMAKE_SOURCE_DIR}/modelos/face_detection_yunet_2023mar.onnx")
  install(FILES modelos/face_detection_yunet_2023mar.onnx DESTINATION share/odyssey3d/modelos)
endif()
