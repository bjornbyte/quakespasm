# CMakeLists.txt for QuakeSpasm
# A modern, cross-platform Quake engine source port
#
# Usage:
#   Linux:
#     mkdir build && cd build
#     cmake .. -DCMAKE_BUILD_TYPE=Release
#     make -j$(nproc)
#
#   Windows (MSVC):
#     mkdir build && cd build
#     cmake .. -G "Visual Studio 17 2022" -A x64
#     cmake --build . --config Release
#
#   Windows (MinGW):
#     mkdir build && cd build
#     cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
#     mingw32-make

cmake_minimum_required(VERSION 3.16)
project(QuakeSpasm VERSION 0.97.0 LANGUAGES C CXX)

# ------------------------------------------------------------------------------
# Build Options
# ------------------------------------------------------------------------------

option(USE_SDL2 "Use SDL2 instead of SDL1.2" ON)
option(DO_USERDIRS "Enable user directories support" OFF)

# Audio codec options - platform-specific defaults
if(WIN32)
    option(USE_CODEC_WAVE "Enable WAV audio codec" OFF)
    option(USE_CODEC_FLAC "Enable FLAC audio codec" OFF)
    option(USE_CODEC_MP3 "Enable MP3 audio codec" OFF)
    option(USE_CODEC_VORBIS "Enable Vorbis audio codec" ON)
    option(USE_CODEC_OPUS "Enable Opus audio codec" OFF)
    option(USE_CODEC_MIKMOD "Enable MikMod audio codec" OFF)
    option(USE_CODEC_XMP "Enable XMP audio codec" OFF)
    option(USE_CODEC_MODPLUG "Enable ModPlug audio codec" OFF)
    option(USE_CODEC_UMX "Enable UMX audio codec" OFF)
else()
    option(USE_CODEC_WAVE "Enable WAV audio codec" OFF)
    option(USE_CODEC_FLAC "Enable FLAC audio codec" OFF)
    option(USE_CODEC_MP3 "Enable MP3 audio codec" OFF)
    option(USE_CODEC_VORBIS "Enable Vorbis audio codec" ON)
    option(USE_CODEC_OPUS "Enable Opus audio codec" OFF)
    option(USE_CODEC_MIKMOD "Enable MikMod audio codec" OFF)
    option(USE_CODEC_XMP "Enable XMP audio codec" OFF)
    option(USE_CODEC_MODPLUG "Enable ModPlug audio codec" OFF)
    option(USE_CODEC_UMX "Enable UMX audio codec" OFF)
endif()

# AccelByte GameSDK
option(USE_ACCELBYTE_GAMESDK "Enable AccelByte GameSDK integration" ON)

# Library selection for MP3 and Vorbis
set(MP3LIB "mad" CACHE STRING "MP3 decoder library (mad or mpg123)")
set_property(CACHE MP3LIB PROPERTY STRINGS "mad" "mpg123")

set(VORBISLIB "vorbis" CACHE STRING "Vorbis decoder library (vorbis or tremor)")
set_property(CACHE VORBISLIB PROPERTY STRINGS "vorbis" "tremor")

# Validate library choices
if(NOT MP3LIB STREQUAL "mad" AND NOT MP3LIB STREQUAL "mpg123")
    message(FATAL_ERROR "Invalid MP3LIB setting: ${MP3LIB}. Must be 'mad' or 'mpg123'")
endif()

if(NOT VORBISLIB STREQUAL "vorbis" AND NOT VORBISLIB STREQUAL "tremor")
    message(FATAL_ERROR "Invalid VORBISLIB setting: ${VORBISLIB}. Must be 'vorbis' or 'tremor'")
endif()

# ------------------------------------------------------------------------------
# Source Files
# ------------------------------------------------------------------------------

set(QUAKE_DIR ${CMAKE_SOURCE_DIR}/Quake)

