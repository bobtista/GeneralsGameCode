# Qt CMake configuration
# TheSuperHackers @build bobtista 01/01/2025 Qt integration for MFC to Qt migration

# Find Qt6 first, fall back to Qt5 if not found
find_package(Qt6 QUIET COMPONENTS Core Widgets Gui)

if(Qt6_FOUND)
    message(STATUS "Found Qt6")
    set(QT_VERSION_MAJOR 6)
    set(QT_USE_QT6 TRUE)
else()
    find_package(Qt5 5.15 QUIET COMPONENTS Core Widgets Gui)
    if(Qt5_FOUND)
        message(STATUS "Found Qt5")
        set(QT_VERSION_MAJOR 5)
        set(QT_USE_QT6 FALSE)
    else()
        message(WARNING "Qt not found. Qt-based tools will not be built.")
        return()
    endif()
endif()

# Set up Qt for automatic MOC, UIC, RCC
if(QT_USE_QT6)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    
    # Create interface library for Qt
    add_library(core_qt INTERFACE)
    target_link_libraries(core_qt INTERFACE
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
    )
else()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    
    # Create interface library for Qt
    add_library(core_qt INTERFACE)
    target_link_libraries(core_qt INTERFACE
        Qt5::Core
        Qt5::Widgets
        Qt5::Gui
    )
endif()

# Add feature info
add_feature_info(Qt core_qt "Qt GUI framework for migrated tools")

