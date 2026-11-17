cmake_minimum_required(VERSION 3.20)
project(dragoon_ps5 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PSX_REWIND OFF CACHE BOOL "Enable rewind")
set(PSX_SDL3_FETCH OFF CACHE BOOL "Fetch a desktop SDL3 build")
set(PSX_RECOMP_UI OFF CACHE BOOL "Enable the desktop launcher")
set(PSXRECOMP_BIOS_STEMS OpenBIOS CACHE STRING "BIOS backends for the baseline")
set(generated "${CMAKE_CURRENT_SOURCE_DIR}/local-codegen/generated-reachable")
set(dispatch "${generated}/SCUS_944.91_dispatch.c")
if(NOT EXISTS "${dispatch}")
    message(FATAL_ERROR "Generate the US Disc 1 reachable code before configuring Dragoon.")
endif()
file(GLOB shards CONFIGURE_DEPENDS "${generated}/SCUS_944.91_full_*.c")

include(framework/runtime/runtime.cmake)
include(usb_entry.cmake)
psxrecomp_add_runtime_target(dragoon
    WINDOW_TITLE "The Legend of Dragoon"
    EXE_NAME "dragoon"
    MAX_PLAYERS 1
    DEFAULT_GAME_CONFIG_PATH "game.toml"
    DEFAULT_BIOS_PATH "bios/openbios.bin"
    PRELOADED_MODS_DIR NONE
    GAME_GENERATED_FULL_C ${shards}
    GAME_GENERATED_DISPATCH_C "${dispatch}")
dragoon_bind_usb_entry(dragoon)
target_sources(dragoon PRIVATE
    framework/runtime/src/dragoon_menu.c
    submap_key.c
    dragoon_field_disc.c
    dragoon_field_runtime.c
    dragoon_field_trace.cpp
    dragoon_field_art.cpp)
set_source_files_properties(
    framework/recompiler/src/config_loader.cpp
    framework/runtime/src/autocompile.c
    framework/runtime/src/boot_state.c
    framework/runtime/src/boot_state_capture_jobs.c
    framework/runtime/src/cdrom.c
    framework/runtime/src/crash_trace.c
    framework/runtime/src/dirty_ram_interp.c
    framework/runtime/src/main.cpp
    framework/runtime/src/gpu_vk_renderer.c
    framework/runtime/src/gpu.c
    framework/runtime/src/memory.c
    framework/runtime/src/mod_runtime.cpp
    framework/runtime/src/psx_runtime_menu.c
    framework/runtime/src/traps.c
    PROPERTIES
        COMPILE_DEFINITIONS DRAGOON_PS5=1
        INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")
