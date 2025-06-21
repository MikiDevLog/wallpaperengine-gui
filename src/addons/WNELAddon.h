#ifndef WNELADDON_H
#define WNELADDON_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QDir>
#include <QPixmap>
#include "../core/WallpaperManager.h"

// Extend WallpaperInfo to support external wallpapers
struct ExternalWallpaperInfo {
    QString id;
    QString name;
    QString originalPath;
    QString symlinkPath;
    QString previewPath;
    QString projectPath;
    QString type; // "image", "gif", "video"
    QString codec; // for videos
    QSize resolution;
    qint64 fileSize = 0;
    QDateTime created;
    QDateTime updated;
    
    // Convert to WallpaperInfo for compatibility
    WallpaperInfo toWallpaperInfo() const;
};

class WNELAddon : public QObject
{
    Q_OBJECT

public:
    explicit WNELAddon(QObject* parent = nullptr);
    ~WNELAddon();

    // Addon management
    bool isEnabled() const;
    void setEnabled(bool enabled);
    
    // External wallpapers management
    QString addExternalWallpaper(const QString& mediaPath, const QString& customName = QString());
    bool removeExternalWallpaper(const QString& wallpaperId);
    QList<ExternalWallpaperInfo> getAllExternalWallpapers() const;
    ExternalWallpaperInfo getExternalWallpaperById(const QString& id) const;
    bool hasExternalWallpaper(const QString& id) const;
    
    // Wallpaper launching
    bool launchExternalWallpaper(const QString& wallpaperId, const QStringList& additionalArgs = QStringList());
    void stopWallpaper();
    bool isWallpaperRunning() const;
    QString getCurrentWallpaper() const;
    
    // Preview generation
    bool generatePreviewFromVideo(const QString& videoPath, const QString& outputPath, const QSize& size = QSize(900, 900));
    bool generatePreviewFromImage(const QString& imagePath, const QString& outputPath, const QSize& size = QSize(900, 900));
    
    // Path management
    QString getExternalWallpapersPath() const;
    void setExternalWallpapersPath(const QString& path);
    bool ensureExternalWallpapersDirectory();
    
    // Utility methods
    QString generateUniqueId() const;
    QString detectMediaType(const QString& filePath) const;
    QString detectVideoCodec(const QString& videoPath) const;
    QSize getMediaResolution(const QString& filePath) const;
    
    // Project.json management
    bool createProjectJson(const ExternalWallpaperInfo& info) const;
    ExternalWallpaperInfo parseProjectJson(const QString& projectPath) const;

signals:
    void externalWallpaperAdded(const QString& wallpaperId);
    void externalWallpaperRemoved(const QString& wallpaperId);
    void wallpaperLaunched(const QString& wallpaperId);
    void wallpaperStopped();
    void errorOccurred(const QString& error);
    void outputReceived(const QString& output);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessOutput();

private:
    // Helper methods
    bool createSymlink(const QString& target, const QString& linkPath);
    QString generateProjectJsonContent(const ExternalWallpaperInfo& info) const;
    void refreshExternalWallpapers();
    
    // Member variables
    QProcess* m_wallpaperProcess;
    QString m_currentWallpaperId;
    QString m_externalWallpapersPath;
    bool m_enabled;
    QList<ExternalWallpaperInfo> m_externalWallpapers;
    QFileSystemWatcher* m_fileWatcher;
};

#endif // WNELADDON_H