# Common source files (shared across all platforms)
set(COMMON_SOURCES
    # Core engine
    ${QUAKE_DIR}/bgmusic.c
    ${QUAKE_DIR}/cfgfile.c
    ${QUAKE_DIR}/chase.c
    ${QUAKE_DIR}/cmd.c
    ${QUAKE_DIR}/common.c
    ${QUAKE_DIR}/crc.c
    ${QUAKE_DIR}/cvar.c
    ${QUAKE_DIR}/host.c
    ${QUAKE_DIR}/host_cmd.c
    ${QUAKE_DIR}/mathlib.c
    ${QUAKE_DIR}/wad.c
    ${QUAKE_DIR}/world.c
    ${QUAKE_DIR}/zone.c

    # Client
    ${QUAKE_DIR}/cl_demo.c
    ${QUAKE_DIR}/cl_input.c
    ${QUAKE_DIR}/cl_main.c
    ${QUAKE_DIR}/cl_parse.c
    ${QUAKE_DIR}/cl_tent.c
    ${QUAKE_DIR}/console.c
    ${QUAKE_DIR}/keys.c
    ${QUAKE_DIR}/menu.c
    ${QUAKE_DIR}/sbar.c
    ${QUAKE_DIR}/view.c

    # Server
    ${QUAKE_DIR}/pr_cmds.c
    ${QUAKE_DIR}/pr_edict.c
    ${QUAKE_DIR}/pr_exec.c
    ${QUAKE_DIR}/sv_main.c
    ${QUAKE_DIR}/sv_move.c
    ${QUAKE_DIR}/sv_phys.c
    ${QUAKE_DIR}/sv_user.c

    # Rendering (OpenGL)
    ${QUAKE_DIR}/gl_draw.c
    ${QUAKE_DIR}/gl_fog.c
    ${QUAKE_DIR}/gl_mesh.c
    ${QUAKE_DIR}/gl_model.c
    ${QUAKE_DIR}/gl_refrag.c
    ${QUAKE_DIR}/gl_rlight.c
    ${QUAKE_DIR}/gl_rmain.c
    ${QUAKE_DIR}/gl_rmisc.c
    ${QUAKE_DIR}/gl_screen.c
    ${QUAKE_DIR}/gl_sky.c
    ${QUAKE_DIR}/gl_texmgr.c
    ${QUAKE_DIR}/gl_vidsdl.c
    ${QUAKE_DIR}/gl_warp.c
    ${QUAKE_DIR}/image.c
    ${QUAKE_DIR}/r_alias.c
    ${QUAKE_DIR}/r_brush.c
    ${QUAKE_DIR}/r_part.c
    ${QUAKE_DIR}/r_sprite.c
    ${QUAKE_DIR}/r_world.c

    # Sound core
    ${QUAKE_DIR}/snd_dma.c
    ${QUAKE_DIR}/snd_mem.c
    ${QUAKE_DIR}/snd_mix.c
    ${QUAKE_DIR}/snd_sdl.c

    # Sound codecs
    ${QUAKE_DIR}/snd_codec.c
    ${QUAKE_DIR}/snd_flac.c
    ${QUAKE_DIR}/snd_mikmod.c
    ${QUAKE_DIR}/snd_modplug.c
    ${QUAKE_DIR}/snd_mp3tag.c
    ${QUAKE_DIR}/snd_opus.c
    ${QUAKE_DIR}/snd_umx.c
    ${QUAKE_DIR}/snd_vorbis.c
    ${QUAKE_DIR}/snd_wave.c
    ${QUAKE_DIR}/snd_xmp.c

    # Input/CD
    ${QUAKE_DIR}/cd_sdl.c
    ${QUAKE_DIR}/in_sdl.c

    # Networking (common)
    ${QUAKE_DIR}/net_dgrm.c
    ${QUAKE_DIR}/net_loop.c
    ${QUAKE_DIR}/net_main.c

    # Main entry point
    ${QUAKE_DIR}/main_sdl.c

    # Utilities
    ${QUAKE_DIR}/strlcat.c
    ${QUAKE_DIR}/strlcpy.c
    ${QUAKE_DIR}/miniz.c
)

# MP3 decoder source selection
if(MP3LIB STREQUAL "mad")
    list(APPEND COMMON_SOURCES ${QUAKE_DIR}/snd_mp3.c)
else()
    list(APPEND COMMON_SOURCES ${QUAKE_DIR}/snd_mpg123.c)
endif()

# Platform-specific sources
if(WIN32)
    set(PLATFORM_SOURCES
        ${QUAKE_DIR}/net_win.c
        ${QUAKE_DIR}/net_wins.c
        ${QUAKE_DIR}/net_wipx.c
        ${QUAKE_DIR}/pl_win.c
        ${QUAKE_DIR}/sys_sdl_win.c
    )
