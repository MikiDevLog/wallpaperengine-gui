#include "WallpaperManager.h"
#include "ConfigManager.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QLoggingCategory>
#include <QProcessEnvironment>

Q_LOGGING_CATEGORY(wallpaperManager, "app.wallpaperManager")

WallpaperManager::WallpaperManager(QObject* parent)
    : QObject(parent)
    , m_wallpaperProcess(nullptr)
    , m_refreshing(false)
{
}

WallpaperManager::~WallpaperManager()
{
    stopWallpaper();
}

void WallpaperManager::refreshWallpapers()
{
    if (m_refreshing) {
        qCDebug(wallpaperManager) << "Refresh already in progress";
        return;
    }
    
    m_refreshing = true;
    m_wallpapers.clear();
    
    qCDebug(wallpaperManager) << "Starting wallpaper refresh";
    scanWorkshopDirectories();
    
    m_refreshing = false;
    emit refreshFinished();
    emit wallpapersChanged();
}

void WallpaperManager::scanWorkshopDirectories()
{
    ConfigManager& config = ConfigManager::instance();
    QStringList libraryPaths = config.steamLibraryPaths();
    
    if (libraryPaths.isEmpty()) {
        QString steamPath = config.steamPath();
        if (!steamPath.isEmpty()) {
            libraryPaths.append(steamPath);
        }
    }
    
    QStringList workshopPaths;
    for (const QString& libraryPath : libraryPaths) {
        QString workshopPath = QDir(libraryPath).filePath("steamapps/workshop/content/431960");
        if (QDir(workshopPath).exists()) {
            workshopPaths.append(workshopPath);
        }
    }
    
    if (workshopPaths.isEmpty()) {
        qCWarning(wallpaperManager) << "No workshop directories found";
        emit errorOccurred("No Steam workshop directories found. Please check your Steam installation path.");
        return;
    }
    
    int totalDirectories = 0;
    for (const QString& workshopPath : workshopPaths) {
        QDir workshopDir(workshopPath);
        totalDirectories += workshopDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).size();
    }
    
    int processed = 0;
    for (const QString& workshopPath : workshopPaths) {
        QDir workshopDir(workshopPath);
        QStringList wallpaperDirs = workshopDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        for (const QString& dirName : wallpaperDirs) {
            QString fullPath = workshopDir.filePath(dirName);
            processWallpaperDirectory(fullPath);
            
            processed++;
            emit refreshProgress(processed, totalDirectories);
        }
    }
    
    qCInfo(wallpaperManager) << "Found" << m_wallpapers.size() << "wallpapers";
}

void WallpaperManager::processWallpaperDirectory(const QString& dirPath)
{
    QDir wallpaperDir(dirPath);
    QString projectPath = wallpaperDir.filePath("project.json");
    
    if (!QFileInfo::exists(projectPath)) {
        return; // Skip directories without project.json
    }
    
    WallpaperInfo wallpaper = parseProjectJson(projectPath);
    if (!wallpaper.id.isEmpty()) {
        wallpaper.path = dirPath;
        wallpaper.projectPath = projectPath;
        wallpaper.previewPath = findPreviewImage(dirPath);
        m_wallpapers.append(wallpaper);
    }
}

WallpaperInfo WallpaperManager::parseProjectJson(const QString& projectPath)
{
    WallpaperInfo wallpaper;
    
    QFile file(projectPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(wallpaperManager) << "Failed to open project.json:" << projectPath;
        return wallpaper;
    }
    
    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qCWarning(wallpaperManager) << "Failed to parse project.json:" << error.errorString();
        return wallpaper;
    }
    
    QJsonObject root = doc.object();
    
    // Extract basic info
    wallpaper.id = extractWorkshopId(QFileInfo(projectPath).dir().path());
    wallpaper.name = root.value("title").toString();
    wallpaper.description = root.value("description").toString();
    wallpaper.type = root.value("type").toString();
    
    // Extract file size
    QFileInfo dirInfo(QFileInfo(projectPath).dir().path());
    wallpaper.fileSize = dirInfo.size();
    
    // Extract tags
    QJsonArray tagsArray = root.value("tags").toArray();
    QStringList tags;
    for (const QJsonValue& tagValue : tagsArray) {
        tags.append(tagValue.toString());
    }
    wallpaper.tags = tags;
    
    // Extract properties - this is the key part for the Properties Panel
    wallpaper.properties = extractProperties(root);
    
    qCDebug(wallpaperManager) << "Parsed wallpaper:" << wallpaper.name 
                              << "with" << wallpaper.properties.size() << "properties";
    
    return wallpaper;
}

