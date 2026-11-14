cmake_minimum_required(VERSION 3.21)

project(AeroDesktop
    VERSION 0.1.0
    DESCRIPTION "Aero Desktop - a liquid-glass desktop environment for DreamyOS"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(cmake/AeroHelpers.cmake)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Widgets Network DBus Concurrent)

qt_standard_project_setup()

option(AERO_BUILD_DREAMYOS_BACKEND
    "Build the DreamyOS platform backend scaffold" OFF)
option(AERO_BUILD_TESTS
    "Build Aero Desktop unit tests" OFF)
option(AERO_SANITIZE
    "Enable address/undefined sanitizers for development builds" OFF)

if(AERO_SANITIZE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(src/core)
add_subdirectory(src/platform)
add_subdirectory(src/theme)
add_subdirectory(src/graphics)
add_subdirectory(src/settings)
add_subdirectory(src/compositor)
add_subdirectory(src/services)
add_subdirectory(src/apps)
add_subdirectory(src/shell)
add_subdirectory(src/main)

if(AERO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS aero-desktop RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/aero-desktop.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/aero-session.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/xsessions)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR})