else()
    set(PLATFORM_SOURCES
        ${QUAKE_DIR}/net_bsd.c
        ${QUAKE_DIR}/net_udp.c
        ${QUAKE_DIR}/pl_linux.c
        ${QUAKE_DIR}/sys_sdl_unix.c
    )
endif()

# AccelByte Integration sources (C++)
if(USE_ACCELBYTE_GAMESDK)
    set(AB_INTEGRATION_SOURCES
        ${QUAKE_DIR}/ABIntegration/ab_integration.cpp
        ${QUAKE_DIR}/ABIntegration/ab_task_runner.cpp
    )
endif()

# ------------------------------------------------------------------------------
# Create Executable
# ------------------------------------------------------------------------------

set(ALL_SOURCES ${COMMON_SOURCES} ${PLATFORM_SOURCES})
if(USE_ACCELBYTE_GAMESDK AND AB_INTEGRATION_SOURCES)
    list(APPEND ALL_SOURCES ${AB_INTEGRATION_SOURCES})
endif()

if(WIN32)
    # Add Windows resource file for application icon
    set(RC_FILE ${CMAKE_SOURCE_DIR}/Windows/QuakeSpasm.rc)
    add_executable(quakespasm WIN32 ${ALL_SOURCES} ${RC_FILE})
else()
    add_executable(quakespasm ${ALL_SOURCES})
endif()

# ------------------------------------------------------------------------------
# Include Directories
# ------------------------------------------------------------------------------

target_include_directories(quakespasm PRIVATE ${QUAKE_DIR})

# ------------------------------------------------------------------------------
# Find Dependencies
# ------------------------------------------------------------------------------

# Use pkg-config for finding libraries on Unix-like systems
if(NOT WIN32)
    find_package(PkgConfig)
endif()

# SDL / SDL2
if(USE_SDL2)
    if(WIN32)
        # Use bundled SDL2 on Windows
        set(SDL2_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Windows/SDL2/include)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(SDL2_LIB_DIR ${CMAKE_SOURCE_DIR}/Windows/SDL2/lib64)
        else()
            set(SDL2_LIB_DIR ${CMAKE_SOURCE_DIR}/Windows/SDL2/lib)
        endif()

        target_include_directories(quakespasm PRIVATE ${SDL2_INCLUDE_DIR})

        if(MSVC)
            target_link_libraries(quakespasm PRIVATE
                ${SDL2_LIB_DIR}/SDL2.lib
                ${SDL2_LIB_DIR}/SDL2main.lib
            )
        else()
            # MinGW
            target_link_libraries(quakespasm PRIVATE
                ${SDL2_LIB_DIR}/libSDL2main.a
                ${SDL2_LIB_DIR}/libSDL2.dll.a
            )
        endif()
        target_compile_definitions(quakespasm PRIVATE USE_SDL2)
    else()
        # Unix: use pkg-config or find_package
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2 sdl2)
        endif()
        if(NOT SDL2_FOUND)
            find_package(SDL2)
            if(SDL2_FOUND)
                set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
                set(SDL2_LIBRARIES SDL2::SDL2)
                if(TARGET SDL2::SDL2main)
                    list(APPEND SDL2_LIBRARIES SDL2::SDL2main)
                endif()
            endif()
        endif()
        if(SDL2_FOUND)
            target_include_directories(quakespasm PRIVATE ${SDL2_INCLUDE_DIRS})
            target_link_libraries(quakespasm PRIVATE ${SDL2_LIBRARIES})
            target_compile_definitions(quakespasm PRIVATE USE_SDL2)
        else()
            message(FATAL_ERROR "SDL2 not found. Install SDL2 development libraries or set USE_SDL2=OFF")
        endif()
    endif()
else()
    # SDL 1.2
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL sdl)
    endif()
    if(NOT SDL_FOUND)
        find_package(SDL)
    endif()
    if(SDL_FOUND)
        target_include_directories(quakespasm PRIVATE ${SDL_INCLUDE_DIRS})
        target_link_libraries(quakespasm PRIVATE ${SDL_LIBRARIES})
    else()
        message(FATAL_ERROR "SDL not found. Install SDL development libraries or set USE_SDL2=ON")
    endif()
