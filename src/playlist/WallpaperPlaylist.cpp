#include "WallpaperPlaylist.h"
#include "../core/ConfigManager.h"
#include "../addons/WNELAddon.h"  // Add WNELAddon include
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QDebug>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QSettings>
#include <QDir>
#include <QFile>

Q_LOGGING_CATEGORY(wallpaperPlaylist, "wallpaperPlaylist")

WallpaperPlaylist::WallpaperPlaylist(QObject* parent)
    : QObject(parent)
    , m_playbackTimer(new QTimer(this))
    , m_currentIndex(-1)
    , m_wallpaperManager(nullptr)
    , m_wnelAddon(nullptr)
{
    connect(m_playbackTimer, &QTimer::timeout, this, &WallpaperPlaylist::onTimerTimeout);
    m_playbackTimer->setSingleShot(false);
    
    // Get WallpaperManager instance if available
    m_wallpaperManager = qobject_cast<WallpaperManager*>(parent);
    if (!m_wallpaperManager) {
        // Try to find it in parent hierarchy
        QObject* p = parent;
        while (p && !m_wallpaperManager) {
            m_wallpaperManager = p->findChild<WallpaperManager*>();
            p = p->parent();
        }
    }
}

WallpaperPlaylist::~WallpaperPlaylist()
{
    stopPlayback();
}

void WallpaperPlaylist::addWallpaper(const QString& wallpaperId)
{
    if (wallpaperId.isEmpty() || containsWallpaper(wallpaperId)) {
        return;
    }

    PlaylistItem item;
    item.wallpaperId = wallpaperId;
    item.position = m_items.size();
    item.addedTime = QDateTime::currentDateTime();

    m_items.append(item);
    updatePositions();

    emit wallpaperAdded(wallpaperId);
    saveToConfig();
}

void WallpaperPlaylist::removeWallpaper(const QString& wallpaperId)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].wallpaperId == wallpaperId) {
            m_items.removeAt(i);
            updatePositions();
            
            // Update current index if needed
            if (m_currentIndex >= i && m_currentIndex > 0) {
                m_currentIndex--;
            } else if (m_currentIndex == i) {
                m_currentIndex = -1;
                m_currentWallpaperId.clear();
            }
            
            emit wallpaperRemoved(wallpaperId);
            saveToConfig();
            return;
        }
    }
}

bool WallpaperPlaylist::containsWallpaper(const QString& wallpaperId) const
{
    for (const auto& item : m_items) {
        if (item.wallpaperId == wallpaperId) {
            return true;
        }
    }
    return false;
}

void WallpaperPlaylist::moveWallpaper(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_items.size() ||
        toIndex < 0 || toIndex >= m_items.size() ||
        fromIndex == toIndex) {
        return;
    }

    m_items.move(fromIndex, toIndex);
    updatePositions();
    
    // Update current index if it was affected
    if (m_currentIndex == fromIndex) {
        m_currentIndex = toIndex;
    } else if (fromIndex < m_currentIndex && toIndex >= m_currentIndex) {
        m_currentIndex--;
    } else if (fromIndex > m_currentIndex && toIndex <= m_currentIndex) {
        m_currentIndex++;
    }

    emit wallpaperMoved(fromIndex, toIndex);
    saveToConfig();
}

void WallpaperPlaylist::clearPlaylist()
{
    m_items.clear();
    m_currentIndex = -1;
    m_currentWallpaperId.clear();
    m_randomHistory.clear();
    
    emit playlistCleared();
    saveToConfig();
}

QList<PlaylistItem> WallpaperPlaylist::getPlaylistItems() const
{
    return m_items;
}

QStringList WallpaperPlaylist::getWallpaperIds() const
{
    QStringList ids;
    for (const auto& item : m_items) {
        ids.append(item.wallpaperId);
    }
    return ids;
}

int WallpaperPlaylist::getWallpaperPosition(const QString& wallpaperId) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].wallpaperId == wallpaperId) {
            return i;
        }
    }
    return -1;
}

int WallpaperPlaylist::size() const
{
    return m_items.size();
}

bool WallpaperPlaylist::isEmpty() const
{
    return m_items.isEmpty();
}

PlaylistSettings WallpaperPlaylist::getSettings() const
{
    return m_settings;
}

