# WallpaperEngine GUI

GUI for the [Almamu/linux-wallpaperengine](https://github.com/Almamu/linux-wallpaperengine).
Now has a pluggable addon [wallpaper_not-engine_linux](https://github.com/MikiDevLog/wallpaper_not-engine_linux).
In short - will allow to run regular media files in full compatibility with regular wallpapers on x11 and Wayland too, just like in Wallpaper Engine (this feature is experemental due to lack of testing in different contitions, you can help with this, when you find any issue related to this addon, just submit it to [issues](https://github.com/MikiDevLog/wallpaper_not-engine_linux/issues))
- No need to transfer your pictures or videos to a separate folder, binding occurs via symlinks.
- The interface does not change much, allowing to use both types of wallpapers.

# Features

- Mostly automated first setup.
- Can get information about wallpaper based on workshop id using Steam API and store it. [You need to get API key for that](https://steamcommunity.com/dev/apikey) any domain name will do.
- Support QT Themes for KDE users.
- Somewhat usable in terms of design (but now even better).
- Restore previous state on restart.
- Global settings
- Has playlist support
- Has support for changing wallpaper properties that are specified in project.json with the ability to save them between launches and roll back to the default state **(realy depends on linux-wallpaperengine since it wont support every wallpaper as of now, and some wallpaper wont react to any change at all since they use advnaced WE specific features that are not implemented currently)**
- Has support for individual settings (settings specific to linux-wallpaperengine) for running wallpapers
- Has support for working in the system tray

## Screenshots

![1](screenshots/image.png)
![2](screenshots/image2.png)
![3](screenshots/image3.png)
![4](screenshots/image4.png)
![5](screenshots/image5.png)
![6](screenshots/image6.png)

## Prerequisites

[Almamu/linux-wallpaperengine](https://github.com/Almamu/linux-wallpaperengine)
You need binary (~/build/output/linux-wallpaperengine) compiled. Go to link above for the instructions. **And make sure you are using the correct DE and composer and installed all dependencies from his instruction.**

### Fedora / RHEL / CentOS
```bash
# Install development tools
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake gcc-c++

# Install Qt6 dependencies
sudo dnf install qt6-qtbase-devel qt6-qtwebengine-devel qt6-qtmultimedia-devel
```

### Ubuntu / Debian
```bash
# Install development tools
sudo apt install build-essential cmake

# Install Qt6 dependencies
sudo apt install qt6-base-dev qt6-webengine-dev qt6-multimedia-dev
```

### Arch Linux
```bash
# Install development tools
sudo pacman -S base-devel cmake

# Install Qt6 dependencies
sudo pacman -S qt6-base qt6-webengine qt6-multimedia
```

#### Build Steps
```bash
git clone https://github.com/MikiDevLog/wallpaperengine-gui.git
cd wallpaperengine-gui
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install # But only you want it to be visible in yours app manager.
```

## Usage

### First Launch
1. Run `wallpaperengine-gui`
2. Configure path in Settings if not auto-detected
3. Browse and select wallpapers from your collection
4. Enjoy

### Keyboard Shortcuts
- `F5` - Refresh wallpaper collection
- `Ctrl+,` - Open Settings
- `Ctrl+Q` - Quit application

## Configuration

Configuration files are stored in `/home/<user>/.config/wallpaperengine-gui/`:
- `config.ini` - Main application settings
- `wallpapers.cache` - Wallpaper metadata cache

## Dependencies

### Runtime Dependencies
- Qt6 Base (Core, Widgets, GUI)
- Qt6 Concurrent
- Qt6 Network
- Steam
- Copy of Wallpaper Engine installed
- Wallpapers downloaded from workshop
- Compiled [linux-wallpaperengine](https://github.com/Almamu/linux-wallpaperengine)

## Contributing

Contributions are welcome.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details. 
**TL:DR Do what you want.**

## Acknowledgments

- Big fat W for this guy [Almamu/linux-wallpaperengine](https://github.com/Almamu/linux-wallpaperengine)
- Inspired by the original Wallpaper Engine for Windows

## Troubleshooting

### Common Build Issues

#### Qt6 Not Found
```bash
# Ensure Qt6 is installed and CMake can find it
export Qt6_DIR=/usr/lib64/cmake/Qt6
# or
export CMAKE_PREFIX_PATH=/usr/lib64/cmake/Qt6
```

#### Missing Dependencies
```bash
# Check for missing development packages
sudo dnf install qt6-qtbase-private-devel qt6-qtwebengine-private-devel
```

### Runtime Issues

#### Application Won't Start
```bash
# Check dependencies
ldd build/wallpaperengine-gui

# Run with debug output
QT_LOGGING_RULES="*=true" ./wallpaperengine-gui

# Run with --debug flag from terminal
./wallpaperengine-gui --debug
wallpaperengine-gui --debug
```

### Code Structure
```
src/
├── main.cpp              # Application entry point
├── addons/               # For addons
│   └──WNELAddon*         # wallpaper_not-engine_linux addon
├── core/                 # Core functionality
│   ├── ConfigManager.*   # Configuration management
│   └── WallpaperManager.*# Wallpaper management logic
├── steam/                # Steam integration
│   ├── SteamDetector.*   # Steam installation detection
│   └── SteamApiManager.* # Steam API integration
├── ui/                   # Main UI components
│   ├── MainWindow.*      # Main application window
│   ├── SettingsDialog.*  # Settings interface
│   └── PropertiesPanel.* # Wallpaper properties panel
├── widgets/              # Custom widgets
│   ├── WallpaperPreview.*# Wallpaper preview widget
│   └── PlaylistPreview.* # Playlist preview widget
└── playlist/             # Playlist functionality
    └── WallpaperPlaylist.*# Playlist management logic
```

### TODO

1. Add multimonitor mode
2. Release rpm apt and other packages for the application
3. Focus on QoL features if any will be suggested