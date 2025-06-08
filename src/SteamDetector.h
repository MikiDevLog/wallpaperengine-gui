#ifndef STEAMDETECTOR_H
#define STEAMDETECTOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>

// Define a structure for Steam installations
struct SteamInstallation {
    QString path;
    QString version;
    bool valid = false;
};

class SteamDetector : public QObject
{
    Q_OBJECT

public:
    explicit SteamDetector(QObject* parent = nullptr);
    
    // Existing methods
    QString findSteamPath();
    QStringList findSteamLibraryPaths();
    
    // Add missing method declaration
    bool isSteamInstalled();
    
    // New methods to match calls in other files
    QList<SteamInstallation> detectSteamInstallations();
    QString getWallpaperEngineAssetsPath(const QString& libraryPath);
    
private:
    bool validateSteamPath(const QString& path);
    
    // Add missing private method declarations
    QStringList getCommonSteamPaths();
    QStringList parseLibraryFolders(const QString& steamPath);
};

#endif // STEAMDETECTOR_H
