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
#include <QTimer>
#include <signal.h>
#include <cerrno>

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

std::optional<WallpaperInfo> WallpaperManager::getWallpaperInfo(const QString& id) const
{
    WallpaperInfo wallpaper = getWallpaperById(id);
    if (!wallpaper.id.isEmpty()) {
        return wallpaper;
    }
    return std::nullopt;
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
    
    // Add wallpaper path before property arguments
    args << wallpaper.path;
    
    // Check for backup file and add property arguments at the very end
    QString backupPath = wallpaper.projectPath + ".backup";
    if (QFileInfo::exists(backupPath)) {
        QStringList propertyArgs = generatePropertyArguments(wallpaper.projectPath);
        if (!propertyArgs.isEmpty()) {
            args.append(propertyArgs);
            // propertyArgs includes "--set-property" plus the property pairs
            int propertyCount = propertyArgs.size() - 1; // Subtract 1 for the --set-property flag
            emit outputReceived(QString("Found backup file, applying %1 property overrides").arg(propertyCount));
        }
    }
    
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
        qint64 pid = m_wallpaperProcess->processId();
        
        m_wallpaperProcess->terminate();
        
        if (!m_wallpaperProcess->waitForFinished(2000)) {
            qCWarning(wallpaperManager) << "Process did not terminate gracefully, killing it";
            m_wallpaperProcess->kill();
            m_wallpaperProcess->waitForFinished(1500);
        }
        
        // Kill child processes
        if (pid > 0) {
            QProcess pkillProcess;
            QStringList pkillArgs;
            pkillArgs << "-P" << QString::number(pid);
            pkillProcess.start("pkill", pkillArgs);
            pkillProcess.waitForFinished(1000);
            
            if (pkillProcess.exitCode() == 0) {
                qCDebug(wallpaperManager) << "Killed child processes of wallpaper";
            }
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

bool WallpaperManager::launchMultiMonitorWallpaper(const QMap<QString, QString>& screenAssignments)
{
    if (screenAssignments.isEmpty()) {
        emit errorOccurred("No screen assignments provided");
        return false;
    }
    
    ConfigManager& config = ConfigManager::instance();
    QString binaryPath = config.wallpaperEnginePath();
    
    if (binaryPath.isEmpty()) {
        emit errorOccurred("Wallpaper Engine binary path not configured");
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
    
    // Build command line arguments using Engine Defaults
    QStringList args;
    
    // Add global audio settings
    if (config.globalSilent()) args << "--silent";
    
    int volume = config.globalVolume();
    if (volume != 15) args << "--volume" << QString::number(volume);
    
    if (config.globalNoAutoMute()) args << "--noautomute";
    if (config.globalNoAudioProcessing()) args << "--no-audio-processing";
    
    // Add global performance settings
    int fps = config.globalFps();
    if (fps != 30) args << "--fps" << QString::number(fps);
    
    // Add global display settings (not screen-root/window-geometry, those are per-screen)
    QString scaling = config.globalScaling();
    if (scaling != "default") args << "--scaling" << scaling;
    
    QString clamping = config.globalClamping();
    if (clamping != "clamp") args << "--clamping" << clamping;
    
    // Add global behavior settings
    if (config.globalDisableMouse()) args << "--disable-mouse";
    if (config.globalDisableParallax()) args << "--disable-parallax";
    if (config.globalNoFullscreenPause()) args << "--no-fullscreen-pause";
    
    // Add assets directory
    QString assetsDir = config.getAssetsDir();
    if (!assetsDir.isEmpty()) {
        args << "--assets-dir" << assetsDir;
    }
    
    // Add screen-root and --bg pairs for each screen assignment
    for (auto it = screenAssignments.constBegin(); it != screenAssignments.constEnd(); ++it) {
        QString screenName = it.key();
        QString wallpaperId = it.value();
        
        WallpaperInfo wallpaper = getWallpaperById(wallpaperId);
        if (wallpaper.id.isEmpty()) {
            emit errorOccurred("Wallpaper not found: " + wallpaperId);
            delete m_wallpaperProcess;
            m_wallpaperProcess = nullptr;
            return false;
        }
        
        args << "--screen-root" << screenName;
        args << "--bg" << wallpaper.path;
    }
    
    emit outputReceived(QString("Launching multi-monitor wallpaper setup (%1 screens)")
                       .arg(screenAssignments.size()));
    emit outputReceived(QString("Command: %1 %2").arg(binaryPath, args.join(" ")));
    
    // Set working directory
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
    
    // Store first wallpaper ID as current (for tracking purposes)
    m_currentWallpaperId = screenAssignments.values().first();
    emit wallpaperLaunched(m_currentWallpaperId);
    return true;
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
    // Only report actual errors, not normal operation
    if (!m_wallpaperProcess) {
        return;
    }
    
    // For crash errors, delay the check to avoid false alarms during startup
    if (error == QProcess::Crashed) {
        // Use a single-shot timer to check the process state after a brief delay
        // This prevents false crash reports during process initialization
        QTimer::singleShot(100, this, [this, error]() {
            if (!m_wallpaperProcess) {
                return; // Process was cleaned up already
            }
            
            // Only report crash if process actually exited abnormally
            if (m_wallpaperProcess->state() == QProcess::NotRunning && 
                m_wallpaperProcess->exitStatus() == QProcess::CrashExit) {
                emit outputReceived("ERROR: Wallpaper process crashed");
                emit errorOccurred("Wallpaper process crashed");
            }
            // If process is still running or exited normally, don't report as crash
        });
        return;
    }
    
    // For other errors, check immediately but still verify process state
    if (m_wallpaperProcess->state() == QProcess::Running) {
        // Process is still running, this might be a false alarm
        return;
    }
    
    QString errorString;
    switch (error) {
    case QProcess::FailedToStart:
        errorString = "Failed to start wallpaper process";
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
            QString stderrOutput = QString::fromUtf8(data).trimmed();
            
            // Filter out normal mpv/wallpaper engine operational messages
            // Only treat as errors if they contain actual error indicators
            if (stderrOutput.contains("ERROR", Qt::CaseInsensitive) ||
                stderrOutput.contains("FATAL", Qt::CaseInsensitive) ||
                stderrOutput.contains("CRITICAL", Qt::CaseInsensitive) ||
                (stderrOutput.contains("failed", Qt::CaseInsensitive) && 
                 !stderrOutput.contains("Fullscreen detection not supported") &&
                 !stderrOutput.contains("Failed to initialize GLEW"))) {
                // This looks like an actual error
                emit outputReceived("ERROR: " + stderrOutput);
            } else {
                // This is likely normal operational output (mpv logging, etc.)
                emit outputReceived("LOG: " + stderrOutput);
            }
        }
    }
}

QStringList WallpaperManager::generatePropertyArguments(const QString& projectJsonPath)
{
    QStringList propertyArgs;
    
    // Read the current project.json file (which contains modified properties)
    QFile file(projectJsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(wallpaperManager) << "Failed to open project.json for property arguments:" << projectJsonPath;
        return propertyArgs;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qCWarning(wallpaperManager) << "Failed to parse project.json for properties:" << error.errorString();
        return propertyArgs;
    }
    
    // Extract properties using the same logic as extractProperties()
    QJsonObject projectJson = doc.object();
    QJsonObject properties = extractProperties(projectJson);
    
    // Convert properties to --set-property arguments
    // Format: --set-property name1=value1 name2=value2 name3=value3
    QStringList propertyPairs;
    
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        QString propName = it.key();
        QJsonObject propObj = it.value().toObject();
        
        if (propObj.contains("value")) {
            QJsonValue value = propObj.value("value");
            QString valueStr;
            
            // Convert value to string based on type
            if (value.isBool()) {
                valueStr = value.toBool() ? "true" : "false";
            } else if (value.isDouble()) {
                valueStr = QString::number(value.toDouble());
            } else if (value.isString()) {
                valueStr = value.toString();
            } else {
                // For other types, use JSON representation
                QJsonDocument valueDoc(QJsonArray{value});
                valueStr = valueDoc.toJson(QJsonDocument::Compact);
                valueStr = valueStr.mid(1, valueStr.length() - 2); // Remove [ and ]
            }
            
            propertyPairs << QString("%1=%2").arg(propName, valueStr);
            qCDebug(wallpaperManager) << "Added property:" << propName << "=" << valueStr;
        }
    }
    
    // Add --set-property flag followed by all property pairs
    if (!propertyPairs.isEmpty()) {
        propertyArgs << "--set-property";
        propertyArgs.append(propertyPairs);
    }
    
    qCDebug(wallpaperManager) << "Generated" << propertyPairs.size() << "property arguments from" << projectJsonPath;
    return propertyArgs;
}

bool WallpaperManager::verifyProcessTerminated(qint64 pid)
{
    if (pid <= 0) {
        return true;
    }
    
    int killResult = kill(pid, 0);
    
    if (killResult == 0) {
        qCWarning(wallpaperManager) << "Process still exists after termination";
        return false;
    } else if (errno == ESRCH) {
        qCDebug(wallpaperManager) << "Process terminated successfully";
        return true;
    } else {
        qCWarning(wallpaperManager) << "Error checking process state";
        return false;
    }
}