void WallpaperPlaylist::setSettings(const PlaylistSettings& settings)
{
    m_settings = settings;
    
    // Update timer interval if playback is active
    if (m_playbackTimer->isActive()) {
        m_playbackTimer->setInterval(m_settings.delaySeconds * 1000);
    }
    
    // Stop/start playback based on enabled state
    if (m_settings.enabled && !m_playbackTimer->isActive() && !isEmpty()) {
        startPlayback();
    } else if (!m_settings.enabled && m_playbackTimer->isActive()) {
        stopPlayback();
    }
    
    emit settingsChanged();
    saveToConfig();
}

void WallpaperPlaylist::setPlaybackOrder(PlaybackOrder order)
{
    m_settings.order = order;
    if (order == PlaybackOrder::Random) {
        resetRandomHistory();
    }
    emit settingsChanged();
    saveToConfig();
}

void WallpaperPlaylist::setDelaySeconds(int seconds)
{
    m_settings.delaySeconds = qMax(1, seconds); // Minimum 1 second
    if (m_playbackTimer->isActive()) {
        m_playbackTimer->setInterval(m_settings.delaySeconds * 1000);
    }
    emit settingsChanged();
    saveToConfig();
}

void WallpaperPlaylist::setEnabled(bool enabled)
{
    m_settings.enabled = enabled;
    
    if (enabled && !isEmpty()) {
        startPlayback();
    } else {
        stopPlayback();
    }
    
    emit settingsChanged();
    saveToConfig();
}

void WallpaperPlaylist::startPlayback()
{
    qCDebug(wallpaperPlaylist) << "WallpaperPlaylist::startPlayback() called";
    qCDebug(wallpaperPlaylist) << "  - isEmpty():" << isEmpty() << "m_settings.enabled:" << m_settings.enabled;
    qCDebug(wallpaperPlaylist) << "  - Playlist size:" << m_items.size();
    
    if (isEmpty() || !m_settings.enabled) {
        qCDebug(wallpaperPlaylist) << "  - Returning early: playlist empty or not enabled";
        return;
    }

    if (m_currentIndex < 0 || m_currentIndex >= m_items.size()) {
        m_currentIndex = 0;
        qCDebug(wallpaperPlaylist) << "  - Reset current index to 0";
    }

    qCDebug(wallpaperPlaylist) << "  - Current index:" << m_currentIndex;
    qCDebug(wallpaperPlaylist) << "  - Delay seconds:" << m_settings.delaySeconds;

    m_playbackTimer->setInterval(m_settings.delaySeconds * 1000);
    m_playbackTimer->start();
    
    // Start with current wallpaper
    if (m_currentIndex < m_items.size()) {
        m_currentWallpaperId = m_items[m_currentIndex].wallpaperId;
        qCDebug(wallpaperPlaylist) << "  - Setting current wallpaper ID:" << m_currentWallpaperId;
        emit currentWallpaperChanged(m_currentWallpaperId);
        
        // Launch wallpaper if WallpaperManager is available
        if (m_wallpaperManager) {
            qCDebug(wallpaperPlaylist) << "  - WallpaperManager available, launching wallpaper via signal";
            QStringList args = loadWallpaperSettings(m_currentWallpaperId);
            qCDebug(wallpaperPlaylist) << "  - Emitting playlistLaunchRequested with ID:" << m_currentWallpaperId << "args:" << args;
            emit playlistLaunchRequested(m_currentWallpaperId, args);
        } else {
            qCDebug(wallpaperPlaylist) << "  - WallpaperManager NOT available!";
        }
    }
    
    qCDebug(wallpaperPlaylist) << "  - Emitting playbackStarted signal";
    emit playbackStarted();
}

void WallpaperPlaylist::stopPlayback()
{
    m_playbackTimer->stop();
    emit playbackStopped();
}

void WallpaperPlaylist::nextWallpaper()
{
    if (isEmpty()) {
        return;
    }

    QString nextWallpaperId = getNextWallpaper();
    if (!nextWallpaperId.isEmpty()) {
        m_currentWallpaperId = nextWallpaperId;
        m_currentIndex = getWallpaperPosition(m_currentWallpaperId);
        emit currentWallpaperChanged(m_currentWallpaperId);
        
        if (m_wallpaperManager) {
            QStringList args = loadWallpaperSettings(m_currentWallpaperId);
            emit playlistLaunchRequested(m_currentWallpaperId, args);
        }
    }
}

