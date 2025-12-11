# TheSuperHackers @refactor bobtista 01/01/2025 Cross-platform source files
# Check if this is a VC6 build (VC6 doesn't support Qt, needs MFC)
if(DEFINED MSVC_VERSION AND MSVC_VERSION LESS 1300)
    set(IS_VS6_BUILD TRUE)
else()
    set(IS_VS6_BUILD FALSE)
endif()

set(BABYLON_SRC
    # Core data structures (now cross-platform)
    "PlatformTypes.h"
    "bin.cpp"
    "bin.h"
    "olestring.cpp"
    "olestring.h"
    "TransDB.cpp"
    "TransDB.h"
    "expimp.cpp"
    "expimp.h"
    "fileops.cpp"
    "fileops.h"
    "iff.cpp"
    "iff.h"
    "list.cpp"
    "list.h"
    "loadsave.cpp"
    "loadsave.h"
    "transcs.cpp"
    "transcs.h"
    
    # Windows-only files (conditionally compiled)
    # excel8.cpp and excel8.h require MFC - only include for VC6 builds
    # Qt builds don't use Excel COM integration (requires MFC)
    "XLStuff.cpp"
    "XLStuff.h"
    
    # Header files (cross-platform)
    "resource.h"
    "VIEWDBSII.h"
)

# Qt UI files - only for non-VC6 builds (VC6 doesn't support Qt)
if(NOT IS_VS6_BUILD AND TARGET core_qt)
    list(APPEND BABYLON_SRC
        "Babylon_Qt.cpp"
        "Babylon_Qt.h"
        "BabylonDlg_Qt.cpp"
        "BabylonDlg_Qt.h"
        "BabylonDlg.ui"
        "ExportDlg_Qt.cpp"
        "ExportDlg_Qt.h"
        "ExportDlg.ui"
        "GenerateDlg_Qt.cpp"
        "GenerateDlg_Qt.h"
        "VerifyDlg_Qt.cpp"
        "VerifyDlg_Qt.h"
        "VerifyTextDlg_Qt.cpp"
        "VerifyTextDlg_Qt.h"
        "VIEWDBSII_Qt.cpp"
        "VIEWDBSII_Qt.h"
        "Report_Qt.cpp"
        "Report_Qt.h"
        "MatchDlg_Qt.cpp"
        "MatchDlg_Qt.h"
        "RetranslateDlg_Qt.cpp"
        "RetranslateDlg_Qt.h"
        "ProceedDlg_Qt.cpp"
        "ProceedDlg_Qt.h"
    )
endif()

# Windows-only files (old MFC dialogs - for VC6 builds or when Qt is not available)
if((WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows") AND (IS_VS6_BUILD OR NOT TARGET core_qt))
    list(APPEND BABYLON_SRC
        "Babylon.cpp"
        "Babylon.h"
        "BabylonDlg.cpp"
        "BabylonDlg.h"
        "DlgProxy.cpp"
        "DlgProxy.h"
        "ExportDlg.cpp"
        "ExportDlg.h"
        "GenerateDlg.cpp"
        "GenerateDlg.h"
        "VerifyDlg.cpp"
        "VerifyDlg.h"
        "VerifyTextDlg.cpp"
        "VerifyTextDlg.h"
        "ViewDBsDlg.cpp"
        "MatchDlg.cpp"
        "MatchDlg.h"
        "ProceedDlg.cpp"
        "ProceedDlg.h"
        "Report.cpp"
        "Report.h"
        "RetranslateDlg.cpp"
        "RetranslateDlg.h"
        "VIEWDBSII.cpp"
        "StdAfx.cpp"
        "StdAfx.h"
        # Excel COM integration requires MFC - only for VC6/MFC builds
        "excel8.cpp"
        "excel8.h"
    )
endif()

add_executable(core_babylon)
set_target_properties(core_babylon PROPERTIES OUTPUT_NAME babylon)

target_sources(core_babylon PRIVATE ${BABYLON_SRC})

if(TARGET core_qt)
    target_link_libraries(core_babylon PRIVATE core_qt)
endif()

target_link_libraries(core_babylon PRIVATE
    core_config
)

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_sources(core_babylon PRIVATE Babylon.rc)
endif()
