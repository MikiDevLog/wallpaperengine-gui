#include "WNELAddon.h"
#include "../core/ConfigManager.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QLoggingCategory>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUuid>
#include <QDateTime>
#include <QPixmap>
#include <QPainter>
#include <QImageReader>
#include <QMessageBox>
#include <QApplication>
#include <QRandomGenerator>

Q_LOGGING_CATEGORY(wnelAddon, "app.wnelAddon")

WallpaperInfo ExternalWallpaperInfo::toWallpaperInfo() const
{
    WallpaperInfo info;
    info.id = id;
    info.name = name;
    info.author = "Custom"; // External wallpapers don't have authors
    info.authorId = "";
    info.description = QString("External %1 wallpaper").arg(type);
    info.type = "External"; // Special type for external wallpapers
    info.path = symlinkPath;
    info.previewPath = previewPath;
    info.projectPath = projectPath;
    info.created = created;
    info.updated = updated;
    info.fileSize = fileSize;
    info.tags << "external" << type;
    
    // Add external flag to properties
    QJsonObject properties;
    properties["external"] = true;
    properties["originalPath"] = originalPath;
    properties["mediaType"] = type;
    if (!codec.isEmpty()) {
        properties["codec"] = codec;
    }
    properties["resolution"] = QString("%1x%2").arg(resolution.width()).arg(resolution.height());
    info.properties = properties;
    
    return info;
}

WNELAddon::WNELAddon(QObject* parent)
    : QObject(parent)
    , m_wallpaperProcess(nullptr)
    , m_enabled(false)
    , m_fileWatcher(new QFileSystemWatcher(this))
{
    ConfigManager& config = ConfigManager::instance();
    m_enabled = config.isWNELAddonEnabled();
    m_externalWallpapersPath = config.externalWallpapersPath();
    
    if (m_enabled) {
        ensureExternalWallpapersDirectory();
        refreshExternalWallpapers();
    }
}

WNELAddon::~WNELAddon()
{
    stopWallpaper();
}

bool WNELAddon::isEnabled() const
{
    return m_enabled;
}

void WNELAddon::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    
    m_enabled = enabled;
    ConfigManager::instance().setWNELAddonEnabled(enabled);
    
    if (enabled) {
        ensureExternalWallpapersDirectory();
        refreshExternalWallpapers();
    } else {
        stopWallpaper();
        m_externalWallpapers.clear();
    }
}

QString WNELAddon::getExternalWallpapersPath() const
{
    return m_externalWallpapersPath;
}

void WNELAddon::setExternalWallpapersPath(const QString& path)
{
    if (m_externalWallpapersPath == path) return;
    
    m_externalWallpapersPath = path;
    ConfigManager::instance().setExternalWallpapersPath(path);
    
    if (m_enabled) {
        ensureExternalWallpapersDirectory();
        refreshExternalWallpapers();
    }
}

bool WNELAddon::ensureExternalWallpapersDirectory()
{
    QDir dir;
    if (!dir.exists(m_externalWallpapersPath)) {
        if (!dir.mkpath(m_externalWallpapersPath)) {
            qCWarning(wnelAddon) << "Failed to create external wallpapers directory:" << m_externalWallpapersPath;
            return false;
        }
        qCDebug(wnelAddon) << "Created external wallpapers directory:" << m_externalWallpapersPath;
    }
    return true;
}

QString WNELAddon::generateUniqueId() const
{
    // Generate short unique ID for external wallpapers
    // Use prefix "ext_" + 6 random alphanumeric characters
    const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    QString id = "ext_";
    
    for (int i = 0; i < 6; ++i) {
        int index = QRandomGenerator::global()->bounded(chars.length());
        id.append(chars.at(index));
    }
    
    // Check if this ID already exists, if so generate a new one
    ConfigManager& config = ConfigManager::instance();
    QString externalWallpapersDir = config.externalWallpapersPath();
    if (!externalWallpapersDir.isEmpty()) {
        QDir dir(externalWallpapersDir);
        while (dir.exists(id)) {
            // Regenerate if collision (very rare)
            id = "ext_";
            for (int i = 0; i < 6; ++i) {
                int index = QRandomGenerator::global()->bounded(chars.length());
                id.append(chars.at(index));
            }
        }
    }
    
    return id;
}

