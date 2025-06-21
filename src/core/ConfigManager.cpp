#include "ConfigManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QStyleFactory>
#include <QApplication>
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(configManager, "app.configManager")

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/wallpaperengine-gui";
    QDir().mkpath(configPath);
    
    m_settings = new QSettings(configPath + "/config.ini", QSettings::IniFormat, this);
}

ConfigManager& ConfigManager::instance()
{
    static ConfigManager instance;
    return instance;
}

QString ConfigManager::configDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/wallpaperengine-gui";
}

void ConfigManager::resetToDefaults()
{
    m_settings->clear();
    m_settings->sync();
}

// Wallpaper Engine binary
QString ConfigManager::wallpaperEnginePath() const
{
    return m_settings->value("paths/wallpaper_engine_binary", "").toString();
}

QString ConfigManager::getWallpaperEnginePath() const
{
    return wallpaperEnginePath();
}

void ConfigManager::setWallpaperEnginePath(const QString& path)
{
    m_settings->setValue("paths/wallpaper_engine_binary", path);
    m_settings->sync();
}

// Steam paths
QString ConfigManager::steamPath() const
{
    return m_settings->value("steam/path", "").toString();
}

void ConfigManager::setSteamPath(const QString& path)
{
    m_settings->setValue("steam/path", path);
    m_settings->sync();
}

QStringList ConfigManager::steamLibraryPaths() const
{
    return m_settings->value("steam/library_paths", QStringList()).toStringList();
}

void ConfigManager::setSteamLibraryPaths(const QStringList& paths)
{
    m_settings->setValue("steam/library_paths", paths);
    m_settings->sync();
}

// Assets directory
QString ConfigManager::getAssetsDir() const
{
    return m_settings->value("paths/assets_dir", "").toString();
}

void ConfigManager::setAssetsDir(const QString& dir)
{
    m_settings->setValue("paths/assets_dir", dir);
    m_settings->sync();
}

QStringList ConfigManager::findPossibleAssetsPaths() const
{
    QStringList paths;
    
    // Get all Steam library paths
    QStringList libraryPaths = steamLibraryPaths();
    
    for (const QString& libraryPath : libraryPaths) {
        // Standard Wallpaper Engine installation paths
        QStringList candidates = {
            QDir(libraryPath).filePath("steamapps/common/wallpaper_engine"),
            QDir(libraryPath).filePath("steamapps/common/wallpaper_engine/assets"),
            QDir(libraryPath).filePath("steamapps/common/wallpaper_engine/bin/assets")
        };
        
        for (const QString& candidate : candidates) {
            if (QDir(candidate).exists()) {
                paths.append(candidate);
                
                // Also check if this contains shaders
                if (QDir(candidate + "/shaders").exists()) {
                    paths.prepend(candidate); // Prioritize paths with shaders
                }
            }
        }
    }
    
    paths.removeDuplicates();
    return paths;
}

// Audio settings
int ConfigManager::masterVolume() const
{
    return m_settings->value("audio/master_volume", 15).toInt(); // Default 15% like linux-wallpaperengine
}

void ConfigManager::setMasterVolume(int volume)
{
    m_settings->setValue("audio/master_volume", volume);
    m_settings->sync();
}

QString ConfigManager::audioDevice() const
{
    return m_settings->value("audio/device", "default").toString();
}

void ConfigManager::setAudioDevice(const QString& device)
{
    m_settings->setValue("audio/device", device);
    m_settings->sync();
}

bool ConfigManager::muteOnFocus() const
{
    return m_settings->value("audio/mute_on_focus", false).toBool();
}

void ConfigManager::setMuteOnFocus(bool mute)
{
    m_settings->setValue("audio/mute_on_focus", mute);
    m_settings->sync();
}

bool ConfigManager::muteOnFullscreen() const
{
    return m_settings->value("audio/mute_on_fullscreen", false).toBool();
}

void ConfigManager::setMuteOnFullscreen(bool mute)
{
    m_settings->setValue("audio/mute_on_fullscreen", mute);
    m_settings->sync();
}

bool ConfigManager::noAutoMute() const
{
    return m_settings->value("audio/no_auto_mute", false).toBool();
}

void ConfigManager::setNoAutoMute(bool noAutoMute)
{
    m_settings->setValue("audio/no_auto_mute", noAutoMute);
    m_settings->sync();
}

