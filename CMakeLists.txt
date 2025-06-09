cmake_minimum_required(VERSION 3.16)
project(wallpaperengine-gui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(INSTALL_PAPIRUS_ICONS "Install icons for Papirus theme integration" OFF)
option(INSTALL_AUTOSTART_DESKTOP "Install autostart desktop file" OFF)
option(BUILD_RPM_PACKAGE "Configure for RPM packaging" OFF)

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Concurrent Network)

# Automatically handle Qt's MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/WallpaperManager.cpp
    src/SteamDetector.cpp
    src/WallpaperPreview.cpp
    src/PropertiesPanel.cpp
    src/SettingsDialog.cpp
    src/ConfigManager.cpp
    src/SteamApiManager.cpp
    src/WallpaperPlaylist.cpp
    src/PlaylistPreview.cpp
)

# Header files
set(HEADERS
    src/MainWindow.h
    src/WallpaperManager.h
    src/SteamDetector.h
    src/WallpaperPreview.h
    src/PropertiesPanel.h
    src/SettingsDialog.h
    src/ConfigManager.h
    src/SteamApiManager.h
    src/WallpaperPlaylist.h
    src/PlaylistPreview.h
)

# Resource files
set(RESOURCES
    resources/icons.qrc
)

# Create executable
add_executable(wallpaperengine-gui
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# Link Qt libraries - Fixed order and added all required components
target_link_libraries(wallpaperengine-gui
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Concurrent
    Qt6::Network
)

# Compiler flags
target_compile_options(wallpaperengine-gui PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# Installation
install(TARGETS wallpaperengine-gui
    RUNTIME DESTINATION bin
)

# Desktop file
set(ICON_INSTALL_DIR "share/pixmaps")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/desktop/wallpaperengine-gui.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/wallpaperengine-gui.desktop
    @ONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/desktop/wallpaperengine-gui-autostart.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/wallpaperengine-gui-autostart.desktop
    @ONLY
)

# Desktop file installation
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/wallpaperengine-gui.desktop
    DESTINATION share/applications
)

# Optional autostart desktop file installation
if(INSTALL_AUTOSTART_DESKTOP)
    message(STATUS "Autostart desktop file installation enabled")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/wallpaperengine-gui-autostart.desktop
        DESTINATION share/applications
    )
endif()

# Icon installation - multiple sizes for better desktop integration
install(FILES resources/icons/wallpaper-engine.svg
    DESTINATION share/icons/hicolor/scalable/apps
    RENAME wallpaperengine-gui.svg
    OPTIONAL
)

install(FILES resources/icons/wallpaper-engine.png
    DESTINATION share/icons/hicolor/64x64/apps
    RENAME wallpaperengine-gui.png
    OPTIONAL
)

# Fallback icon installation
install(FILES resources/icons/wallpaper-engine.png
    DESTINATION ${ICON_INSTALL_DIR}
    RENAME wallpaperengine-gui.png
)

# Optional Papirus theme integration
if(INSTALL_PAPIRUS_ICONS)
    message(STATUS "Papirus icon theme integration enabled")
    install(FILES resources/icons/wallpaper-engine-papirus.svg
        DESTINATION share/icons/Papirus/64x64/apps
        RENAME wallpaperengine-gui.svg
    )
    
    install(FILES resources/icons/wallpaper-engine-papirus.svg
        DESTINATION share/icons/Papirus-Dark/64x64/apps
        RENAME wallpaperengine-gui.svg
    )
    
    # Install additional Papirus-styled icons
    install(FILES resources/icons/refresh-papirus.svg
        DESTINATION share/icons/Papirus/24x24/actions
        RENAME refresh.svg
        OPTIONAL
    )
    
    install(FILES resources/icons/settings-papirus.svg
        DESTINATION share/icons/Papirus/24x24/categories
        RENAME preferences-system.svg
        OPTIONAL
    )
    
    install(FILES resources/icons/folder-papirus.svg
        DESTINATION share/icons/Papirus/24x24/places
        RENAME folder-open.svg
        OPTIONAL
    )
endif()

# Add uninstall target
# Create uninstall script
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)

# Add uninstall target
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    COMMENT "Uninstalling wallpaperengine-gui..."
)

# RPM packaging support
set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGE_NAME "wallpaperengine-gui")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern GUI for Linux Wallpaper Engine")
set(CPACK_PACKAGE_DESCRIPTION "A feature-rich graphical user interface for managing and configuring Wallpaper Engine wallpapers on Linux. Includes Steam Workshop integration, live preview, advanced search, and comprehensive settings management.")
set(CPACK_PACKAGE_VENDOR "WallpaperEngine GUI Project")
set(CPACK_PACKAGE_CONTACT "wallpaperengine-gui@example.com")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_URL "https://github.com/yourusername/wallpaperengine-gui")

# Enhanced RPM dependencies
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.0, qt6-qtbase-gui >= 6.0")
set(CPACK_RPM_PACKAGE_SUGGESTS "papirus-icon-theme, steam")
set(CPACK_RPM_PACKAGE_PROVIDES "wallpaperengine-gui")

# RPM package description
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")

# Build information
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_RPM_PACKAGE_RELEASE "1")

# Post-install scripts for icon cache update
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/rpm-post-install.sh")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/rpm-post-uninstall.sh")

# File ownership
set(CPACK_RPM_DEFAULT_USER "root")
set(CPACK_RPM_DEFAULT_GROUP "root")
set(CPACK_RPM_DEFAULT_DIRMODE "755")
set(CPACK_RPM_DEFAULT_FILEMODE "644")

# Exclude build directory and temporary files
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/local
    /usr/local/bin
    /usr/local/share
    /usr/local/share/applications
    /usr/local/share/pixmaps
    /usr/local/share/icons
    /usr/local/share/icons/hicolor
    /usr/local/share/icons/hicolor/64x64
    /usr/local/share/icons/hicolor/64x64/apps
    /usr/local/share/icons/hicolor/scalable
    /usr/local/share/icons/hicolor/scalable/apps
    /usr/local/share/icons/Papirus
    /usr/local/share/icons/Papirus/64x64
    /usr/local/share/icons/Papirus/64x64/apps
    /usr/local/share/icons/Papirus-Dark
    /usr/local/share/icons/Papirus-Dark/64x64
    /usr/local/share/icons/Papirus-Dark/64x64/apps
)

include(CPack)