QString WNELAddon::detectMediaType(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    QStringList imageFormats = {"png", "jpg", "jpeg", "bmp", "tiff", "webp"};
    QStringList videoFormats = {"mp4", "avi", "mkv", "mov", "webm", "m4v"};
    QStringList gifFormats = {"gif"};
    
    if (imageFormats.contains(suffix)) {
        return "image";
    } else if (videoFormats.contains(suffix)) {
        return "video";
    } else if (gifFormats.contains(suffix)) {
        return "gif";
    }
    
    return "unknown";
}

QString WNELAddon::detectVideoCodec(const QString& videoPath) const
{
    // Use ffprobe to detect video codec
    QProcess ffprobe;
    QStringList args;
    args << "-v" << "quiet"
         << "-select_streams" << "v:0"
         << "-show_entries" << "stream=codec_name"
         << "-of" << "csv=p=0"
         << videoPath;
    
    ffprobe.start("ffprobe", args);
    if (ffprobe.waitForFinished(5000)) {
        QString codec = ffprobe.readAllStandardOutput().trimmed();
        if (!codec.isEmpty()) {
            return codec;
        }
    }
    
    return "unknown";
}

QSize WNELAddon::getMediaResolution(const QString& filePath) const
{
    QString mediaType = detectMediaType(filePath);
    
    if (mediaType == "image" || mediaType == "gif") {
        QImageReader reader(filePath);
        return reader.size();
    } else if (mediaType == "video") {
        // Use ffprobe to get video resolution
        QProcess ffprobe;
        QStringList args;
        args << "-v" << "quiet"
             << "-select_streams" << "v:0"
             << "-show_entries" << "stream=width,height"
             << "-of" << "csv=p=0"
             << filePath;
        
        ffprobe.start("ffprobe", args);
        if (ffprobe.waitForFinished(5000)) {
            QString output = ffprobe.readAllStandardOutput().trimmed();
            QStringList parts = output.split(',');
            if (parts.size() == 2) {
                bool widthOk, heightOk;
                int width = parts[0].toInt(&widthOk);
                int height = parts[1].toInt(&heightOk);
                if (widthOk && heightOk) {
                    return QSize(width, height);
                }
            }
        }
    }
    
    return QSize();
}

bool WNELAddon::generatePreviewFromVideo(const QString& videoPath, const QString& outputPath, const QSize& size)
{
    // Check if output already exists to avoid regenerating
    if (QFileInfo::exists(outputPath)) {
        qCDebug(wnelAddon) << "Preview already exists, skipping generation:" << outputPath;
        return true;
    }
    
    // Use ffmpeg to extract first frame and resize
    QProcess ffmpeg;
    ffmpeg.setWorkingDirectory(QDir::tempPath()); // Set working directory to avoid issues
    
    QStringList args;
    args << "-v" << "quiet"  // Reduce verbosity
         << "-i" << videoPath
         << "-vf" << QString("scale=%1:%2:force_original_aspect_ratio=decrease,pad=%1:%2:(ow-iw)/2:(oh-ih)/2")
                     .arg(size.width()).arg(size.height())
         << "-vframes" << "1"
         << "-y" << outputPath;
    
    qCDebug(wnelAddon) << "Generating preview with ffmpeg:" << args.join(" ");
    
    ffmpeg.start("ffmpeg", args);
    if (ffmpeg.waitForFinished(10000)) {
        bool success = ffmpeg.exitCode() == 0 && QFileInfo::exists(outputPath);
        if (!success) {
            qCWarning(wnelAddon) << "ffmpeg failed:" << ffmpeg.readAllStandardError();
        }
        return success;
    }
    
    qCWarning(wnelAddon) << "Failed to generate preview from video:" << videoPath;
    return false;
}

bool WNELAddon::generatePreviewFromImage(const QString& imagePath, const QString& outputPath, const QSize& size)
{
    QPixmap originalPixmap(imagePath);
    if (originalPixmap.isNull()) {
        qCWarning(wnelAddon) << "Failed to load image:" << imagePath;
        return false;
    }
    
    QPixmap scaledPixmap = originalPixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // Create output pixmap with exact target size and center the scaled image
    QPixmap outputPixmap(size);
    outputPixmap.fill(Qt::black);
    
    QPainter painter(&outputPixmap);
    int x = (size.width() - scaledPixmap.width()) / 2;
    int y = (size.height() - scaledPixmap.height()) / 2;
    painter.drawPixmap(x, y, scaledPixmap);
    painter.end();
    
    return outputPixmap.save(outputPath, "PNG");
}