bool ConfigManager::noAudioProcessing() const
{
    return m_settings->value("audio/no_audio_processing", false).toBool();
}

void ConfigManager::setNoAudioProcessing(bool noProcessing)
{
    m_settings->setValue("audio/no_audio_processing", noProcessing);
    m_settings->sync();
}

// Performance settings
int ConfigManager::targetFps() const
{
    return m_settings->value("performance/fps", 30).toInt();
}

void ConfigManager::setTargetFps(int fps)
{
    m_settings->setValue("performance/fps", fps);
    m_settings->sync();
}

bool ConfigManager::cpuLimitEnabled() const
{
    return m_settings->value("performance/cpu_limit_enabled", false).toBool();
}

void ConfigManager::setCpuLimitEnabled(bool enabled)
{
    m_settings->setValue("performance/cpu_limit_enabled", enabled);
    m_settings->sync();
}

int ConfigManager::cpuLimit() const
{
    return m_settings->value("performance/cpu_limit", 50).toInt();
}

void ConfigManager::setCpuLimit(int limit)
{
    m_settings->setValue("performance/cpu_limit", limit);
    m_settings->sync();
}

// Behavior settings
bool ConfigManager::pauseOnFocus() const
{
    return m_settings->value("behavior/pause_on_focus", false).toBool();
}

void ConfigManager::setPauseOnFocus(bool pause)
{
    m_settings->setValue("behavior/pause_on_focus", pause);
    m_settings->sync();
}

bool ConfigManager::pauseOnFullscreen() const
{
    return m_settings->value("behavior/pause_on_fullscreen", true).toBool();
}

void ConfigManager::setPauseOnFullscreen(bool pause)
{
    m_settings->setValue("behavior/pause_on_fullscreen", pause);
    m_settings->sync();
}

bool ConfigManager::disableMouse() const
{
    return m_settings->value("behavior/disable_mouse", false).toBool();
}

void ConfigManager::setDisableMouse(bool disable)
{
    m_settings->setValue("behavior/disable_mouse", disable);
    m_settings->sync();
}

bool ConfigManager::disableParallax() const
{
    return m_settings->value("behavior/disable_parallax", false).toBool();
}

void ConfigManager::setDisableParallax(bool disable)
{
    m_settings->setValue("behavior/disable_parallax", disable);
    m_settings->sync();
}

// Rendering settings
QString ConfigManager::renderMode() const
{
    return m_settings->value("rendering/mode", "Auto").toString();
}

void ConfigManager::setRenderMode(const QString& mode)
{
    m_settings->setValue("rendering/mode", mode);
    m_settings->sync();
}

QString ConfigManager::msaaLevel() const
{
    return m_settings->value("rendering/msaa", "Off").toString();
}

void ConfigManager::setMsaaLevel(const QString& level)
{
    m_settings->setValue("rendering/msaa", level);
    m_settings->sync();
}

int ConfigManager::anisotropicFiltering() const
{
    return m_settings->value("rendering/anisotropic", 1).toInt();
}

void ConfigManager::setAnisotropicFiltering(int level)
{
    m_settings->setValue("rendering/anisotropic", level);
    m_settings->sync();
}

bool ConfigManager::vsyncEnabled() const
{
    return m_settings->value("rendering/vsync", true).toBool();
}

void ConfigManager::setVsyncEnabled(bool enabled)
{
    m_settings->setValue("rendering/vsync", enabled);
    m_settings->sync();
}

bool ConfigManager::bloomEnabled() const
{
    return m_settings->value("rendering/bloom", true).toBool();
}

void ConfigManager::setBloomEnabled(bool enabled)
{
    m_settings->setValue("rendering/bloom", enabled);
    m_settings->sync();
}

bool ConfigManager::reflectionsEnabled() const
{
    return m_settings->value("rendering/reflections", true).toBool();
}

void ConfigManager::setReflectionsEnabled(bool enabled)
{
    m_settings->setValue("rendering/reflections", enabled);
    m_settings->sync();
}

// Advanced settings
QString ConfigManager::windowMode() const
{
    return m_settings->value("display/window_mode", "").toString();
}

void ConfigManager::setWindowMode(const QString& mode)
{
    m_settings->setValue("display/window_mode", mode);
    m_settings->sync();
}

