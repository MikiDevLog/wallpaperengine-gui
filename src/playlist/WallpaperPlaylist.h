#ifndef WALLPAPERPLAYLIST_H
#define WALLPAPERPLAYLIST_H

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QStringList>
#include "../core/WallpaperManager.h"
#include "../ui/PropertiesPanel.h" // For WallpaperSettings

// Forward declaration to avoid circular dependency
class WNELAddon;

enum class PlaybackOrder {
    Cycle,
    Random
};

struct PlaylistSettings {
    PlaybackOrder order = PlaybackOrder::Cycle;
    int delaySeconds = 300; // 5 minutes default
    bool enabled = false;
};

struct PlaylistItem {
    QString wallpaperId;
    int position = 0;
    QDateTime addedTime = QDateTime::currentDateTime();
};

class WallpaperPlaylist : public QObject
{
    Q_OBJECT

public:
    explicit WallpaperPlaylist(QObject* parent = nullptr);
    ~WallpaperPlaylist();

    // Playlist management
    void addWallpaper(const QString& wallpaperId);
    void removeWallpaper(const QString& wallpaperId);
    bool containsWallpaper(const QString& wallpaperId) const;
    void moveWallpaper(int fromIndex, int toIndex);
    void clearPlaylist();

    // Getters
    QList<PlaylistItem> getPlaylistItems() const;
    QStringList getWallpaperIds() const;
    int getWallpaperPosition(const QString& wallpaperId) const;
    int size() const;
    bool isEmpty() const;

    // Settings
    PlaylistSettings getSettings() const;
    void setSettings(const PlaylistSettings& settings);
    void setPlaybackOrder(PlaybackOrder order);
    void setDelaySeconds(int seconds);
    void setEnabled(bool enabled);

    // Playback control
    void startPlayback();
    void stopPlayback();
    void nextWallpaper();
    void previousWallpaper();
    QString getCurrentWallpaperId() const;
    int getCurrentIndex() const;
    bool isRunning() const;  // Check if playback is active

    // Individual wallpaper settings helper
    QStringList loadWallpaperSettings(const QString& wallpaperId) const;

    // WallpaperManager connection
    void setWallpaperManager(WallpaperManager* manager);
    
    // WNELAddon connection
    void setWNELAddon(WNELAddon* addon);

    // Persistence
    void saveToConfig();
    void loadFromConfig();
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

signals:
    void wallpaperAdded(const QString& wallpaperId);
    void wallpaperRemoved(const QString& wallpaperId);
    void wallpaperMoved(int fromIndex, int toIndex);
    void playlistCleared();
    void currentWallpaperChanged(const QString& wallpaperId);
    void playbackStarted();
    void playbackStopped();
    void settingsChanged();
    void playlistLaunchRequested(const QString& wallpaperId, const QStringList& args);

private slots:
    void onTimerTimeout();

private:
    void updatePositions();
    QString getNextWallpaper();
    QString getRandomWallpaper();
    void resetRandomHistory();

    QList<PlaylistItem> m_items;
    PlaylistSettings m_settings;
    QTimer* m_playbackTimer;
    
    // Playback state
    int m_currentIndex;
    QString m_currentWallpaperId;
    QStringList m_randomHistory; // For random playback without repeats
    
    WallpaperManager* m_wallpaperManager;
    WNELAddon* m_wnelAddon;
};

#endif // WALLPAPERPLAYLIST_H