void WallpaperPlaylist::previousWallpaper()
{
    if (isEmpty()) {
        return;
    }

    if (m_settings.order == PlaybackOrder::Cycle) {
        m_currentIndex = (m_currentIndex - 1 + m_items.size()) % m_items.size();
    } else {
        // For random, just pick a random wallpaper
        m_currentIndex = QRandomGenerator::global()->bounded(m_items.size());
    }

    if (m_currentIndex < m_items.size()) {
        m_currentWallpaperId = m_items[m_currentIndex].wallpaperId;
        emit currentWallpaperChanged(m_currentWallpaperId);
        
        if (m_wallpaperManager) {
            QStringList args = loadWallpaperSettings(m_currentWallpaperId);
            emit playlistLaunchRequested(m_currentWallpaperId, args);
        }
    }
}

QString WallpaperPlaylist::getCurrentWallpaperId() const
{
    return m_currentWallpaperId;
}

int WallpaperPlaylist::getCurrentIndex() const
{
    return m_currentIndex;
}

bool WallpaperPlaylist::isRunning() const
{
    return m_playbackTimer && m_playbackTimer->isActive();
}

QStringList WallpaperPlaylist::loadWallpaperSettings(const QString& wallpaperId) const
{
    QStringList args;
    
    // Build settings file path (same logic as in MainWindow::onWallpaperLaunched)
    QString settingsPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (settingsPath.isEmpty()) {
        settingsPath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    }
    if (settingsPath.isEmpty()) {
        settingsPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + 
                      "/.cache/wallpaperengine-gui";
    } else {
        settingsPath += "/wallpaperengine-gui";
    }
    settingsPath += "/settings/" + wallpaperId + ".json";
    
    // Check if settings file exists
    QFile settingsFile(settingsPath);
    if (settingsFile.exists() && settingsFile.open(QIODevice::ReadOnly)) {
        // Parse settings
        QJsonDocument doc = QJsonDocument::fromJson(settingsFile.readAll());
        settingsFile.close();
        
        if (doc.isObject()) {
            QJsonObject settings = doc.object();
            
            // Add command line args based on settings (same logic as MainWindow)
            if (settings["silent"].toBool()) args << "--silent";
            
            int volume = settings["volume"].toInt(15);
            if (volume != 15) args << "--volume" << QString::number(volume);
            
            if (settings["noAutoMute"].toBool()) args << "--noautomute";
            if (settings["noAudioProcessing"].toBool()) args << "--no-audio-processing";
            
            int fps = settings["fps"].toInt(30);
            if (fps != 30) args << "--fps" << QString::number(fps);
            
            QString windowGeometry = settings["windowGeometry"].toString();
            if (!windowGeometry.isEmpty()) args << "--window" << windowGeometry;
            
            QString screenRoot = settings["screenRoot"].toString();
            if (!screenRoot.isEmpty()) {
                args << "--screen-root" << screenRoot;
                
                QString backgroundId = settings["backgroundId"].toString();
                if (!backgroundId.isEmpty()) args << "--bg" << backgroundId;
            }
            
            QString scaling = settings["scaling"].toString();
            if (scaling != "default") args << "--scaling" << scaling;
            
            QString clamping = settings["clamping"].toString();
            if (clamping != "clamp") args << "--clamping" << clamping;
            
            if (settings["disableMouse"].toBool()) args << "--disable-mouse";
            if (settings["disableParallax"].toBool()) args << "--disable-parallax";
            if (settings["noFullscreenPause"].toBool()) args << "--no-fullscreen-pause";
        }
    } else {
        // No settings file exists, apply default values (same as MainWindow)
        args << "--volume" << "15";
        args << "--fps" << "30";
        args << "--screen-root" << "HDMI-A-1";
    }
    
    return args;
}

void WallpaperPlaylist::saveToConfig()
{
    // Save playlist data to QSettings using a similar pattern as other ConfigManager methods
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/wallpaperengine-gui";
    QDir().mkpath(configPath);
    QSettings settings(configPath + "/config.ini", QSettings::IniFormat);
    
    // Convert QJsonObject to QVariant for QSettings storage
    QJsonDocument doc(toJson());
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    settings.setValue("playlist/data", data);
    settings.sync();
}

void WallpaperPlaylist::loadFromConfig()
{
    // Load playlist data from QSettings using a similar pattern as other ConfigManager methods
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/wallpaperengine-gui";
    QSettings settings(configPath + "/config.ini", QSettings::IniFormat);
    
    QByteArray data = settings.value("playlist/data", QByteArray()).toByteArray();
    if (!data.isEmpty()) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            fromJson(doc.object());
        }
    }
}