QString WNELAddon::addExternalWallpaper(const QString& mediaPath, const QString& customName)
{
    if (!m_enabled) {
        qCWarning(wnelAddon) << "WNEL addon is not enabled";
        return QString();
    }
    
    if (!ensureExternalWallpapersDirectory()) {
        return QString();
    }
    
    QFileInfo mediaFileInfo(mediaPath);
    if (!mediaFileInfo.exists()) {
        qCWarning(wnelAddon) << "Media file does not exist:" << mediaPath;
        return QString();
    }
    
    // Generate unique ID and create directory
    QString wallpaperId = generateUniqueId();
    QString wallpaperDir = QDir(m_externalWallpapersPath).filePath(wallpaperId);
    
    QDir dir;
    if (!dir.mkpath(wallpaperDir)) {
        qCWarning(wnelAddon) << "Failed to create wallpaper directory:" << wallpaperDir;
        return QString();
    }
    
    // Create external wallpaper info
    ExternalWallpaperInfo info;
    info.id = wallpaperId;
    info.name = customName.isEmpty() ? mediaFileInfo.baseName() : customName;
    info.originalPath = mediaFileInfo.absoluteFilePath();
    info.type = detectMediaType(mediaPath);
    info.resolution = getMediaResolution(mediaPath);
    info.fileSize = mediaFileInfo.size();
    info.created = QDateTime::currentDateTime();
    info.updated = info.created;
    
    if (info.type == "video") {
        info.codec = detectVideoCodec(mediaPath);
    }
    
    // Create symlink to media file
    QString symlinkPath = QDir(wallpaperDir).filePath("media." + mediaFileInfo.suffix());
    if (!createSymlink(info.originalPath, symlinkPath)) {
        QDir(wallpaperDir).removeRecursively();
        return QString();
    }
    info.symlinkPath = symlinkPath;
    
    // Generate preview
    QString previewPath = QDir(wallpaperDir).filePath("preview.png");
    bool previewGenerated = false;
    
    if (info.type == "image") {
        info.previewPath = symlinkPath; // Use the image itself as preview
        previewGenerated = true;
    } else if (info.type == "gif") {
        info.previewPath = symlinkPath; // Use GIF as preview
        previewGenerated = true;
    } else if (info.type == "video") {
        previewGenerated = generatePreviewFromVideo(mediaPath, previewPath);
        if (previewGenerated) {
            info.previewPath = previewPath;
        }
    }
    
    if (!previewGenerated) {
        qCWarning(wnelAddon) << "Failed to generate preview for wallpaper:" << wallpaperId;
        // Continue anyway, just without preview
    }
    
    // Create project.json
    info.projectPath = QDir(wallpaperDir).filePath("project.json");
    if (!createProjectJson(info)) {
        qCWarning(wnelAddon) << "Failed to create project.json for wallpaper:" << wallpaperId;
        QDir(wallpaperDir).removeRecursively();
        return QString();
    }
    
    // Add to our list
    m_externalWallpapers.append(info);
    
    qCDebug(wnelAddon) << "Added external wallpaper:" << wallpaperId << "(" << info.name << ")";
    emit externalWallpaperAdded(wallpaperId);
    
    return wallpaperId;
}

bool WNELAddon::removeExternalWallpaper(const QString& wallpaperId)
{
    auto it = std::find_if(m_externalWallpapers.begin(), m_externalWallpapers.end(),
                          [&wallpaperId](const ExternalWallpaperInfo& info) {
                              return info.id == wallpaperId;
                          });
    
    if (it == m_externalWallpapers.end()) {
        return false;
    }
    
    // Stop wallpaper if it's currently running
    if (m_currentWallpaperId == wallpaperId) {
        stopWallpaper();
    }
    
    // Remove directory
    QString wallpaperDir = QDir(m_externalWallpapersPath).filePath(wallpaperId);
    QDir dir(wallpaperDir);
    if (dir.exists()) {
        if (!dir.removeRecursively()) {
            qCWarning(wnelAddon) << "Failed to remove wallpaper directory:" << wallpaperDir;
            return false;
        }
    }
    
    // Remove from list
    m_externalWallpapers.erase(it);
    
    qCDebug(wnelAddon) << "Removed external wallpaper:" << wallpaperId;
    emit externalWallpaperRemoved(wallpaperId);
    
    return true;
}