endif()

# OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(quakespasm PRIVATE OpenGL::GL)

# AccelByte GameSDK
if(USE_ACCELBYTE_GAMESDK)
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/thirdparties/accelbyte-gamesdk")
    find_package(accelbyte-gamesdk REQUIRED)

    # Find the cpp_web_socket library
    find_library(ACCELBYTE_CPP_WEBSOCKET_LIB
        NAMES abcnl-cpp_web_socket
        PATHS "${CMAKE_SOURCE_DIR}/thirdparties/accelbyte-gamesdk/lib"
        NO_DEFAULT_PATH
    )

    target_link_libraries(quakespasm PRIVATE
        accelbyte-gamesdk::user
        accelbyte-gamesdk::lobby
        accelbyte-gamesdk::settings
        abcnl-codegen::social
        abcnl::curl_http_executor
    )

    # Link cpp_web_socket library if found
    if(ACCELBYTE_CPP_WEBSOCKET_LIB)
        target_link_libraries(quakespasm PRIVATE ${ACCELBYTE_CPP_WEBSOCKET_LIB})
    else()
        message(WARNING "AccelByte cpp_web_socket library not found, lobby connections may fail")
    endif()

    target_compile_definitions(quakespasm PRIVATE USE_ACCELBYTE_GAMESDK ACCELBYTE_BUILD_SHARED_DEFINE)
    target_compile_features(quakespasm PRIVATE cxx_std_20)
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(quakespasm PRIVATE ws2_32 winmm)
    target_compile_definitions(quakespasm PRIVATE _USE_WINSOCK2)

    # Windows bundled codec libraries
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WINDOWS_CODEC_LIB_DIR ${CMAKE_SOURCE_DIR}/Windows/codecs/x64)
    else()
        set(WINDOWS_CODEC_LIB_DIR ${CMAKE_SOURCE_DIR}/Windows/codecs/x86)
    endif()
    set(WINDOWS_CODEC_INC_DIR ${CMAKE_SOURCE_DIR}/Windows/codecs/include)
else()
    # Unix math library
    target_link_libraries(quakespasm PRIVATE m)
endif()

# ------------------------------------------------------------------------------
# Audio Codec Dependencies
# ------------------------------------------------------------------------------

# Helper macro for finding codec libraries
macro(find_codec_library CODEC_NAME PKG_NAME LIBRARY_NAMES HEADER_PATH)
    set(${CODEC_NAME}_FOUND FALSE)

    if(WIN32)
        # Use bundled libraries on Windows
        target_include_directories(quakespasm PRIVATE ${WINDOWS_CODEC_INC_DIR})
        find_library(${CODEC_NAME}_LIBRARY
            NAMES ${LIBRARY_NAMES}
            PATHS ${WINDOWS_CODEC_LIB_DIR}
            NO_DEFAULT_PATH
        )
        if(${CODEC_NAME}_LIBRARY)
            set(${CODEC_NAME}_FOUND TRUE)
            target_link_libraries(quakespasm PRIVATE ${${CODEC_NAME}_LIBRARY})
        endif()
    else()
        # Try pkg-config first on Unix
        if(PkgConfig_FOUND AND NOT "${PKG_NAME}" STREQUAL "")
            pkg_check_modules(${CODEC_NAME} ${PKG_NAME})
        endif()

        if(${CODEC_NAME}_FOUND)
            target_include_directories(quakespasm PRIVATE ${${CODEC_NAME}_INCLUDE_DIRS})
            target_link_libraries(quakespasm PRIVATE ${${CODEC_NAME}_LIBRARIES})
        else()
            # Fall back to find_library
            find_library(${CODEC_NAME}_LIBRARY NAMES ${LIBRARY_NAMES})
            find_path(${CODEC_NAME}_INCLUDE_DIR NAMES ${HEADER_PATH})
            if(${CODEC_NAME}_LIBRARY)
                set(${CODEC_NAME}_FOUND TRUE)
                target_link_libraries(quakespasm PRIVATE ${${CODEC_NAME}_LIBRARY})
                if(${CODEC_NAME}_INCLUDE_DIR)
                    target_include_directories(quakespasm PRIVATE ${${CODEC_NAME}_INCLUDE_DIR})
                endif()
            endif()
        endif()
    endif()