QJsonObject WallpaperPlaylist::toJson() const
{
    QJsonObject json;
    
    // Save settings
    QJsonObject settingsObj;
    settingsObj["order"] = static_cast<int>(m_settings.order);
    settingsObj["delaySeconds"] = m_settings.delaySeconds;
    settingsObj["enabled"] = m_settings.enabled;
    json["settings"] = settingsObj;
    
    // Save playlist items
    QJsonArray itemsArray;
    for (const auto& item : m_items) {
        QJsonObject itemObj;
        itemObj["wallpaperId"] = item.wallpaperId;
        itemObj["position"] = item.position;
        itemObj["addedTime"] = item.addedTime.toString(Qt::ISODate);
        itemsArray.append(itemObj);
    }
    json["items"] = itemsArray;
    
    // Save current state
    json["currentIndex"] = m_currentIndex;
    json["currentWallpaperId"] = m_currentWallpaperId;
    
    return json;
}

void WallpaperPlaylist::fromJson(const QJsonObject& json)
{
    // Load settings
    QJsonObject settingsObj = json["settings"].toObject();
    m_settings.order = static_cast<PlaybackOrder>(settingsObj["order"].toInt());
    m_settings.delaySeconds = settingsObj["delaySeconds"].toInt(300);
    m_settings.enabled = settingsObj["enabled"].toBool(false);
    
    // Load playlist items
    m_items.clear();
    QJsonArray itemsArray = json["items"].toArray();
    for (const auto& value : itemsArray) {
        QJsonObject itemObj = value.toObject();
        PlaylistItem item;
        item.wallpaperId = itemObj["wallpaperId"].toString();
        item.position = itemObj["position"].toInt();
        item.addedTime = QDateTime::fromString(itemObj["addedTime"].toString(), Qt::ISODate);
        if (item.addedTime.isNull()) {
            item.addedTime = QDateTime::currentDateTime();
        }
        m_items.append(item);
    }
    
    // Load current state
    m_currentIndex = json["currentIndex"].toInt(-1);
    m_currentWallpaperId = json["currentWallpaperId"].toString();
    
    // Validate current index
    if (m_currentIndex >= m_items.size()) {
        m_currentIndex = -1;
        m_currentWallpaperId.clear();
    }
}

void WallpaperPlaylist::onTimerTimeout()
{
    nextWallpaper();
}

void WallpaperPlaylist::updatePositions()
{
    for (int i = 0; i < m_items.size(); ++i) {
        m_items[i].position = i;
    }
}

QString WallpaperPlaylist::getNextWallpaper()
{
    if (isEmpty()) {
        return QString();
    }

    if (m_settings.order == PlaybackOrder::Cycle) {
        m_currentIndex = (m_currentIndex + 1) % m_items.size();
    } else {
        // Random order with history to avoid immediate repeats
        return getRandomWallpaper();
    }

    return m_items[m_currentIndex].wallpaperId;
}

QString WallpaperPlaylist::getRandomWallpaper()
{
    if (isEmpty()) {
        return QString();
    }

    // If we've played all wallpapers, reset history
    if (m_randomHistory.size() >= m_items.size()) {
        resetRandomHistory();
    }

    // Find wallpapers not in recent history
    QStringList availableWallpapers;
    for (const auto& item : m_items) {
        if (!m_randomHistory.contains(item.wallpaperId)) {
            availableWallpapers.append(item.wallpaperId);
        }
    }

    if (availableWallpapers.isEmpty()) {
        // Fallback to any wallpaper
        m_currentIndex = QRandomGenerator::global()->bounded(m_items.size());
    } else {
        // Pick a random wallpaper from available ones
        QString selectedId = availableWallpapers.at(
            QRandomGenerator::global()->bounded(availableWallpapers.size()));
        
        // Find its index
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i].wallpaperId == selectedId) {
                m_currentIndex = i;
                break;
            }
        }
    }

    QString wallpaperId = m_items[m_currentIndex].wallpaperId;
    m_randomHistory.append(wallpaperId);
    
    return wallpaperId;
}

void WallpaperPlaylist::resetRandomHistory()
{
    m_randomHistory.clear();
}

void WallpaperPlaylist::setWallpaperManager(WallpaperManager* manager)
{
    qCDebug(wallpaperPlaylist) << "WallpaperPlaylist::setWallpaperManager() - Setting manager:" << (manager ? "valid" : "null");
    m_wallpaperManager = manager;
}

void WallpaperPlaylist::setWNELAddon(WNELAddon* addon)
{
    qCDebug(wallpaperPlaylist) << "WallpaperPlaylist::setWNELAddon() - Setting addon:" << (addon ? "valid" : "null");
    m_wnelAddon = addon;
}
