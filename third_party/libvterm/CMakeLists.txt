cmake_minimum_required(VERSION 3.16)
project(libvterm C)

set(LIBVTERM_SOURCES
    src/encoding.c
    src/keyboard.c
    src/mouse.c
    src/parser.c
    src/pen.c
    src/screen.c
    src/state.c
    src/unicode.c
    src/vterm.c
)

add_library(vterm STATIC ${LIBVTERM_SOURCES})

target_include_directories(vterm
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    # libvterm POSIX-ish source, MSVC'nin C4146/C4244 gibi gevezeliklerini sustur
    target_compile_definitions(vterm PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(vterm PRIVATE /W3 /wd4244 /wd4267 /wd4146)
endif()

set_target_properties(vterm PROPERTIES POSITION_INDEPENDENT_CODE ON)