QList<ExternalWallpaperInfo> WNELAddon::getAllExternalWallpapers() const
{
    return m_externalWallpapers;
}

ExternalWallpaperInfo WNELAddon::getExternalWallpaperById(const QString& id) const
{
    auto it = std::find_if(m_externalWallpapers.begin(), m_externalWallpapers.end(),
                          [&id](const ExternalWallpaperInfo& info) {
                              return info.id == id;
                          });
    
    if (it != m_externalWallpapers.end()) {
        return *it;
    }
    
    return ExternalWallpaperInfo();
}

bool WNELAddon::hasExternalWallpaper(const QString& id) const
{
    return std::any_of(m_externalWallpapers.begin(), m_externalWallpapers.end(),
                      [&id](const ExternalWallpaperInfo& info) {
                          return info.id == id;
                      });
}

bool WNELAddon::launchExternalWallpaper(const QString& wallpaperId, const QStringList& additionalArgs)
{
    if (!m_enabled) {
        qCWarning(wnelAddon) << "WNEL addon is not enabled";
        return false;
    }
    
    ExternalWallpaperInfo info = getExternalWallpaperById(wallpaperId);
    if (info.id.isEmpty()) {
        qCWarning(wnelAddon) << "External wallpaper not found:" << wallpaperId;
        return false;
    }
    
    // Stop any currently running wallpaper
    stopWallpaper();
    
    // Get binary path
    ConfigManager& config = ConfigManager::instance();
    QString binaryPath = config.wnelBinaryPath();
    
    // Validate binary path
    QFileInfo binaryInfo(binaryPath);
    if (!binaryInfo.exists()) {
        qCWarning(wnelAddon) << "WNEL binary not found at:" << binaryPath;
        emit errorOccurred(QString("WNEL binary not found at: %1").arg(binaryPath));
        return false;
    }
    
    if (!binaryInfo.isExecutable()) {
        qCWarning(wnelAddon) << "WNEL binary is not executable:" << binaryPath;
        emit errorOccurred(QString("WNEL binary is not executable: %1").arg(binaryPath));
        return false;
    }
    
    // Validate symlink path
    QFileInfo symlinkInfo(info.symlinkPath);
    if (!symlinkInfo.exists()) {
        qCWarning(wnelAddon) << "External wallpaper symlink not found:" << info.symlinkPath;
        emit errorOccurred(QString("External wallpaper file not found: %1").arg(info.symlinkPath));
        return false;
    }
    
    // Create process
    m_wallpaperProcess = new QProcess(this);
    connect(m_wallpaperProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WNELAddon::onProcessFinished);
    connect(m_wallpaperProcess, &QProcess::errorOccurred, this, &WNELAddon::onProcessError);
    connect(m_wallpaperProcess, &QProcess::readyReadStandardOutput, this, &WNELAddon::onProcessOutput);
    connect(m_wallpaperProcess, &QProcess::readyReadStandardError, this, &WNELAddon::onProcessOutput);
    
    // Build command line arguments
    QStringList args;
    
    // Convert linux-wallpaperengine arguments to WNEL format
    for (int i = 0; i < additionalArgs.size(); ++i) {
        const QString& arg = additionalArgs[i];
        
        if (arg == "--volume" && i + 1 < additionalArgs.size()) {
            // Convert volume from percentage (0-100) to decimal (0.0-1.0)
            QString volumeStr = additionalArgs[i + 1];
            bool ok;
            int volumePercent = volumeStr.toInt(&ok);
            if (ok) {
                double volumeDecimal = volumePercent / 100.0;
                args << "--volume" << QString::number(volumeDecimal, 'f', 2);
            }
            i++; // Skip the volume value
        } else if (arg == "--fps" && i + 1 < additionalArgs.size()) {
            args << "--fps" << additionalArgs[i + 1];
            i++; // Skip the fps value
        } else if (arg == "--screen-root" && i + 1 < additionalArgs.size()) {
            args << "--output" << additionalArgs[i + 1]; // WNEL uses --output instead of --screen-root
            i++; // Skip the output value
        } else if (arg == "--silent") {
            args << "--silent";
        } else if (arg == "--scaling" && i + 1 < additionalArgs.size()) {
            args << "--scaling" << additionalArgs[i + 1];
            i++; // Skip the scaling value
        } else if (arg == "--no-loop") {
            args << "--no-loop";
        } else if (arg == "--no-hardware-decode") {
            args << "--no-hardware-decode";
        } else if (arg == "--mpv-options" && i + 1 < additionalArgs.size()) {
            args << "--mpv-options" << additionalArgs[i + 1];
            i++; // Skip the mpv options value
        } else if (arg == "--log-level" && i + 1 < additionalArgs.size()) {
            args << "--log-level" << additionalArgs[i + 1];
            i++; // Skip the log level value
        } else if (arg == "--noautomute") {
            args << "--noautomute";
        }
        // Skip linux-wallpaperengine specific arguments that don't apply to WNEL:
        // --assets-dir, --disable-mouse, --disable-parallax, --no-fullscreen-pause, etc.
    }
    
    // Add media file path at the end
    args.append(info.symlinkPath);
    
    qCDebug(wnelAddon) << "Original arguments:" << additionalArgs.join(" ");
    qCDebug(wnelAddon) << "Converted WNEL arguments:" << args.join(" ");
    qCDebug(wnelAddon) << "Launching external wallpaper with command:" << binaryPath;
    qCDebug(wnelAddon) << "Media file symlink path:" << info.symlinkPath;
    
    // Start process
    m_wallpaperProcess->start(binaryPath, args);
    
    if (!m_wallpaperProcess->waitForStarted(5000)) {
        QString errorMsg = QString("Failed to start WNEL process: %1").arg(m_wallpaperProcess->errorString());
        qCWarning(wnelAddon) << errorMsg;
        emit errorOccurred(errorMsg);
        m_wallpaperProcess->deleteLater();
        m_wallpaperProcess = nullptr;
        return false;
    }
    
    m_currentWallpaperId = wallpaperId;
    emit wallpaperLaunched(wallpaperId);
    
    return true;
}

