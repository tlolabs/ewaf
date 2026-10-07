# SPDX-FileCopyrightText: Thomas Lothian
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(EWAF VERSION 1.0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Test)

# Find ewaf_ffi library
if(NOT EWAF_FFI_LIB)
    if(APPLE)
        set(FFI_LIB_NAME libewaf_ffi.dylib)
    elseif(WIN32)
        set(FFI_LIB_NAME ewaf_ffi.dll)
    else()
        set(FFI_LIB_NAME libewaf_ffi.so)
    endif()

    find_file(EWAF_FFI_LIB ${FFI_LIB_NAME}
        PATHS
            ${CMAKE_CURRENT_SOURCE_DIR}/target/release
            ${CMAKE_CURRENT_SOURCE_DIR}/target/debug
            ${CMAKE_CURRENT_SOURCE_DIR}/target/aarch64-apple-darwin/release
            ${CMAKE_CURRENT_SOURCE_DIR}/target/x86_64-unknown-linux-gnu/release
            ${CMAKE_CURRENT_SOURCE_DIR}/target/aarch64-unknown-linux-gnu/release
            ${CMAKE_CURRENT_SOURCE_DIR}/target/x86_64-pc-windows-msvc/release
            ${CMAKE_CURRENT_SOURCE_DIR}/target/aarch64-pc-windows-msvc/release
        NO_DEFAULT_PATH
    )
    if(NOT EWAF_FFI_LIB)
        set(EWAF_FFI_LIB ewaf_ffi)
    endif()
endif()

message(STATUS "Using EWAF_FFI_LIB: ${EWAF_FFI_LIB}")

set(EWAF_QT_HEADERS
    platform/qt/include/core.h
    platform/qt/include/preferences.h
    platform/qt/include/identity.h
    platform/qt/include/workspace.h
    platform/qt/include/mainwindow.h
    platform/qt/include/app.h
    platform/qt/include/updateclient.h
    platform/qt/include/updatecoordinator.h
)

set(EWAF_QT_SOURCES
    platform/qt/src/core.cpp
    platform/qt/src/preferences.cpp
    platform/qt/src/workspace.cpp
    platform/qt/src/mainwindow.cpp
    platform/qt/src/app.cpp
    platform/qt/src/updateclient.cpp
    platform/qt/src/updatecoordinator.cpp
)

set(EWAF_QT_INCLUDES
    platform/qt/include
    bindings/include
)

# Main EWAF Application executable
add_executable(EWAF
    platform/qt/src/main.cpp
    ${EWAF_QT_SOURCES}
    ${EWAF_QT_HEADERS}
)

target_include_directories(EWAF PRIVATE ${EWAF_QT_INCLUDES})
target_link_libraries(EWAF PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    ${EWAF_FFI_LIB}
)

if(APPLE)
    target_compile_definitions(EWAF PRIVATE INTERNAL_REFERENCE=1)
    set_target_properties(EWAF PROPERTIES
        OUTPUT_NAME "EWAF"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "EWAF Qt Internal"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.tlolabs.ewaf.qt-internal"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
elseif(WIN32)
    target_link_libraries(EWAF PRIVATE advapi32 user32)
else()
    set_target_properties(EWAF PROPERTIES OUTPUT_NAME "ewaf")
endif()

# Shared Qt Test Executable
add_executable(ewaf_tests
    tests/qt/test_ewaf_qt.cpp
    ${EWAF_QT_SOURCES}
    ${EWAF_QT_HEADERS}
)

target_include_directories(ewaf_tests PRIVATE ${EWAF_QT_INCLUDES})
target_link_libraries(ewaf_tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Test
    ${EWAF_FFI_LIB}
)

if(APPLE)
    target_compile_definitions(ewaf_tests PRIVATE INTERNAL_REFERENCE=1)
elseif(WIN32)
    target_link_libraries(ewaf_tests PRIVATE advapi32 user32)
endif()

# Windows Installer verification helper
if(WIN32)
    add_executable(ewaf-installer
        platform/windows/installer/ewaf-installer.cpp
    )
    target_link_libraries(ewaf-installer PRIVATE
        wintrust
        crypt32
        msi
        user32
        advapi32
        bcrypt
    )
endif()