QJsonObject WallpaperManager::extractProperties(const QJsonObject& projectJson)
{
    QJsonObject properties;
    
    // Look for properties in the "general" section
    QJsonObject general = projectJson.value("general").toObject();
    if (general.contains("properties")) {
        QJsonObject generalProps = general.value("properties").toObject();
        
        // Merge general properties
        for (auto it = generalProps.begin(); it != generalProps.end(); ++it) {
            properties[it.key()] = it.value();
        }
    }
    
    // Also check for properties directly in root
    if (projectJson.contains("properties")) {
        QJsonObject rootProps = projectJson.value("properties").toObject();
        
        // Merge root properties
        for (auto it = rootProps.begin(); it != rootProps.end(); ++it) {
            properties[it.key()] = it.value();
        }
    }
    
    return properties;
}

QString WallpaperManager::findPreviewImage(const QString& wallpaperDir)
{
    QDir dir(wallpaperDir);
    QStringList filters = {"preview.*", "thumb.*", "thumbnail.*"};
    QStringList imageExtensions = {"jpg", "jpeg", "png", "gif", "bmp"};
    
    for (const QString& filter : filters) {
        QStringList matches = dir.entryList({filter}, QDir::Files);
        for (const QString& match : matches) {
            QString ext = QFileInfo(match).suffix().toLower();
            if (imageExtensions.contains(ext)) {
                return dir.filePath(match);
            }
        }
    }
    
    // Fallback: look for any image file
    for (const QString& ext : imageExtensions) {
        QStringList images = dir.entryList({"*." + ext}, QDir::Files);
        if (!images.isEmpty()) {
            return dir.filePath(images.first());
        }
    }
    
    return QString();
}

QString WallpaperManager::extractWorkshopId(const QString& dirPath)
{
    QFileInfo pathInfo(dirPath);
    QString dirName = pathInfo.fileName();
    
    // Check if directory name is a numeric workshop ID
    bool ok;
    dirName.toULongLong(&ok);
    if (ok) {
        return dirName;
    }
    
    // Fallback: extract from path pattern
    QRegularExpression workshopRegex(R"(/workshop/content/431960/(\d+))");
    QRegularExpressionMatch match = workshopRegex.match(dirPath);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    
    return dirName; // Use directory name as fallback
}

QList<WallpaperInfo> WallpaperManager::getAllWallpapers() const
{
    return m_wallpapers;
}

WallpaperInfo WallpaperManager::getWallpaperById(const QString& id) const
{
    for (const WallpaperInfo& wallpaper : m_wallpapers) {
        if (wallpaper.id == id) {
            return wallpaper;
        }
    }
    return WallpaperInfo();
}