void WNELAddon::stopWallpaper()
{
    if (m_wallpaperProcess) {
        qCDebug(wnelAddon) << "Stopping external wallpaper process";
        
        // Disconnect signals to prevent double cleanup
        m_wallpaperProcess->disconnect();
        
        m_wallpaperProcess->terminate();
        if (!m_wallpaperProcess->waitForFinished(3000)) {
            qCWarning(wnelAddon) << "Process did not terminate gracefully, killing it";
            m_wallpaperProcess->kill();
            m_wallpaperProcess->waitForFinished(1000);
        }
        
        m_wallpaperProcess->deleteLater();
        m_wallpaperProcess = nullptr;
        
        m_currentWallpaperId.clear();
        emit wallpaperStopped();
    }
}

bool WNELAddon::isWallpaperRunning() const
{
    return m_wallpaperProcess && m_wallpaperProcess->state() == QProcess::Running;
}

QString WNELAddon::getCurrentWallpaper() const
{
    return m_currentWallpaperId;
}

bool WNELAddon::createSymlink(const QString& target, const QString& linkPath)
{
    // Remove existing symlink if it exists
    if (QFileInfo::exists(linkPath)) {
        QFile::remove(linkPath);
    }
    
    return QFile::link(target, linkPath);
}

QString WNELAddon::generateProjectJsonContent(const ExternalWallpaperInfo& info) const
{
    QJsonObject project;
    project["external"] = true;
    project["title"] = info.name;
    project["type"] = info.type;
    project["file"] = QFileInfo(info.symlinkPath).fileName();
    
    if (!info.codec.isEmpty()) {
        project["codec"] = info.codec;
    }
    
    if (!info.resolution.isEmpty()) {
        project["width"] = info.resolution.width();
        project["height"] = info.resolution.height();
    }
    
    project["created"] = info.created.toString(Qt::ISODate);
    project["updated"] = info.updated.toString(Qt::ISODate);
    project["originalPath"] = info.originalPath;
    
    QJsonDocument doc(project);
    return doc.toJson(QJsonDocument::Indented);
}

bool WNELAddon::createProjectJson(const ExternalWallpaperInfo& info) const
{
    QFile file(info.projectPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(wnelAddon) << "Failed to create project.json:" << info.projectPath;
        return false;
    }
    
    QString content = generateProjectJsonContent(info);
    file.write(content.toUtf8());
    file.close();
    
    return true;
}

