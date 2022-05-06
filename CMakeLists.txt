cmake_minimum_required(VERSION 3.19...3.29)

project(asciicker LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

# --- External libraries

find_package(OpenGL REQUIRED)

find_library(SDL_LIBRARY
NAMES SDL2 SDL
HINTS ${SDL_ROOT} ENV SDL_ROOT
PATH_SUFFIXES lib/x64
REQUIRED
)
find_path(SDL_INCLUDE_DIR
NAMES SDL.h
HINTS ${SDL_ROOT} ENV SDL_ROOT
PATH_SUFFIXES include SDL2
REQUIRED
)

if(APPLE)
  find_library(V8_LIBRARY
  NAMES v8
  REQUIRED
  )
  find_library(V8_BASE_LIBRARY
  NAMES v8_libbase
  REQUIRED
  )
  find_library(V8_PLATFORM_LIBRARY
  NAMES v8_libplatform
  REQUIRED
  )
  set(V8_LIBRARIES ${V8_LIBRARY} ${V8_BASE_LIBRARY} ${V8_PLATFORM_LIBRARY})
else()
  find_library(V8_LIBRARIES
  NAMES v8_monolith
  REQUIRED)
endif()
find_path(V8_INCLUDE_DIR
NAMES libplatform/libplatform.h
REQUIRED
)
message(STATUS "V8_INCLUDE_DIR: ${V8_INCLUDE_DIR}
V8_LIBRARIES: ${V8_LIBRARIES}")

find_library(GPM_LIBRARY NAMES gpm)
find_path(GPM_INCLUDE_DIR	NAMES gpm.h)
if(GPM_LIBRARY AND GPM_INCLUDE_DIR)
  set(GPM_FOUND true)
else()
  message(STATUS "GPM not found")
endif()


if(WIN32)
  set(WINSOCK_LIBRARIES ws2_32)
elseif(UNIX)
  find_package(X11 REQUIRED)
  find_package(Threads REQUIRED)
  find_library(UTIL_LIBRARY NAMES util REQUIRED)
endif()

if(APPLE)
  find_path(UTIL_INCLUDE_DIR NAMES util.h REQUIRED)
elseif(UNIx)
  find_path(UTIL_INCLUDE_DIR NAMES pty.h REQUIRED)
endif()

# --- executables

add_executable(asciiid asciiid.cpp
audio.cpp
stb_vorbis.cpp
term.cpp
sdl.cpp
urdo.cpp
font1.cpp
gamepad.cpp
game.cpp
enemygen.cpp
render.cpp
terrain.cpp
world.cpp
inventory.cpp
physics.cpp
sprite.cpp
texheap.cpp
rgba8.cpp
upng.c
tinfl.c
gl.c
gl45_emu.cpp
imgui_impl_opengl3.cpp
imgui/imgui.cpp
imgui/imgui_demo.cpp
imgui/imgui_draw.cpp
imgui/imgui_widgets.cpp
$<$<BOOL:${UNIX}>:x11.cpp>
$<$<BOOL:${WIN32}>:mswin.cpp>
)
target_include_directories(asciiid PRIVATE
$<$<BOOL:${UNIX}>:${UTIL_INCLUDE_DIR}>
${SDL_INCLUDE_DIR}
)
target_link_libraries(asciiid PRIVATE OpenGL::GL
"$<$<BOOL:${UNIX}>:X11::Xinerama;X11::X11;${UTIL_LIBRARY}>"
${SDL_LIBRARY}
${CMAKE_DL_LIBS}
)
target_compile_definitions(asciiid PRIVATE EDITOR UNICODE
USE_SDL
$<$<BOOL:${MSVC}>:_CRT_SECURE_NO_WARNINGS>
)
if(APPLE)
  target_link_libraries(asciiid PRIVATE "-framework AudioToolbox" "-framework Foundation")
endif()

add_executable(game
gl.c
gl45_emu.cpp
sdl.cpp
term.cpp
font1.cpp
game.cpp
gamepad.cpp
mainmenu.cpp
game_api.cpp
enemygen.cpp
game_app.cpp
network.cpp
render.cpp
terrain.cpp
world.cpp
inventory.cpp
physics.cpp
sprite.cpp
upng.c
tinfl.c
rgba8.cpp
audio.cpp
stb_vorbis.cpp
$<$<BOOL:${UNIX}>:x11.cpp>
$<$<BOOL:${WIN32}>:mswin.cpp>
)
target_compile_definitions(game PRIVATE GAME UNICODE USE_SDL
$<$<BOOL:${MSVC}>:_CRT_SECURE_NO_WARNINGS>
V8_ENABLE_SANDBOX
)
target_include_directories(game PRIVATE
${SDL_INCLUDE_DIR}
${V8_INCLUDE_DIR}
)
target_link_libraries(game PRIVATE
${SDL_LIBRARY}
${V8_LIBRARIES}
$<$<BOOL:${UNIX}>:X11::X11>
OpenGL::GL
$<$<BOOL:${UNIX}>:Threads::Threads> ${WINSOCK_LIBRARIES}
${CMAKE_DL_LIBS}
)
if(APPLE)
  target_link_libraries(game PRIVATE "-framework AudioToolbox" "-framework Foundation")
endif()


add_executable(game_term
game.cpp
mainmenu.cpp
game_api.cpp
gamepad.cpp
font1.cpp
enemygen.cpp
game_app.cpp
network.cpp
render.cpp
terrain.cpp
world.cpp
inventory.cpp
physics.cpp
sprite.cpp
upng.c
tinfl.c
rgba8.cpp
audio.cpp
stb_vorbis.cpp
$<$<BOOL:${WIN32}>:mswin.cpp>
)
target_compile_definitions(game_term PRIVATE GAME PURE_TERM UNICODE
$<$<BOOL:${GPM_FOUND}>:USE_GPM>
V8_ENABLE_SANDBOX
)
if(MSVC)
  target_compile_definitions(game_term PRIVATE _CRT_SECURE_NO_WARNINGS
    STDIN_FILENO=0 STDOUT_FILENO=1
  )
endif()
target_include_directories(game_term PRIVATE
$<$<BOOL:${UNIX}>:${UTIL_INCLUDE_DIR}>
${V8_INCLUDE_DIR}
)
target_link_libraries(game_term PRIVATE
${V8_LIBRARIES}
OpenGL::GL
"$<$<BOOL:${UNIX}>:${UTIL_LIBRARY};Threads::Threads>"
$<$<BOOL:${WIN32}>:${WINSOCK_LIBRARIES}>
${CMAKE_DL_LIBS}
)
if(APPLE)
  target_link_libraries(game_term PRIVATE "-framework AudioToolbox" "-framework Foundation")
endif()


add_executable(game_server game_svr.cpp
audio.cpp
stb_vorbis.cpp
network.cpp
sha1.c
font1.cpp
gamepad.cpp
game.cpp
enemygen.cpp
render.cpp
terrain.cpp
world.cpp
inventory.cpp
physics.cpp
sprite.cpp
tinfl.c
)
target_compile_definitions(game_server PRIVATE SERVER)
target_link_libraries(game_server PRIVATE
${WINSOCK_LIBRARIES}
${CMAKE_DL_LIBS}
)
if(APPLE)
  target_link_libraries(game_server PRIVATE "-framework AudioToolbox" "-framework Foundation")
endif()