endmacro()

# WAVE codec (built-in, no external dependencies)
if(USE_CODEC_WAVE)
    target_compile_definitions(quakespasm PRIVATE USE_CODEC_WAVE)
endif()

# FLAC codec
if(USE_CODEC_FLAC)
    find_codec_library(FLAC "flac" "FLAC;libFLAC" "FLAC/stream_decoder.h")
    if(FLAC_FOUND)
        target_compile_definitions(quakespasm PRIVATE USE_CODEC_FLAC)
    else()
        message(WARNING "FLAC library not found, disabling FLAC support")
        set(USE_CODEC_FLAC OFF)
    endif()
endif()

# MP3 codec
if(USE_CODEC_MP3)
    if(MP3LIB STREQUAL "mad")
        find_codec_library(MAD "mad" "mad;libmad" "mad.h")
        if(MAD_FOUND)
            target_compile_definitions(quakespasm PRIVATE USE_CODEC_MP3)
        else()
            message(WARNING "libmad not found, disabling MP3 support")
            set(USE_CODEC_MP3 OFF)
        endif()
    else()
        find_codec_library(MPG123 "libmpg123" "mpg123;libmpg123" "mpg123.h")
        if(MPG123_FOUND)
            target_compile_definitions(quakespasm PRIVATE USE_CODEC_MP3)
        else()
            message(WARNING "libmpg123 not found, disabling MP3 support")
            set(USE_CODEC_MP3 OFF)
        endif()
    endif()
endif()

# Vorbis codec
if(USE_CODEC_VORBIS)
    if(VORBISLIB STREQUAL "vorbis")
        if(WIN32)
            # Windows: find individual libraries
            find_library(VORBISFILE_LIBRARY NAMES vorbisfile libvorbisfile PATHS ${WINDOWS_CODEC_LIB_DIR} NO_DEFAULT_PATH)
            find_library(VORBIS_LIBRARY NAMES vorbis libvorbis PATHS ${WINDOWS_CODEC_LIB_DIR} NO_DEFAULT_PATH)
            find_library(OGG_LIBRARY NAMES ogg libogg PATHS ${WINDOWS_CODEC_LIB_DIR} NO_DEFAULT_PATH)
            if(VORBISFILE_LIBRARY AND VORBIS_LIBRARY AND OGG_LIBRARY)
                target_include_directories(quakespasm PRIVATE ${WINDOWS_CODEC_INC_DIR})
                target_link_libraries(quakespasm PRIVATE ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY} ${OGG_LIBRARY})
                target_compile_definitions(quakespasm PRIVATE USE_CODEC_VORBIS)
            else()
                message(WARNING "Vorbis libraries not found, disabling Vorbis support")
                set(USE_CODEC_VORBIS OFF)
            endif()
        else()
            # Unix: use pkg-config or find_library
            if(PkgConfig_FOUND)
                pkg_check_modules(VORBISFILE vorbisfile)
            endif()
            if(VORBISFILE_FOUND)
                target_include_directories(quakespasm PRIVATE ${VORBISFILE_INCLUDE_DIRS})
                target_link_libraries(quakespasm PRIVATE ${VORBISFILE_LIBRARIES})
                target_compile_definitions(quakespasm PRIVATE USE_CODEC_VORBIS)
            else()
                find_library(VORBISFILE_LIBRARY NAMES vorbisfile)
                find_library(VORBIS_LIBRARY NAMES vorbis)
                find_library(OGG_LIBRARY NAMES ogg)
                if(VORBISFILE_LIBRARY AND VORBIS_LIBRARY AND OGG_LIBRARY)
                    target_link_libraries(quakespasm PRIVATE ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY} ${OGG_LIBRARY})
                    target_compile_definitions(quakespasm PRIVATE USE_CODEC_VORBIS)
                else()
                    message(WARNING "Vorbis libraries not found, disabling Vorbis support")
                    set(USE_CODEC_VORBIS OFF)
                endif()
            endif()
        endif()
    else()
        # Tremor (integer-only Vorbis decoder)
        find_codec_library(TREMOR "" "vorbisidec;tremor" "tremor/ivorbisfile.h")
        if(TREMOR_FOUND)
            find_library(OGG_LIBRARY NAMES ogg libogg)
            if(OGG_LIBRARY)
                target_link_libraries(quakespasm PRIVATE ${OGG_LIBRARY})
            endif()
            target_compile_definitions(quakespasm PRIVATE USE_CODEC_VORBIS VORBIS_USE_TREMOR)
        else()
            message(WARNING "Tremor library not found, disabling Vorbis support")
            set(USE_CODEC_VORBIS OFF)
        endif()
    endif()