bool WallpaperManager::launchWallpaper(const QString& wallpaperId, const QStringList& additionalArgs)
{
    ConfigManager& config = ConfigManager::instance();
    QString binaryPath = config.wallpaperEnginePath();
    
    if (binaryPath.isEmpty()) {
        emit errorOccurred("Wallpaper Engine binary path not configured");
        return false;
    }
    
    WallpaperInfo wallpaper = getWallpaperById(wallpaperId);
    if (wallpaper.id.isEmpty()) {
        emit errorOccurred("Wallpaper not found: " + wallpaperId);
        return false;
    }
    
    // Stop current wallpaper if running
    stopWallpaper();
    
    // Create new process
    m_wallpaperProcess = new QProcess(this);
    connect(m_wallpaperProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WallpaperManager::onProcessFinished);
    connect(m_wallpaperProcess, &QProcess::errorOccurred,
            this, &WallpaperManager::onProcessError);
    connect(m_wallpaperProcess, &QProcess::readyReadStandardOutput,
            this, &WallpaperManager::onProcessOutput);
    connect(m_wallpaperProcess, &QProcess::readyReadStandardError,
            this, &WallpaperManager::onProcessOutput);
    
    // Build command line arguments
    QStringList args;
    
    // Add additional arguments (custom settings) first
    args.append(additionalArgs);
    
    // Add assets directory if configured and not already present in additionalArgs
    ConfigManager& configManager = ConfigManager::instance();
    QString assetsDir = configManager.getAssetsDir();
    if (!assetsDir.isEmpty() && !args.contains("--assets-dir")) {
        args << "--assets-dir" << assetsDir;
    }
    
    // Add wallpaper path as final argument (not with --dir flag)
    args << wallpaper.path;
    
    emit outputReceived(QString("Launching wallpaper: %1").arg(wallpaper.name));
    emit outputReceived(QString("Command: %1 %2").arg(binaryPath, args.join(" ")));
    
    // Set working directory to the directory containing the binary
    QFileInfo binaryInfo(binaryPath);
    QString workingDir = binaryInfo.absolutePath();
    m_wallpaperProcess->setWorkingDirectory(workingDir);
    
    // Preserve the current environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    m_wallpaperProcess->setProcessEnvironment(env);
    
    // Start process
    m_wallpaperProcess->start(binaryPath, args);
    
    if (!m_wallpaperProcess->waitForStarted(5000)) {
        emit errorOccurred("Failed to start wallpaper process");
        delete m_wallpaperProcess;
        m_wallpaperProcess = nullptr;
        return false;
    }
    
    m_currentWallpaperId = wallpaperId;
    emit wallpaperLaunched(wallpaperId);
    return true;
}

void WallpaperManager::stopWallpaper()
{
    if (m_wallpaperProcess) {
        emit outputReceived("Stopping wallpaper...");
        m_wallpaperProcess->terminate();
        
        if (!m_wallpaperProcess->waitForFinished(5000)) {
            m_wallpaperProcess->kill();
            m_wallpaperProcess->waitForFinished(3000);
        }
        
        delete m_wallpaperProcess;
        m_wallpaperProcess = nullptr;
        m_currentWallpaperId.clear();
        emit wallpaperStopped();
    }
}

bool WallpaperManager::isWallpaperRunning() const
{
    return m_wallpaperProcess && m_wallpaperProcess->state() == QProcess::Running;
}

QString WallpaperManager::getCurrentWallpaper() const
{
    return m_currentWallpaperId;
}

void WallpaperManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    emit outputReceived(QString("Wallpaper process finished (exit code: %1, status: %2)")
                       .arg(exitCode)
                       .arg(exitStatus == QProcess::NormalExit ? "Normal" : "Crashed"));
    
    m_currentWallpaperId.clear();
    
    if (m_wallpaperProcess) {
        m_wallpaperProcess->deleteLater();
        m_wallpaperProcess = nullptr;
    }
    
    emit wallpaperStopped();
}

void WallpaperManager::onProcessError(QProcess::ProcessError error)
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
        errorString = "Unknown error in wallpaper process";
        break;
    }
    
    emit outputReceived("ERROR: " + errorString);
    emit errorOccurred(errorString);
}

void WallpaperManager::onProcessOutput()
{
    if (m_wallpaperProcess) {
        QByteArray data = m_wallpaperProcess->readAllStandardOutput();
        if (!data.isEmpty()) {
            emit outputReceived(QString::fromUtf8(data).trimmed());
        }
        
        data = m_wallpaperProcess->readAllStandardError();
        if (!data.isEmpty()) {
            emit outputReceived("STDERR: " + QString::fromUtf8(data).trimmed());
        }
    }
}
