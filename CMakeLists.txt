cmake_minimum_required(VERSION 3.16)
project(ssh-tool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Windows 7 hedef
option(SSHTOOL_STATIC_RUNTIME "Link MSVC runtime statically (shipping build)" OFF)
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601 -DWINVER=0x0601)
    if(MSVC)
        if(SSHTOOL_STATIC_RUNTIME)
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        else()
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        endif()
        add_compile_options(/utf-8 /W3)
    endif()
endif()

# Qt 5 statik
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Network)

# libssh (FindPkgConfig veya elle path)
find_package(libssh CONFIG REQUIRED)

# Vendor'lanmış libvterm (third_party/libvterm)
add_subdirectory(third_party/libvterm)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/connectionsdialog.cpp
    src/connectionsdialog.h
    src/sshsession.cpp
    src/sshsession.h
    src/sftpmodel.cpp
    src/sftpmodel.h
    src/terminalwidget.cpp
    src/terminalwidget.h
    src/serverconfig.cpp
    src/serverconfig.h
    src/editorwindow.cpp
    src/editorwindow.h
    src/syntaxhighlighter.cpp
    src/syntaxhighlighter.h
    resources/app.qrc
)

add_executable(ssh-tool WIN32 ${SOURCES})

target_link_libraries(ssh-tool PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    ssh
    vterm
)

# Manifest: Win7+ uyumluluk ve modern controls
# Icon: explorer ve taskbar için
if(WIN32)
    target_sources(ssh-tool PRIVATE
        resources/ssh-tool.manifest
        resources/ssh-tool.rc
    )
endif()
