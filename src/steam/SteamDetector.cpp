#include "SteamDetector.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

SteamDetector::SteamDetector(QObject* parent)
    : QObject(parent)
{
}

QString SteamDetector::findSteamPath()
{
    QStringList paths = getCommonSteamPaths();
    
    for (const QString& path : paths) {
        if (QDir(path).exists()) {
            return path;
        }
    }
    
    return QString();
}

QStringList SteamDetector::findSteamLibraryPaths()
{
    QString steamPath = findSteamPath();
    if (steamPath.isEmpty()) {
        return QStringList();
    }
    
    QStringList libraryPaths;
    libraryPaths.append(steamPath);
    libraryPaths.append(parseLibraryFolders(steamPath));
    
    return libraryPaths;
}

bool SteamDetector::isSteamInstalled()
{
    return !findSteamPath().isEmpty();
}

QStringList SteamDetector::getCommonSteamPaths()
{
    QStringList paths;
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    
    // Common Steam installation paths on Linux
    paths << home + "/.steam/steam"
          << home + "/.local/share/Steam"
          << home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam"
          << "/usr/share/steam"
          << "/opt/steam";
    
    return paths;
}

QStringList SteamDetector::parseLibraryFolders(const QString& steamPath)
{
    // This would parse steamapps/libraryfolders.vdf
    // For now, return empty list - implement if needed
    Q_UNUSED(steamPath)
    return QStringList();
}

// Implement the new method to detect Steam installations
QList<SteamInstallation> SteamDetector::detectSteamInstallations()
{
    QList<SteamInstallation> installations;
    
    // Common Steam installation paths on Linux
    QStringList possiblePaths = {
        QDir::homePath() + "/.steam/steam",
        QDir::homePath() + "/.steam/root",
        QDir::homePath() + "/.local/share/Steam",
        "/opt/steam",
        "/usr/local/games/steam",
        "/usr/games/steam"
    };
    
    // Add Steam path from config if available
    QString steamPathFromConfig = findSteamPath();
    if (!steamPathFromConfig.isEmpty() && !possiblePaths.contains(steamPathFromConfig)) {
        possiblePaths.prepend(steamPathFromConfig);
    }
    
    // Check each path
    for (const QString& path : possiblePaths) {
        if (validateSteamPath(path)) {
            SteamInstallation installation;
            installation.path = path;
            installation.valid = true;
            
            // Try to determine version (basic implementation)
            QString versionFilePath = path + "/steam_client.txt";
            QFile versionFile(versionFilePath);
            if (versionFile.exists() && versionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                installation.version = versionFile.readLine().trimmed();
                versionFile.close();
            } else {
                installation.version = "Unknown";
            }
            
            installations.append(installation);
        }
    }
    
    return installations;
}

// Implementation of the method to get Wallpaper Engine assets path
QString SteamDetector::getWallpaperEngineAssetsPath(const QString& libraryPath)
{
    // Check for standard assets path
    QString assetsPath = QDir(libraryPath).filePath("steamapps/common/wallpaper_engine/assets");
    if (QDir(assetsPath).exists()) {
        return assetsPath;
    }
    
    // Check for alternative location
    assetsPath = QDir(libraryPath).filePath("steamapps/common/wallpaper_engine/bin/assets");
    if (QDir(assetsPath).exists()) {
        return assetsPath;
    }
    
    return QString();
}

// Helper method to validate a Steam path
bool SteamDetector::validateSteamPath(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return false;
    }
    
    // Check for typical Steam directories/files that would indicate this is a valid Steam installation
    return dir.exists("steamapps") || 
           dir.exists("steam.sh") || 
           dir.exists("ubuntu12_32") || 
           dir.exists("steam");
}
