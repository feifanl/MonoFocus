cmake_minimum_required(VERSION 3.21)
project(monofocus CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT (/MT, /MTd for Debug) so the exe is copy-anywhere with no runtime deps.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# One WIN32 executable built from every src/*.cpp plus the resource (manifest).
file(GLOB SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(monofocus WIN32 ${SOURCES} res/app.rc)

target_compile_definitions(monofocus PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

target_link_libraries(monofocus PRIVATE
    magnification user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32)

# We embed our own manifest via res/app.rc; stop the linker from generating a
# second one (which would collide as duplicate MANIFEST resource id 1).
target_link_options(monofocus PRIVATE /MANIFEST:NO)
