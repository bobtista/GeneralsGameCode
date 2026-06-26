# FindFFMPEG.cmake
#
# TheSuperHackers @build bobtista 28/04/2026 macOS-friendly FFmpeg discovery
# for the GeneralsMD FFmpeg video and OpenAL audio backends.

find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(PC_FFMPEG QUIET libavcodec libavformat libavutil libswscale)
endif()

find_path(FFMPEG_INCLUDE_DIR
    NAMES libavcodec/avcodec.h
    HINTS
        ${PC_FFMPEG_INCLUDE_DIRS}
        ENV FFMPEG_ROOT
    PATH_SUFFIXES include
)

find_library(FFMPEG_AVCODEC_LIBRARY
    NAMES avcodec
    HINTS ${PC_FFMPEG_LIBRARY_DIRS} ENV FFMPEG_ROOT
    PATH_SUFFIXES lib
)
find_library(FFMPEG_AVFORMAT_LIBRARY
    NAMES avformat
    HINTS ${PC_FFMPEG_LIBRARY_DIRS} ENV FFMPEG_ROOT
    PATH_SUFFIXES lib
)
find_library(FFMPEG_AVUTIL_LIBRARY
    NAMES avutil
    HINTS ${PC_FFMPEG_LIBRARY_DIRS} ENV FFMPEG_ROOT
    PATH_SUFFIXES lib
)
find_library(FFMPEG_SWSCALE_LIBRARY
    NAMES swscale
    HINTS ${PC_FFMPEG_LIBRARY_DIRS} ENV FFMPEG_ROOT
    PATH_SUFFIXES lib
)

set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR})
set(FFMPEG_LIBRARIES
    ${FFMPEG_AVCODEC_LIBRARY}
    ${FFMPEG_AVFORMAT_LIBRARY}
    ${FFMPEG_AVUTIL_LIBRARY}
    ${FFMPEG_SWSCALE_LIBRARY}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFMPEG
    REQUIRED_VARS
        FFMPEG_INCLUDE_DIR
        FFMPEG_AVCODEC_LIBRARY
        FFMPEG_AVFORMAT_LIBRARY
        FFMPEG_AVUTIL_LIBRARY
        FFMPEG_SWSCALE_LIBRARY
)

if(FFMPEG_FOUND)
    if(PC_FFMPEG_LIBRARY_DIRS)
        set(FFMPEG_LIBRARY_DIRS ${PC_FFMPEG_LIBRARY_DIRS})
    endif()
    if(NOT TARGET FFMPEG::FFMPEG)
        add_library(FFMPEG::FFMPEG INTERFACE IMPORTED)
        set_target_properties(FFMPEG::FFMPEG PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${FFMPEG_LIBRARIES}"
        )
        if(FFMPEG_LIBRARY_DIRS)
            set_target_properties(FFMPEG::FFMPEG PROPERTIES
                INTERFACE_LINK_DIRECTORIES "${FFMPEG_LIBRARY_DIRS}"
            )
        endif()
    endif()
endif()

mark_as_advanced(
    FFMPEG_INCLUDE_DIR
    FFMPEG_AVCODEC_LIBRARY
    FFMPEG_AVFORMAT_LIBRARY
    FFMPEG_AVUTIL_LIBRARY
    FFMPEG_SWSCALE_LIBRARY
)