QString ConfigManager::screenRoot() const
{
    return m_settings->value("display/screen_root", "").toString();
}

void ConfigManager::setScreenRoot(const QString& root)
{
    m_settings->setValue("display/screen_root", root);
    m_settings->sync();
}

QString ConfigManager::clampingMode() const
{
    return m_settings->value("rendering/clamping_mode", "clamp").toString();
}

void ConfigManager::setClampingMode(const QString& mode)
{
    m_settings->setValue("rendering/clamping_mode", mode);
    m_settings->sync();
}

QString ConfigManager::getScaling() const
{
    return m_settings->value("rendering/scaling", "").toString();
}

void ConfigManager::setScaling(const QString& scaling)
{
    m_settings->setValue("rendering/scaling", scaling);
    m_settings->sync();
}

bool ConfigManager::getSilent() const
{
    return m_settings->value("General/silent", false).toBool();
}

void ConfigManager::setSilent(bool silent)
{
    m_settings->setValue("General/silent", silent);
    m_settings->sync();
}

// Theme settings
QString ConfigManager::theme() const
{
    return m_settings->value("ui/theme", "").toString();
}

void ConfigManager::setTheme(const QString& theme)
{
    m_settings->setValue("ui/theme", theme);
    m_settings->sync();
}

QString ConfigManager::qtTheme() const
{
    return m_settings->value("ui/theme", "System Default").toString();
}

void ConfigManager::setQtTheme(const QString& theme)
{
    m_settings->setValue("ui/theme", theme);
    m_settings->sync();
}

QStringList ConfigManager::availableQtThemes() const
{
    QStringList themes;
    themes << "System Default";
    themes << QStyleFactory::keys();
    return themes;
}

// Window state
QByteArray ConfigManager::windowGeometry() const
{
    return m_settings->value("ui/window_geometry").toByteArray();
}

void ConfigManager::setWindowGeometry(const QByteArray& geometry)
{
    m_settings->setValue("ui/window_geometry", geometry);
    m_settings->sync();
}

QByteArray ConfigManager::windowState() const
{
    return m_settings->value("ui/window_state").toByteArray();
}

void ConfigManager::setWindowState(const QByteArray& state)
{
    m_settings->setValue("ui/window_state", state);
    m_settings->sync();
}

QByteArray ConfigManager::getSplitterState() const
{
    return m_settings->value("ui/splitter_state").toByteArray();
}

void ConfigManager::setSplitterState(const QByteArray& state)
{
    m_settings->setValue("ui/splitter_state", state);
    m_settings->sync();
}

// Application state
bool ConfigManager::isFirstRun() const
{
    return m_settings->value("General/first_run", true).toBool();
}

void ConfigManager::setFirstRun(bool firstRun)
{
    m_settings->setValue("General/first_run", firstRun);
    m_settings->sync();
}

// Configuration validation
bool ConfigManager::isConfigurationValid() const
{
    // Check if we have valid paths configured
    return hasValidPaths();
}

bool ConfigManager::hasValidPaths() const
{
    // Check if we have at least one Steam library path that exists
    QStringList libraryPaths = steamLibraryPaths();
    for (const QString& path : libraryPaths) {
        if (!path.isEmpty() && QDir(path).exists()) {
            // Check if this library contains wallpaper engine
            QString weePath = QDir(path).filePath("steamapps/common/wallpaper_engine");
            if (QDir(weePath).exists()) {
                return true;
            }
        }
    }
    
    // Alternative: Check if Steam path is configured and valid
    QString steamPath = this->steamPath();
    if (!steamPath.isEmpty() && QDir(steamPath).exists()) {
        return true;
    }
    
    return false;
}