endif()

# Opus codec
if(USE_CODEC_OPUS)
    if(WIN32)
        find_library(OPUSFILE_LIBRARY NAMES opusfile libopusfile PATHS ${WINDOWS_CODEC_LIB_DIR} NO_DEFAULT_PATH)
        find_library(OPUS_LIBRARY NAMES opus libopus PATHS ${WINDOWS_CODEC_LIB_DIR} NO_DEFAULT_PATH)
        find_library(OGG_LIBRARY_OPUS NAMES ogg libogg PATHS ${WINDOWS_CODEC_LIB_DIR} NO_DEFAULT_PATH)
        if(OPUSFILE_LIBRARY AND OPUS_LIBRARY AND OGG_LIBRARY_OPUS)
            target_include_directories(quakespasm PRIVATE ${WINDOWS_CODEC_INC_DIR})
            target_link_libraries(quakespasm PRIVATE ${OPUSFILE_LIBRARY} ${OPUS_LIBRARY} ${OGG_LIBRARY_OPUS})
            target_compile_definitions(quakespasm PRIVATE USE_CODEC_OPUS)
        else()
            message(WARNING "Opus libraries not found, disabling Opus support")
            set(USE_CODEC_OPUS OFF)
        endif()
    else()
        if(PkgConfig_FOUND)
            pkg_check_modules(OPUSFILE opusfile)
        endif()
        if(OPUSFILE_FOUND)
            target_include_directories(quakespasm PRIVATE ${OPUSFILE_INCLUDE_DIRS})
            target_link_libraries(quakespasm PRIVATE ${OPUSFILE_LIBRARIES})
            target_compile_definitions(quakespasm PRIVATE USE_CODEC_OPUS)
        else()
            message(WARNING "opusfile not found, disabling Opus support")
            set(USE_CODEC_OPUS OFF)
        endif()
    endif()
endif()

# MikMod codec
if(USE_CODEC_MIKMOD)
    find_codec_library(MIKMOD "libmikmod" "mikmod;libmikmod" "mikmod.h")
    if(MIKMOD_FOUND)
        target_compile_definitions(quakespasm PRIVATE USE_CODEC_MIKMOD)
    else()
        message(WARNING "MikMod library not found, disabling MikMod support")
        set(USE_CODEC_MIKMOD OFF)
    endif()
endif()

# XMP codec
if(USE_CODEC_XMP)
    find_codec_library(XMP "libxmp" "xmp;libxmp" "xmp.h")
    if(XMP_FOUND)
        target_compile_definitions(quakespasm PRIVATE USE_CODEC_XMP)
    else()
        message(WARNING "XMP library not found, disabling XMP support")
        set(USE_CODEC_XMP OFF)
    endif()
endif()

# ModPlug codec
if(USE_CODEC_MODPLUG)
    find_codec_library(MODPLUG "libmodplug" "modplug;libmodplug" "libmodplug/modplug.h")
    if(MODPLUG_FOUND)
        target_compile_definitions(quakespasm PRIVATE USE_CODEC_MODPLUG)
    else()
        message(WARNING "ModPlug library not found, disabling ModPlug support")
        set(USE_CODEC_MODPLUG OFF)
    endif()
endif()

# UMX codec (built-in, no external dependencies)
if(USE_CODEC_UMX)
    target_compile_definitions(quakespasm PRIVATE USE_CODEC_UMX)
endif()

# ------------------------------------------------------------------------------
# Compiler Flags
# ------------------------------------------------------------------------------

# User directories support
if(DO_USERDIRS)
    target_compile_definitions(quakespasm PRIVATE DO_USERDIRS=1)
endif()

