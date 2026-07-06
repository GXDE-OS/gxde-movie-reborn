set(CMAKE_AUTOMOC ON)
set(CMD_NAME gxde-movie)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_INCLUDE ${PROJECT_SOURCE_DIR}/src/widgets
    ${PROJECT_SOURCE_DIR}/src/common
    ${PROJECT_SOURCE_DIR}/src/libgxmr-qt6)

include_directories(${CMAKE_INCLUDE_CURRENT_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(Qt6 REQUIRED COMPONENTS
    Widgets Concurrent Network Sql DBus Gui OpenGL OpenGLWidgets LinguistTools)

pkg_check_modules(DTK2W REQUIRED IMPORTED_TARGET dtk2widget dtk6core)

pkg_check_modules(Mpv REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(Xcb REQUIRED IMPORTED_TARGET xcb xcb-aux
    xcb-proto xcb-ewmh xcb-shape)
pkg_check_modules(AV REQUIRED IMPORTED_TARGET libavformat
    libavutil libavcodec)
# IMPORTED_TARGET failed to work for some of libs under flatpak env
pkg_check_modules(Other REQUIRED libffmpegthumbnailer
    libpulse libpulse-simple openssl dvdnav)

qt_add_resources(RCS resources.qrc)


file(GLOB_RECURSE SRCS LIST_DIRECTORIES false common/*.cpp widgets/*.cpp libgxmr-qt6/*.cpp)
list(APPEND SRCS main.cpp)

# mpv backend
file(GLOB_RECURSE MPV_SRCS LIST_DIRECTORIES false backends/mpv/*.cpp)
list(APPEND SRCS ${MPV_SRCS})
list(APPEND PROJECT_INCLUDE ${PROJECT_SOURCE_DIR}/src/backends/mpv)
#~

set (DTK_SETTINGS_TOOLS_EXECUTABLE ${DTKCORE_TOOL_DIR}/dtk-settings)
if (EXISTS ${DTK_SETTINGS_TOOLS_EXECUTABLE})
    add_custom_target(json_i18n ALL
        ${DTKCORE_TOOL_DIR}/dtk-settings resources/data/settings.json -o common/settings_translation.cpp
        COMMAND sed -i "s,#include .*,#include <DSettings>," common/settings_translation.cpp
        VERBATIM
        SOURCES resources/data/settings.json
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/src/)
endif ()

## translations
file(GLOB TS LIST_DIRECTORIES false translations/${CMD_NAME}*.ts)

set_source_files_properties(${TS}
    PROPERTIES OUTPUT_LOCATION ${PROJECT_SOURCE_DIR}/src/translations)

if (NOT (${CMAKE_BUILD_TYPE} MATCHES "Debug"))
    qt_add_translation(QM ${TS})
endif ()
# ~

add_executable(${CMD_NAME} ${SRCS} ${RCS} ${QM})
target_include_directories(${CMD_NAME} PUBLIC ${PROJECT_INCLUDE})

set(TARGET_LIBS X11 Xext Xtst PkgConfig::Xcb Qt6::Widgets Qt6::Network
    Qt6::Concurrent Qt6::DBus Qt6::Sql Qt6::Gui Qt6::GuiPrivate
    Qt6::OpenGL Qt6::OpenGLWidgets
    PkgConfig::Mpv
    PkgConfig::AV pthread GL)
target_link_libraries(${CMD_NAME} ${TARGET_LIBS} PkgConfig::DTK2W ${Other_LIBRARIES})


install(TARGETS ${CMD_NAME} DESTINATION bin)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/translations
    DESTINATION share/${CMD_NAME}
    FILES_MATCHING PATTERN "*.qm")
install(FILES ${PROJECT_SOURCE_DIR}/src/gxde-movie.desktop
    DESTINATION share/applications)
install(FILES ${PROJECT_SOURCE_DIR}/src/resources/icons/logo-big.svg
    DESTINATION share/icons/hicolor/scalable/apps
    RENAME gxde-movie.svg)

add_subdirectory(libgxmr)
add_subdirectory(libgxmr-qt6)
add_subdirectory(test)
