# TheSuperHackers @build bobtista 29/04/2026 WWDownload uses winsock + Win
# registry APIs heavily. On non-Windows, compile only the platform-neutral
# urlBuilder TU + a stub that provides Cftp/CDownload skeletons for headers
# that show up in cross-platform engine code.
if(WIN32)
    set(WWDOWNLOAD_SRC
        Download.cpp
        Download.h
        DownloadDebug.h
        downloaddefs.h
        FTP.cpp
        ftp.h
        ftpdefs.h
        registry.cpp
        Registry.h
        urlBuilder.cpp
        urlBuilder.h
    )
else()
    set(WWDOWNLOAD_SRC
        Download.h
        DownloadDebug.h
        downloaddefs.h
        ftp.h
        ftpdefs.h
        Registry.h
        urlBuilder.cpp
        urlBuilder.h
        WWDownloadStub.cpp
    )
endif()

add_library(corei_wwdownload INTERFACE)

target_sources(corei_wwdownload INTERFACE ${WWDOWNLOAD_SRC})

target_link_libraries(corei_wwdownload INTERFACE
    core_wwcommon
)
