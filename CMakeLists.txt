cmake_minimum_required(VERSION 3.16)
project(odyssey3d C CXX)

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

add_executable(odyssey3d
  src/main.cpp
  src/util.cpp
  src/sha3.c
  src/lens.cpp
  src/scaler.cpp
  src/ircam.cpp
  src/panel.cpp
  src/glutil.cpp
  src/weave.cpp
  src/testsrc.cpp
  src/mpvsrc.cpp
  src/viewer.cpp
)

target_include_directories(odyssey3d PRIVATE src ${GLFW_INCLUDE_DIRS} ${CRYPTO_INCLUDE_DIRS})
target_link_libraries(odyssey3d PRIVATE ${GLFW_LIBRARIES} ${CRYPTO_LIBRARIES} GL EGL pthread m)

if(MPV_FOUND)
  target_include_directories(odyssey3d PRIVATE ${MPV_INCLUDE_DIRS})
  target_link_libraries(odyssey3d PRIVATE ${MPV_LIBRARIES})
  target_compile_definitions(odyssey3d PRIVATE HAVE_MPV=1)
endif()
if(PNG_FOUND)
  target_include_directories(odyssey3d PRIVATE ${PNG_INCLUDE_DIRS})
  target_link_libraries(odyssey3d PRIVATE ${PNG_LIBRARIES})
  target_compile_definitions(odyssey3d PRIVATE HAVE_PNG=1)
endif()

install(TARGETS odyssey3d RUNTIME DESTINATION bin)
install(FILES udev/70-odyssey-3d.rules DESTINATION /etc/udev/rules.d)
install(FILES calib/3DStackCorrection_A.png calib/3DStackCorrection_B.png DESTINATION share/odyssey3d/calib)
