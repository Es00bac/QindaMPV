cmake_minimum_required(VERSION 3.24)

# AGENT-NOTE: this version, the Portage ebuild version, and the git tag move
# together; the ebuild pins the commit it builds.
project(qqmpv VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 DBus OpenGL)
find_package(Qt6GuiPrivate 6.11 REQUIRED)
find_package(Qt6CorePrivate 6.11 REQUIRED)
qt_standard_project_setup()

# The QindaQt desktop ships these archives/headers without CMake package
# configs (owned by gui-wm/qindaqt-desktop), so resolve them explicitly
# instead of guessing paths. libqindaqt_app_shell.so is the backing library of
# the installed QindaQt.AppShell QML module and lives in the module directory.
find_library(QINDAQT_APP_SHELL_LIB NAMES qindaqt_app_shell
    PATH_SUFFIXES qt6/qml/QindaQt/AppShell)
find_library(QINDAQT_MENU_EXPORT_LIB NAMES qindaqt_app_shell_menu_export)
find_library(QINDAQT_MENU_DBUSMENU_LIB NAMES qindaqt_global_menu_dbusmenu)
find_library(QINDAQT_MENU_REGISTRAR_LIB NAMES qindaqt_global_menu_registrar)
find_library(QINDAQT_MENU_EXPORTER_LIB NAMES qindaqt_global_menu_exporter)
find_library(QINDAQT_MENU_OWNERSHIP_LIB NAMES qindaqt_global_menu_ownership)
find_library(QINDAQT_MENU_PROTOCOL_LIB NAMES qindaqt_global_menu_protocol)
find_path(QINDAQT_APP_SHELL_INCLUDE_DIR qindaqt/app_shell/application_coordinator.h)
find_path(QINDAQT_MENU_EXPORT_INCLUDE_DIR qindaqt/app_shell/menu_export/first_party_composition.h)

foreach(dependency IN ITEMS
        QINDAQT_APP_SHELL_LIB QINDAQT_MENU_EXPORT_LIB QINDAQT_MENU_DBUSMENU_LIB
        QINDAQT_MENU_REGISTRAR_LIB QINDAQT_MENU_EXPORTER_LIB QINDAQT_MENU_OWNERSHIP_LIB
        QINDAQT_MENU_PROTOCOL_LIB QINDAQT_APP_SHELL_INCLUDE_DIR QINDAQT_MENU_EXPORT_INCLUDE_DIR)
    if(NOT ${dependency})
        message(FATAL_ERROR "QindaMPV requires the QindaQt desktop libraries (${dependency}). "
                            "Install gui-wm/qindaqt-desktop first.")
    endif()
endforeach()

# The AppShell backing library sits outside the default linker search path;
# embed its directory so the installed binary resolves it without wrappers.
cmake_path(GET QINDAQT_APP_SHELL_LIB PARENT_PATH QINDAQT_APP_SHELL_LIB_DIR)

qt_add_executable(qqmpv
    src/main.cpp
    src/app/action_catalog.cpp
    src/app/action_catalog.h
    src/player/mpv_api.h
    src/player/mpv_library.cpp
    src/player/mpv_library.h
    src/player/player_controller.cpp
    src/player/player_controller.h
)

qt_add_qml_module(qqmpv
    URI QindaMPV
    VERSION 1.0
    # AGENT-GUARD: the engine resolves module QML from qrc via the default
    # /qt/qml import root. Without this prefix the resources land at qrc:/,
    # which only the build tree's filesystem qmldir masks — the installed
    # binary then fails with "module QindaMPV contains no type Main".
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/PlayerMenuBar.qml
        qml/ControlIsland.qml
        qml/SeekBar.qml
    SOURCES
        src/player/mpv_video_item.h
        src/player/mpv_video_item.cpp
)

target_include_directories(qqmpv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/player
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${QINDAQT_APP_SHELL_INCLUDE_DIR}
    ${QINDAQT_MENU_EXPORT_INCLUDE_DIR}
)

target_compile_definitions(qqmpv PRIVATE
    QQMPV_VERSION="${PROJECT_VERSION}"
    QT_NO_CAST_FROM_ASCII
)

set_target_properties(qqmpv PROPERTIES
    INSTALL_RPATH "${QINDAQT_APP_SHELL_LIB_DIR}"
)

# Static-archive order matters: dependents before dependencies.
target_link_libraries(qqmpv PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
    Qt6::DBus
    Qt6::Gui
    Qt6::OpenGL
    Qt6::GuiPrivate
    Qt6::CorePrivate
    ${QINDAQT_MENU_EXPORT_LIB}
    ${QINDAQT_MENU_DBUSMENU_LIB}
    ${QINDAQT_MENU_REGISTRAR_LIB}
    ${QINDAQT_MENU_EXPORTER_LIB}
    ${QINDAQT_MENU_OWNERSHIP_LIB}
    ${QINDAQT_MENU_PROTOCOL_LIB}
    ${QINDAQT_APP_SHELL_LIB}
)

install(TARGETS qqmpv RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/org.qindaqt.QQMpv.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/icons/hicolor/scalable/apps/qqmpv.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