# Common compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(quakespasm PRIVATE
        -Wall
        -Wno-trigraphs
    )

    # Check for C11 support
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-std=gnu11" COMPILER_SUPPORTS_GNU11)
    if(COMPILER_SUPPORTS_GNU11)
        target_compile_options(quakespasm PRIVATE -std=gnu11)
    endif()

    # Release optimizations
    check_c_compiler_flag("-fweb" COMPILER_SUPPORTS_FWEB)
    check_c_compiler_flag("-frename-registers" COMPILER_SUPPORTS_FRENAME_REGISTERS)

    if(COMPILER_SUPPORTS_FWEB)
        target_compile_options(quakespasm PRIVATE $<$<CONFIG:Release>:-fweb>)
        target_compile_options(quakespasm PRIVATE $<$<CONFIG:RelWithDebInfo>:-fweb>)
    endif()
    if(COMPILER_SUPPORTS_FRENAME_REGISTERS)
        target_compile_options(quakespasm PRIVATE $<$<CONFIG:Release>:-frename-registers>)
        target_compile_options(quakespasm PRIVATE $<$<CONFIG:RelWithDebInfo>:-frename-registers>)
    endif()

    # MinGW-specific flags
    if(MINGW)
        target_link_options(quakespasm PRIVATE -static-libgcc)
    endif()
elseif(MSVC)
    target_compile_options(quakespasm PRIVATE
        /W3
        /wd4244  # conversion from 'type1' to 'type2', possible loss of data
        /wd4305  # truncation from 'type1' to 'type2'
    )
endif()

# Build type definitions
target_compile_definitions(quakespasm PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:RelWithDebInfo>:NDEBUG>
    $<$<CONFIG:MinSizeRel>:NDEBUG>
)

# ------------------------------------------------------------------------------
# Installation
# ------------------------------------------------------------------------------

install(TARGETS quakespasm
    RUNTIME DESTINATION bin
)

# ------------------------------------------------------------------------------
# Copy DLLs to build directory (Windows)
# ------------------------------------------------------------------------------