ExternalWallpaperInfo WNELAddon::parseProjectJson(const QString& projectPath) const
{
    ExternalWallpaperInfo info;
    
    QFile file(projectPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return info;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return info;
    }
    
    QJsonObject project = doc.object();
    
    // Only parse if it's marked as external
    if (!project.value("external").toBool()) {
        return info;
    }
    
    QFileInfo projectFileInfo(projectPath);
    QString wallpaperDir = projectFileInfo.absolutePath();
    
    info.id = projectFileInfo.dir().dirName();
    info.name = project.value("title").toString();
    info.type = project.value("type").toString();
    info.codec = project.value("codec").toString();
    info.projectPath = projectPath;
    info.originalPath = project.value("originalPath").toString();
    
    if (project.contains("width") && project.contains("height")) {
        info.resolution = QSize(project.value("width").toInt(), project.value("height").toInt());
    }
    
    info.created = QDateTime::fromString(project.value("created").toString(), Qt::ISODate);
    info.updated = QDateTime::fromString(project.value("updated").toString(), Qt::ISODate);
    
    // Find media file and preview
    QString fileName = project.value("file").toString();
    if (!fileName.isEmpty()) {
        info.symlinkPath = QDir(wallpaperDir).filePath(fileName);
    }
    
    QString previewPath = QDir(wallpaperDir).filePath("preview.png");
    if (QFileInfo::exists(previewPath)) {
        info.previewPath = previewPath;
    } else if (info.type == "image" || info.type == "gif") {
        info.previewPath = info.symlinkPath;
    }
    
    // Get file size
    QFileInfo mediaInfo(info.symlinkPath);
    if (mediaInfo.exists()) {
        info.fileSize = mediaInfo.size();
    }
    
    return info;
}

void WNELAddon::refreshExternalWallpapers()
{
    if (!m_enabled) return;
    
    m_externalWallpapers.clear();
    
    QDir externalDir(m_externalWallpapersPath);
    if (!externalDir.exists()) {
        return;
    }
    
    // Scan for wallpaper directories
    QStringList entries = externalDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        QString wallpaperDir = externalDir.filePath(entry);
        QString projectPath = QDir(wallpaperDir).filePath("project.json");
        
        if (QFileInfo::exists(projectPath)) {
            ExternalWallpaperInfo info = parseProjectJson(projectPath);
            if (!info.id.isEmpty()) {
                m_externalWallpapers.append(info);
            }
        }
    }
    
    qCDebug(wnelAddon) << "Refreshed external wallpapers, found:" << m_externalWallpapers.size();
}

void WNELAddon::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)
    qCDebug(wnelAddon) << "External wallpaper process finished with exit code:" << exitCode;
    
    if (m_wallpaperProcess) {
        // Don't call deleteLater here if we manually stopped the process
        // as stopWallpaper() already handles cleanup
        m_wallpaperProcess = nullptr;
    }
    
    m_currentWallpaperId.clear();
    emit wallpaperStopped();
}

void WNELAddon::onProcessError(QProcess::ProcessError error)
{
    QString errorString;
    switch (error) {
    case QProcess::FailedToStart:
        errorString = "Failed to start wallpaper process";
        break;
    case QProcess::Crashed:
        errorString = "Wallpaper process crashed";
        break;
    case QProcess::Timedout:
        errorString = "Wallpaper process timed out";
        break;
    case QProcess::WriteError:
        errorString = "Write error to wallpaper process";
        break;
    case QProcess::ReadError:
        errorString = "Read error from wallpaper process";
        break;
    default:
        errorString = "Unknown wallpaper process error";
        break;
    }
    
    qCWarning(wnelAddon) << "Wallpaper process error:" << errorString;
    emit errorOccurred(errorString);
}

void WNELAddon::onProcessOutput()
{
    if (m_wallpaperProcess) {
        QByteArray standardOutput = m_wallpaperProcess->readAllStandardOutput();
        QByteArray standardError = m_wallpaperProcess->readAllStandardError();
        
        if (!standardOutput.isEmpty()) {
            emit outputReceived(QString::fromUtf8(standardOutput));
        }
        
        if (!standardError.isEmpty()) {
            emit outputReceived(QString::fromUtf8(standardError));
        }
    }
}
