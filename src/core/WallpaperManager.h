#ifndef WALLPAPERMANAGER_H
#define WALLPAPERMANAGER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <optional>

struct WallpaperInfo {
    QString id;
    QString name;
    QString author;
    QString authorId;
    QString description;
    QString type;
    QString path;
    QString previewPath;
    QString projectPath;
    QDateTime created;
    QDateTime updated;
    qint64 fileSize = 0;
    QStringList tags;
    QJsonObject properties;  // Properties from project.json
    
    WallpaperInfo() = default;
    
    bool operator==(const WallpaperInfo& other) const {
        return id == other.id;
    }
};

class WallpaperManager : public QObject
{
    Q_OBJECT

public:
    explicit WallpaperManager(QObject* parent = nullptr);
    ~WallpaperManager();

    void refreshWallpapers();
    QList<WallpaperInfo> getAllWallpapers() const;
    WallpaperInfo getWallpaperById(const QString& id) const;
    std::optional<WallpaperInfo> getWallpaperInfo(const QString& id) const;

    bool launchWallpaper(const QString& wallpaperId, const QStringList& additionalArgs = QStringList());
    void stopWallpaper();
    bool isWallpaperRunning() const;
    QString getCurrentWallpaper() const;
    
    // Multi-monitor mode
    bool launchMultiMonitorWallpaper(const QMap<QString, QString>& screenAssignments);

signals:
    void refreshProgress(int current, int total);
    void refreshFinished();
    void wallpapersChanged();
    void outputReceived(const QString& output);
    void errorOccurred(const QString& error);
    void wallpaperLaunched(const QString& wallpaperId);
    void wallpaperStopped();

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessOutput();

private:
    void scanWorkshopDirectories();
    void processWallpaperDirectory(const QString& dirPath);
    WallpaperInfo parseProjectJson(const QString& projectPath);
    QJsonObject extractProperties(const QJsonObject& projectJson);
    QString findPreviewImage(const QString& wallpaperDir);
    QString extractWorkshopId(const QString& dirPath);
    QStringList generatePropertyArguments(const QString& projectJsonPath);
    bool verifyProcessTerminated(qint64 pid);  // Helper to verify process is really dead
    
    QList<WallpaperInfo> m_wallpapers;
    QProcess* m_wallpaperProcess;
    QString m_currentWallpaperId;
    bool m_refreshing;
};

#endif // WALLPAPERMANAGER_H
