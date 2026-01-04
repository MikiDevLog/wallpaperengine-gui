#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QDateTime>

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    static ConfigManager& instance();
    
    // Configuration file management
    QString configDir() const;
    void resetToDefaults();
    
    // Steam paths
    QString steamPath() const;
    void setSteamPath(const QString& path);
    QStringList steamLibraryPaths() const;
    void setSteamLibraryPaths(const QStringList& paths);
    
    // Wallpaper Engine binary
    QString wallpaperEnginePath() const;
    void setWallpaperEnginePath(const QString& path);
    QString getWallpaperEnginePath() const;
    
    // Assets directory
    QString getAssetsDir() const;
    void setAssetsDir(const QString& dir);
    QStringList findPossibleAssetsPaths() const;
    
    // Audio settings
    int masterVolume() const;
    void setMasterVolume(int volume);
    QString audioDevice() const;
    void setAudioDevice(const QString& device);
    bool muteOnFocus() const;
    void setMuteOnFocus(bool mute);
    bool muteOnFullscreen() const;
    void setMuteOnFullscreen(bool mute);
    bool noAutoMute() const;
    void setNoAutoMute(bool noAutoMute);
    bool noAudioProcessing() const;
    void setNoAudioProcessing(bool noProcessing);
    
    // Theme settings
    QString theme() const;
    void setTheme(const QString& theme);
    
    // Performance settings
    int targetFps() const;
    void setTargetFps(int fps);
    bool cpuLimitEnabled() const;
    void setCpuLimitEnabled(bool enabled);
    int cpuLimit() const;
    void setCpuLimit(int limit);
    
    // Behavior settings
    bool pauseOnFocus() const;
    void setPauseOnFocus(bool pause);
    bool pauseOnFullscreen() const;
    void setPauseOnFullscreen(bool pause);
    bool disableMouse() const;
    void setDisableMouse(bool disable);
    bool disableParallax() const;
    void setDisableParallax(bool disable);
    
    // Rendering settings
    QString renderMode() const;
    void setRenderMode(const QString& mode);
    QString msaaLevel() const;
    void setMsaaLevel(const QString& level);
    int anisotropicFiltering() const;
    void setAnisotropicFiltering(int level);
    bool vsyncEnabled() const;
    void setVsyncEnabled(bool enabled);
    bool bloomEnabled() const;
    void setBloomEnabled(bool enabled);
    bool reflectionsEnabled() const;
    void setReflectionsEnabled(bool enabled);
    
    // Advanced settings
    QString windowMode() const;
    void setWindowMode(const QString& mode);
    QString screenRoot() const;
    void setScreenRoot(const QString& root);
    QString clampingMode() const;
    void setClampingMode(const QString& mode);
    QString getScaling() const;
    void setScaling(const QString& scaling);
    bool getSilent() const;
    void setSilent(bool silent);
    
    // Theme settings
    QString qtTheme() const;
    void setQtTheme(const QString& theme);
    QStringList availableQtThemes() const;
    
    // Window state
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);
    QByteArray windowState() const;
    void setWindowState(const QByteArray& state);
    QByteArray getSplitterState() const;
    void setSplitterState(const QByteArray& state);
    
    // Application state
    bool isFirstRun() const;
    void setFirstRun(bool firstRun);
    
    // Configuration validation
    bool isConfigurationValid() const;
    bool hasValidPaths() const;
    QString getConfigurationIssues() const;
    
    QString lastSelectedWallpaper() const;
    void setLastSelectedWallpaper(const QString& wallpaperId);
    
    // Playlist state tracking
    bool lastSessionUsedPlaylist() const;
    void setLastSessionUsedPlaylist(bool usedPlaylist);
    
    int refreshInterval() const;
    void setRefreshInterval(int seconds);
    
    // System tray settings
    bool showTrayWarning() const;
    void setShowTrayWarning(bool show);

    // Steam API settings
    QString steamApiKey() const;
    void setSteamApiKey(const QString& apiKey);
    bool useSteamApi() const;
    void setUseSteamApi(bool use);
    QDateTime lastApiUpdate() const;
    void setLastApiUpdate(const QDateTime& dateTime);
    
    // WNEL Addon settings
    bool isWNELAddonEnabled() const;
    void setWNELAddonEnabled(bool enabled);
    QString externalWallpapersPath() const;
    void setExternalWallpapersPath(const QString& path);
    QString wnelBinaryPath() const;
    void setWNELBinaryPath(const QString& path);
    
    // Global Engine Defaults (system-wide defaults for all wallpapers)
    // Audio settings
    bool globalSilent() const;
    void setGlobalSilent(bool silent);
    int globalVolume() const;
    void setGlobalVolume(int volume);
    bool globalNoAutoMute() const;
    void setGlobalNoAutoMute(bool noAutoMute);
    bool globalNoAudioProcessing() const;
    void setGlobalNoAudioProcessing(bool noProcessing);
    
    // Performance settings
    int globalFps() const;
    void setGlobalFps(int fps);
    
    // Display settings
    QString globalWindowGeometry() const;
    void setGlobalWindowGeometry(const QString& geometry);
    QString globalScreenRoot() const;
    void setGlobalScreenRoot(const QString& root);
    QString globalBackgroundId() const;
    void setGlobalBackgroundId(const QString& id);
    QString globalScaling() const;
    void setGlobalScaling(const QString& scaling);
    QString globalClamping() const;
    void setGlobalClamping(const QString& clamping);
    
    // Behavior settings
    bool globalDisableMouse() const;
    void setGlobalDisableMouse(bool disable);
    bool globalDisableParallax() const;
    void setGlobalDisableParallax(bool disable);
    bool globalNoFullscreenPause() const;
    void setGlobalNoFullscreenPause(bool noPause);
    
    // WNEL-specific settings
    bool globalNoLoop() const;
    void setGlobalNoLoop(bool noLoop);
    bool globalNoHardwareDecode() const;
    void setGlobalNoHardwareDecode(bool noHardwareDecode);
    bool globalForceX11() const;
    void setGlobalForceX11(bool forceX11);
    bool globalForceWayland() const;
    void setGlobalForceWayland(bool forceWayland);
    bool globalVerbose() const;
    void setGlobalVerbose(bool verbose);
    QString globalLogLevel() const;
    void setGlobalLogLevel(const QString& level);
    QString globalMpvOptions() const;
    void setGlobalMpvOptions(const QString& options);
    
    // Multi-Monitor Mode settings
    bool multiMonitorModeEnabled() const;
    void setMultiMonitorModeEnabled(bool enabled);
    QStringList multiMonitorScreenOrder() const;
    void setMultiMonitorScreenOrder(const QStringList& order);
    QMap<QString, QString> multiMonitorScreenNames() const;
    void setMultiMonitorScreenNames(const QMap<QString, QString>& names);
    QMap<QString, QString> multiMonitorScreenAssignments() const;
    void setMultiMonitorScreenAssignments(const QMap<QString, QString>& assignments);
    
    // Generic settings access for custom configuration values
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void setValue(const QString& key, const QVariant& value);

private:
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager() = default;
    
    // Prevent copying
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    QSettings* m_settings;
};

#endif // CONFIGMANAGER_H