if(WIN32)
    # Determine source directories based on architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CODEC_DLL_DIR ${CMAKE_SOURCE_DIR}/Windows/codecs/x64)
        set(SDL2_DLL_DIR ${CMAKE_SOURCE_DIR}/Windows/SDL2/lib64)
    else()
        set(CODEC_DLL_DIR ${CMAKE_SOURCE_DIR}/Windows/codecs/x86)
        set(SDL2_DLL_DIR ${CMAKE_SOURCE_DIR}/Windows/SDL2/lib)
    endif()

    # Copy SDL2.dll
    if(USE_SDL2)
        add_custom_command(TARGET quakespasm POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL2_DLL_DIR}/SDL2.dll"
                $<TARGET_FILE_DIR:quakespasm>
            COMMENT "Copying SDL2.dll to output directory"
        )
    endif()

    # Copy codec DLLs based on enabled codecs
    set(CODEC_DLLS "")
    if(USE_CODEC_FLAC)
        list(APPEND CODEC_DLLS "libFLAC-8.dll")
    endif()
    if(USE_CODEC_MP3)
        if(MP3LIB STREQUAL "mad")
            list(APPEND CODEC_DLLS "libmad-0.dll")
        else()
            list(APPEND CODEC_DLLS "libmpg123-0.dll")
        endif()
    endif()
    if(USE_CODEC_VORBIS)
        list(APPEND CODEC_DLLS "libogg-0.dll" "libvorbis-0.dll" "libvorbisfile-3.dll")
    endif()
    if(USE_CODEC_OPUS)
        list(APPEND CODEC_DLLS "libogg-0.dll" "libopus-0.dll" "libopusfile-0.dll")
    endif()
    if(USE_CODEC_MIKMOD)
        list(APPEND CODEC_DLLS "libmikmod-3.dll")
    endif()
    if(USE_CODEC_XMP)
        list(APPEND CODEC_DLLS "libxmp.dll")
    endif()

    # Remove duplicates (e.g., libogg-0.dll used by both vorbis and opus)
    list(REMOVE_DUPLICATES CODEC_DLLS)

    foreach(DLL ${CODEC_DLLS})
        add_custom_command(TARGET quakespasm POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CODEC_DLL_DIR}/${DLL}"
                $<TARGET_FILE_DIR:quakespasm>
            COMMENT "Copying ${DLL} to output directory"
        )
    endforeach()

    # Copy AccelByte GameSDK DLLs
    if(USE_ACCELBYTE_GAMESDK)
        set(ACCELBYTE_DLL_DIR ${CMAKE_SOURCE_DIR}/thirdparties/accelbyte-gamesdk/bin)

        # Core SDK DLLs (user, lobby, settings and their dependencies)
        set(ACCELBYTE_DLLS
            # Main SDK components
            accelbyte-gamesdk-user.dll
            accelbyte-gamesdk-lobby.dll
            accelbyte-gamesdk-settings.dll
            accelbyte-gamesdk-crypto.dll
            accelbyte-gamesdk-common.dll
            accelbyte-gamesdk-chat.dll
            accelbyte-gamesdk-p2p_connection.dll
            # Required abcnl dependencies
            abcnl-common.dll
            abcnl-memory.dll
            abcnl-http.dll
            abcnl-utils.dll
            abcnl-tls.dll
            abcnl-platform_connection.dll
            abcnl-web_socket.dll
            abcnl-cpp_web_socket.dll
            abcnl-curl_http_executor.dll
            abcnl-iam.dll
            abcnl-lobby.dll
            abcnl-codegen_common.dll
            abcnl-codegen_test_tools.dll
            abcnl-debug_handler.dll
            abcnl-debug_http_executor.dll
            abcnl-achievement.dll
            abcnl-cloudsave.dll
            abcnl-audit.dll
            abcnl-session.dll
            abcnl-match2.dll
            abcnl-basic.dll
            abcnl-challenge.dll
            abcnl-group.dll
            abcnl-chat.dll
            abcnl-turn_manager.dll
            abcnl-social.dll
            abcnl-ugc.dll
            abcnl-leaderboard.dll
            abcnl-platform.dll
            abcnl-logger.dll
            abcnl-network_utility.dll
            abcnl-network_debug_output.dll
            abcnl-p2p_connection.dll
            abcnl-libjuice_p2p_connection.dll
            abcnl-windows_network_utility.dll
            abcnl-xbox_connection.dll
            abcnl-inventory.dll
            abcnl-legal.dll
            abcnl-reporting.dll
            abcnl-seasonpass.dll
            abcnl-test_utils.dll
            abcnl-ehs.dll
            abcnl-file_debug_output.dll
            abcnl-gametelemetry.dll
            abcnl-gdpr.dll
            abcnl-login_queue.dll
        )

        foreach(DLL ${ACCELBYTE_DLLS})
            if(EXISTS "${ACCELBYTE_DLL_DIR}/${DLL}")
                add_custom_command(TARGET quakespasm POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${ACCELBYTE_DLL_DIR}/${DLL}"
                        $<TARGET_FILE_DIR:quakespasm>
                    COMMENT "Copying ${DLL} to output directory"
                )
            endif()
        endforeach()
    endif()
endif()

# ------------------------------------------------------------------------------
# Configuration Summary
# ------------------------------------------------------------------------------

message(STATUS "")
message(STATUS "QuakeSpasm ${PROJECT_VERSION} Configuration Summary")
message(STATUS "=========================================")
message(STATUS "")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler:      ${CMAKE_C_COMPILER}")
message(STATUS "Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  USE_SDL2:            ${USE_SDL2}")
message(STATUS "  DO_USERDIRS:         ${DO_USERDIRS}")
message(STATUS "  USE_ACCELBYTE_GAMESDK: ${USE_ACCELBYTE_GAMESDK}")
message(STATUS "")
message(STATUS "Audio Codecs:")
message(STATUS "  WAVE:          ${USE_CODEC_WAVE}")
message(STATUS "  FLAC:          ${USE_CODEC_FLAC}")
message(STATUS "  MP3:           ${USE_CODEC_MP3} (${MP3LIB})")
message(STATUS "  Vorbis:        ${USE_CODEC_VORBIS} (${VORBISLIB})")
message(STATUS "  Opus:          ${USE_CODEC_OPUS}")
message(STATUS "  MikMod:        ${USE_CODEC_MIKMOD}")
message(STATUS "  XMP:           ${USE_CODEC_XMP}")
message(STATUS "  ModPlug:       ${USE_CODEC_MODPLUG}")
message(STATUS "  UMX:           ${USE_CODEC_UMX}")
message(STATUS "")