QString ConfigManager::getConfigurationIssues() const
{
    QStringList issues;
    
    // Check Steam paths
    bool hasSteamPath = false;
    QString steamPath = this->steamPath();
    QStringList libraryPaths = steamLibraryPaths();
    
    if (!steamPath.isEmpty() && QDir(steamPath).exists()) {
        hasSteamPath = true;
    }
    
    bool hasValidLibrary = false;
    for (const QString& path : libraryPaths) {
        if (!path.isEmpty() && QDir(path).exists()) {
            QString weePath = QDir(path).filePath("steamapps/common/wallpaper_engine");
            if (QDir(weePath).exists()) {
                hasValidLibrary = true;
                break;
            }
        }
    }
    
    if (!hasSteamPath && !hasValidLibrary) {
        issues << "No valid Steam installation or library paths configured";
    }
    
    // Check Wallpaper Engine binary (optional but recommended)
    QString enginePath = wallpaperEnginePath();
    if (!enginePath.isEmpty() && !QFileInfo(enginePath).exists()) {
        issues << "Wallpaper Engine binary path is configured but file doesn't exist";
    }
    
    if (issues.isEmpty()) {
        return QString();
    }
    
    return "Configuration issues found:\n• " + issues.join("\n• ");
}

QString ConfigManager::lastSelectedWallpaper() const
{
    QString result = m_settings->value("General/last_wallpaper", "").toString();
    qCDebug(configManager) << "ConfigManager::lastSelectedWallpaper() returning:" << result;
    return result;
}

void ConfigManager::setLastSelectedWallpaper(const QString& wallpaperId)
{
    qCDebug(configManager) << "ConfigManager::setLastSelectedWallpaper() writing:" << wallpaperId;
    m_settings->setValue("General/last_wallpaper", wallpaperId);
    m_settings->sync();
    
    // Verify the write
    QString verify = m_settings->value("General/last_wallpaper", "").toString();
    qCDebug(configManager) << "ConfigManager::setLastSelectedWallpaper() verification read:" << verify;
}

int ConfigManager::refreshInterval() const
{
    return m_settings->value("General/refresh_interval", 30).toInt();
}

void ConfigManager::setRefreshInterval(int seconds)
{
    m_settings->setValue("General/refresh_interval", seconds);
    m_settings->sync();
}

bool ConfigManager::lastSessionUsedPlaylist() const
{
    return m_settings->value("General/last_session_used_playlist", false).toBool();
}

void ConfigManager::setLastSessionUsedPlaylist(bool usedPlaylist)
{
    qCDebug(configManager) << "ConfigManager::setLastSessionUsedPlaylist() writing:" << usedPlaylist;
    m_settings->setValue("General/last_session_used_playlist", usedPlaylist);
    m_settings->sync();
}

// Steam API settings
QString ConfigManager::steamApiKey() const
{
    return m_settings->value("steam/api_key", "").toString();
}

void ConfigManager::setSteamApiKey(const QString& apiKey)
{
    m_settings->setValue("steam/api_key", apiKey);
    m_settings->sync();
}

bool ConfigManager::useSteamApi() const
{
    return m_settings->value("steam/use_api", true).toBool();
}

void ConfigManager::setUseSteamApi(bool use)
{
    m_settings->setValue("steam/use_api", use);
    m_settings->sync();
}

QDateTime ConfigManager::lastApiUpdate() const
{
    return m_settings->value("steam_api/last_update").toDateTime();
}

void ConfigManager::setLastApiUpdate(const QDateTime& dateTime)
{
    m_settings->setValue("steam_api/last_update", dateTime);
    m_settings->sync();
}

// System tray settings
bool ConfigManager::showTrayWarning() const
{
    return m_settings->value("ui/show_tray_warning", true).toBool();
}

void ConfigManager::setShowTrayWarning(bool show)
{
    m_settings->setValue("ui/show_tray_warning", show);
    m_settings->sync();
}

// WNEL Addon settings
bool ConfigManager::isWNELAddonEnabled() const
{
    return m_settings->value("wnel/enabled", false).toBool();
}

void ConfigManager::setWNELAddonEnabled(bool enabled)
{
    m_settings->setValue("wnel/enabled", enabled);
    m_settings->sync();
}

QString ConfigManager::externalWallpapersPath() const
{
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/external_wallpapers";
    return m_settings->value("wnel/external_wallpapers_path", defaultPath).toString();
}

void ConfigManager::setExternalWallpapersPath(const QString& path)
{
    m_settings->setValue("wnel/external_wallpapers_path", path);
    m_settings->sync();
}

QString ConfigManager::wnelBinaryPath() const
{
    return m_settings->value("wnel/binary_path", "").toString();
}

void ConfigManager::setWNELBinaryPath(const QString& path)
{
    m_settings->setValue("wnel/binary_path", path);
    m_settings->sync();
}
