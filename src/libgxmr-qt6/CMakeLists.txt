cmake_minimum_required(VERSION 3.16)
project(libgxmr-qt6 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMD_NAME gxmr-qt6)

add_definitions(-D_LIBDMR_)

find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS
    Widgets Concurrent Network Sql DBus Gui OpenGL OpenGLWidgets)

pkg_check_modules(DTK2W REQUIRED IMPORTED_TARGET dtk2widget dtk6core)

pkg_check_modules(Mpv REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(AV REQUIRED IMPORTED_TARGET libavformat libavutil libavcodec)
pkg_check_modules(Xcb REQUIRED IMPORTED_TARGET xcb xcb-aux xcb-ewmh xcb-shape)
pkg_check_modules(FFTHUMB REQUIRED libffmpegthumbnailer)
pkg_check_modules(Other REQUIRED libpulse libpulse-simple openssl dvdnav)

set(PROJECT_VERSION 3.3.1)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../../config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB_RECURSE SRCS LIST_DIRECTORIES false *.cpp)
file(GLOB_RECURSE MPV_SRCS LIST_DIRECTORIES false ../backends/mpv/*.cpp)
list(APPEND SRCS ${MPV_SRCS})

add_library(${CMD_NAME} SHARED ${SRCS})
set_target_properties(${CMD_NAME} PROPERTIES
    OUTPUT_NAME gxmr-qt6 VERSION 0.1.0 SOVERSION 0.1)

target_include_directories(${CMD_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../backends/mpv)

target_link_libraries(${CMD_NAME}
    PkgConfig::DTK2W
    Qt6::Widgets Qt6::Concurrent Qt6::Network Qt6::Sql Qt6::DBus
    Qt6::Gui Qt6::GuiPrivate Qt6::OpenGL Qt6::OpenGLWidgets
    PkgConfig::Mpv PkgConfig::AV PkgConfig::Xcb
    ${FFTHUMB_LIBRARIES} ${Other_LIBRARIES} pthread GL)

include(GNUInstallDirs)

configure_file(libgxmr-qt6.pc.in ${PROJECT_BINARY_DIR}/libgxmr-qt6.pc @ONLY)

install(TARGETS ${CMD_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES
    player_widget.h
    player_backend.h
    player_engine.h
    playlist_model.h
    movie_configuration.h
    compositing_manager.h
    dvd_utils.h
    utils.h
    opengl_cb.h
    online_sub.h
    qthelper.hpp
    DESTINATION include/libgxmr-qt6)

install(FILES ${PROJECT_BINARY_DIR}/libgxmr-qt6.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
