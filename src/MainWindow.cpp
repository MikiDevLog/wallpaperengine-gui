#include "MainWindow.h"
#include "WallpaperPreview.h"
#include "PropertiesPanel.h"
#include "SettingsDialog.h"
#include "ConfigManager.h"
#include "WallpaperManager.h"
#include "SteamDetector.h"
#include <QApplication>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QMessageBox>
#include <QCloseEvent>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTabWidget>
#include <QDateTime>
#include <QFileDialog>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(mainWindow, "app.mainwindow")

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_splitter(nullptr)
    , m_wallpaperPreview(nullptr)
    , m_propertiesPanel(nullptr)
    , m_refreshAction(nullptr)
    , m_settingsAction(nullptr)
    , m_aboutAction(nullptr)
    , m_exitAction(nullptr)
    , m_statusLabel(nullptr)
    , m_wallpaperCountLabel(nullptr)
    , m_progressBar(nullptr)
    , m_config(ConfigManager::instance())
    , m_wallpaperManager(new WallpaperManager(this))
    , m_refreshing(false)
{
    setWindowTitle("Wallpaper Engine GUI");
    setWindowIcon(QIcon(":/icons/wallpaper-engine.png"));
    
    setupUI();
    loadSettings();
    
    // Check for first run after UI is set up
    QTimer::singleShot(100, this, &MainWindow::checkFirstRun);
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::setupUI()
{
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    createCentralWidget();
    
    // Set initial window size
    resize(1200, 800);
    
    // Connect wallpaper manager signals
    connect(m_wallpaperManager, &WallpaperManager::refreshProgress,
            this, &MainWindow::onRefreshProgress);
    connect(m_wallpaperManager, &WallpaperManager::refreshFinished,
            this, &MainWindow::onRefreshFinished);
    connect(m_wallpaperManager, &WallpaperManager::errorOccurred,
            this, [this](const QString& error) {
                QMessageBox::warning(this, "Error", error);
                m_statusLabel->setText("Error: " + error);
            });
}

void MainWindow::setupMenuBar()
{
    // File menu
    auto *fileMenu = menuBar()->addMenu("&File");
    
    m_refreshAction = new QAction(QIcon(":/icons/refresh.png"), "&Refresh Wallpapers", this);
    m_refreshAction->setShortcut(QKeySequence::Refresh);
    m_refreshAction->setStatusTip("Refresh wallpaper list from Steam workshop");
    connect(m_refreshAction, &QAction::triggered, this, &MainWindow::refreshWallpapers);
    fileMenu->addAction(m_refreshAction);
    
    fileMenu->addSeparator();
    
    m_settingsAction = new QAction(QIcon(":/icons/settings.png"), "&Settings", this);
    m_settingsAction->setShortcut(QKeySequence::Preferences);
    m_settingsAction->setStatusTip("Open application settings");
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
    fileMenu->addAction(m_settingsAction);
    
    fileMenu->addSeparator();
    
    m_exitAction = new QAction("E&xit", this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_exitAction->setStatusTip("Exit the application");
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(m_exitAction);
    
    // Help menu
    auto *helpMenu = menuBar()->addMenu("&Help");
    
    m_aboutAction = new QAction("&About", this);
    m_aboutAction->setStatusTip("Show application information");
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    helpMenu->addAction(m_aboutAction);
    
    auto *aboutQtAction = new QAction("About &Qt", this);
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
    helpMenu->addAction(aboutQtAction);
}

void MainWindow::setupToolBar()
{
    auto *toolBar = addToolBar("Main");
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    
    // Remove the refresh action from the toolbar
    // toolBar->addAction(m_refreshAction);
    // toolBar->addSeparator();
    
    toolBar->addAction(m_settingsAction);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("Ready");
    statusBar()->addWidget(m_statusLabel);
    
    statusBar()->addPermanentWidget(new QLabel("|"));
    
    m_wallpaperCountLabel = new QLabel("0 wallpapers");
    statusBar()->addPermanentWidget(m_wallpaperCountLabel);
    
    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumWidth(200);
    statusBar()->addPermanentWidget(m_progressBar);
}

void MainWindow::createCentralWidget()
{
    m_splitter = new QSplitter(Qt::Horizontal);
    setCentralWidget(m_splitter);

    // left: wallpaper preview
    m_wallpaperPreview = new WallpaperPreview;
    m_splitter->addWidget(m_wallpaperPreview);

    // connect preview to manager so grid updates
    m_wallpaperPreview->setWallpaperManager(m_wallpaperManager);

    // right: properties panel with 4 tabs
    m_propertiesPanel = new PropertiesPanel;
    m_splitter->addWidget(m_propertiesPanel);

    // Store a reference to the properties panel's inner tab widget
    m_rightTabWidget = m_propertiesPanel->m_innerTabWidget;

    // instantiate output controls
    m_outputTextEdit    = new QTextEdit;
    m_clearOutputButton = new QPushButton("Clear");
    m_saveOutputButton  = new QPushButton("Save Log");
    m_outputTextEdit->setReadOnly(true);
    connect(m_clearOutputButton, &QPushButton::clicked, this, &MainWindow::clearOutput);
    connect(m_saveOutputButton,  &QPushButton::clicked, this, &MainWindow::saveOutput);

    // reparent output controls into "Engine Log" tab
    if (auto *logLayout = qobject_cast<QVBoxLayout*>(m_propertiesPanel->engineLogTab()->layout())) {
        logLayout->addWidget(m_outputTextEdit);
        logLayout->addWidget(m_clearOutputButton);
        logLayout->addWidget(m_saveOutputButton);
    }

    // preview → selection
    connect(m_wallpaperPreview, &WallpaperPreview::wallpaperSelected,
            this, &MainWindow::onWallpaperSelected);
    connect(m_wallpaperPreview, &WallpaperPreview::wallpaperDoubleClicked,
            this, &MainWindow::onWallpaperLaunched);

    // properties panel → launch
    connect(m_propertiesPanel, &PropertiesPanel::launchWallpaper,
            this, &MainWindow::onWallpaperLaunched);

    // wallpaper manager → output log
    connect(m_wallpaperManager, &WallpaperManager::outputReceived,
            this, &MainWindow::onOutputReceived);

    // initial splitter sizing
    m_splitter->setSizes({840, 360});
}

void MainWindow::loadSettings()
{
    // Restore window geometry
    restoreGeometry(m_config.windowGeometry());
    restoreState(m_config.windowState());
    
    // Restore splitter state
    m_splitter->restoreState(m_config.getSplitterState());
}

void MainWindow::saveSettings()
{
    // Save window geometry
    m_config.setWindowGeometry(saveGeometry());
    m_config.setWindowState(saveState());
    
    // Save splitter state
    m_config.setSplitterState(m_splitter->saveState());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

void MainWindow::checkFirstRun()
{
    // Manual debug of config reading
    qCDebug(mainWindow) << "About to read isFirstRun() and isConfigurationValid()";
    
    bool isFirstRun = m_config.isFirstRun();
    bool isConfigValid = m_config.isConfigurationValid();
    
    qCDebug(mainWindow) << "Startup check: isFirstRun=" << isFirstRun << "isConfigValid=" << isConfigValid;
    qCDebug(mainWindow) << "Steam path:" << m_config.steamPath();
    qCDebug(mainWindow) << "Steam library paths:" << m_config.steamLibraryPaths();
    qCDebug(mainWindow) << "WE binary path:" << m_config.wallpaperEnginePath();
    qCDebug(mainWindow) << "Configuration issues:" << m_config.getConfigurationIssues();
    
    // Manually test reading the config value
    QSettings testSettings(m_config.configDir() + "/config.ini", QSettings::IniFormat);
    qCDebug(mainWindow) << "Manual config read test: general/first_run =" << testSettings.value("general/first_run", "NOT_FOUND");
    QStringList allKeys = testSettings.allKeys();
    qCDebug(mainWindow) << "All config keys found:" << allKeys;
    qCDebug(mainWindow) << "Number of keys found:" << allKeys.size();
    
    // Test different possible key names
    qCDebug(mainWindow) << "Trying 'General/first_run':" << testSettings.value("General/first_run", "NOT_FOUND");
    qCDebug(mainWindow) << "Trying 'first_run':" << testSettings.value("first_run", "NOT_FOUND");
    
    // If configuration is valid, clear first run flag and proceed
    if (isConfigValid) {
        if (isFirstRun) {
            qCInfo(mainWindow) << "Configuration is valid, clearing first-run flag";
            m_config.setFirstRun(false);
        }
        qCInfo(mainWindow) << "Configuration is valid, starting automatic initialization";
        initializeWithValidConfig();
    } else {
        // Configuration is invalid, show appropriate dialog
        QString issues = m_config.getConfigurationIssues();
        if (isFirstRun) {
            qCInfo(mainWindow) << "First run detected, showing welcome dialog";
            showFirstRunDialog();
        } else {
            qCInfo(mainWindow) << "Configuration invalid:" << issues;
            showConfigurationIssuesDialog(issues);
        }
    }
}

void MainWindow::initializeWithValidConfig()
{
    // Add your custom library path if it's not already configured
    QStringList libraryPaths = m_config.steamLibraryPaths();
    QString customLibraryPath = "/home/miki/general/steamlibrary";
    
    if (QDir(customLibraryPath).exists() && !libraryPaths.contains(customLibraryPath)) {
        libraryPaths.append(customLibraryPath);
        m_config.setSteamLibraryPaths(libraryPaths);
        qCDebug(mainWindow) << "Added custom library path:" << customLibraryPath;
    }
    
    // Start automatic wallpaper refresh
    qCInfo(mainWindow) << "Starting automatic wallpaper refresh";
    m_statusLabel->setText("Initializing... Loading wallpapers");
    QTimer::singleShot(500, this, &MainWindow::refreshWallpapers);
    
    // TODO: Load and launch last selected wallpapers with their settings
    QString lastWallpaper = m_config.lastSelectedWallpaper();
    if (!lastWallpaper.isEmpty()) {
        qCInfo(mainWindow) << "Will restore last wallpaper:" << lastWallpaper;
        // Schedule wallpaper restoration after refresh completes
        QTimer::singleShot(2000, this, [this, lastWallpaper]() {
            // This will be called after refresh hopefully completes
            // Implementation for launching wallpaper can be added here
            qCDebug(mainWindow) << "TODO: Restore wallpaper:" << lastWallpaper;
        });
    }
}

void MainWindow::showFirstRunDialog()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Welcome to Wallpaper Engine GUI");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText("Welcome to Wallpaper Engine GUI!");
    msgBox.setInformativeText(
        "This application provides a graphical interface for linux-wallpaperengine.\n\n"
        "To get started, you'll need to:\n"
        "1. Configure the path to your compiled linux-wallpaperengine binary\n"
        "2. Set up Steam detection to find your wallpapers\n\n"
        "Would you like to open the settings now?");
    
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    
    if (msgBox.exec() == QMessageBox::Yes) {
        openSettings();
    }
}

void MainWindow::showConfigurationIssuesDialog(const QString& issues)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Configuration Issues");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText("Configuration needs attention");
    msgBox.setInformativeText(
        issues + "\n\n"
        "The application cannot function properly without valid configuration.\n"
        "Would you like to open the settings to fix these issues?");
    
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    
    if (msgBox.exec() == QMessageBox::Yes) {
        openSettings();
    } else {
        // User chose not to configure, show a warning in status bar
        m_statusLabel->setText("Warning: Configuration incomplete - check Settings");
        m_statusLabel->setStyleSheet("color: orange;");
    }
}

void MainWindow::openSettings()
{
    bool wasConfigValid = m_config.isConfigurationValid();
    
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // Settings were saved, update status
        updateStatusBar();
        
        bool isConfigValid = m_config.isConfigurationValid();
        
        if (!wasConfigValid && isConfigValid) {
            // Configuration just became valid
            m_statusLabel->setText("Configuration complete!");
            m_statusLabel->setStyleSheet("color: green;");
            
            QMessageBox::information(this, "Configuration Complete",
                "Settings have been saved successfully!\n\n"
                "The application will now automatically refresh wallpapers and is ready to use.");
            
            // Start automatic initialization
            QTimer::singleShot(500, this, &MainWindow::initializeWithValidConfig);
            
        } else if (isConfigValid && !m_refreshing) {
            // Configuration was already valid, just offer refresh
            if (!m_config.steamPath().isEmpty() || !m_config.steamLibraryPaths().isEmpty()) {
                auto result = QMessageBox::question(this, "Refresh Wallpapers",
                    "Settings have been updated. Would you like to refresh the wallpaper list now?");
                if (result == QMessageBox::Yes) {
                    refreshWallpapers();
                }
            }
        } else if (!isConfigValid) {
            // Configuration is still invalid
            QString issues = m_config.getConfigurationIssues();
            m_statusLabel->setText("Configuration incomplete");
            m_statusLabel->setStyleSheet("color: orange;");
            
            QMessageBox::warning(this, "Configuration Incomplete",
                "The configuration still has issues:\n\n" + issues + 
                "\n\nPlease ensure all required paths are correctly configured.");
        }
    }
}

void MainWindow::refreshWallpapers()
{
    // Better guard against multiple refreshes
    if (m_refreshing) {
        qCDebug(mainWindow) << "Refresh already in progress, ignoring request";
        return;
    }
    
    // Check if Steam path is configured
    if (m_config.steamPath().isEmpty() && m_config.steamLibraryPaths().isEmpty()) {
        QMessageBox::warning(this, "Steam Path Not Configured",
            "Please configure the Steam installation path or library paths in Settings first.");
        openSettings();
        return;
    }
    
    // Set refresh state
    m_refreshing = true;
    m_refreshAction->setEnabled(false);
    m_progressBar->setVisible(true);
    m_statusLabel->setText("Refreshing wallpapers...");
    
    // Clear current selection
    m_propertiesPanel->clear();
    
    // Force cursor to wait to indicate that refresh is happening
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    qCDebug(mainWindow) << "Starting wallpaper refresh...";
    
    // Start wallpaper discovery
    m_wallpaperManager->refreshWallpapers();
}

void MainWindow::onRefreshProgress(int current, int total)
{
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
    m_statusLabel->setText(QString("Processing wallpaper %1 of %2...").arg(current).arg(total));
}

void MainWindow::onRefreshFinished()
{
    qCDebug(mainWindow) << "Refresh finished, updating UI";
    
    // Reset refresh state
    m_refreshing = false;
    m_refreshAction->setEnabled(true);
    m_progressBar->setVisible(false);
    
    // Restore cursor - let individual components restore their own cursors
    QApplication::restoreOverrideCursor();
    
    // Reset status bar styling
    m_statusLabel->setStyleSheet("");
    
    // Update wallpaper preview 
    auto wallpapers = m_wallpaperManager->getAllWallpapers();
    
    // Update wallpaper count display
    int count = wallpapers.size();
    m_wallpaperCountLabel->setText(QString("%1 wallpapers").arg(count));
    
    if (count > 0) {
        m_statusLabel->setText(QString("Ready - Found %1 wallpapers").arg(count));
        qCInfo(mainWindow) << "Loaded" << count << "wallpapers successfully";
    } else {
        m_statusLabel->setText("No wallpapers found");
        qCWarning(mainWindow) << "No wallpapers found in configured Steam directories";
        QMessageBox::information(this, "No Wallpapers Found",
            "No wallpapers were found in the configured Steam directories.\n\n"
            "Make sure you have Wallpaper Engine installed through Steam and have "
            "subscribed to some wallpapers from the Steam Workshop.");
    }
    
    // Schedule an extra UI update to ensure grid is displayed properly
    QTimer::singleShot(100, [this]() {
        if (m_wallpaperPreview) {
            QMetaObject::invokeMethod(m_wallpaperPreview, "update");
        }
    });
}

void MainWindow::onWallpaperSelected(const WallpaperInfo& wallpaper)
{
    qCDebug(mainWindow) << "onWallpaperSelected - START:" << wallpaper.name;
    
    try {
        if (!wallpaper.id.isEmpty()) {
            qCDebug(mainWindow) << "Setting wallpaper on properties panel";
            m_propertiesPanel->setWallpaper(wallpaper);
            m_statusLabel->setText(QString("Selected: %1").arg(wallpaper.name));
        } else {
            qCDebug(mainWindow) << "Clearing properties panel";
            m_propertiesPanel->clear();
            m_statusLabel->setText("Ready");
        }
    } catch (const std::exception& e) {
        qCCritical(mainWindow) << "Exception in onWallpaperSelected:" << e.what();
    } catch (...) {
        qCCritical(mainWindow) << "Unknown exception in onWallpaperSelected";
    }
    
    qCDebug(mainWindow) << "onWallpaperSelected - END:" << wallpaper.name;
}

void MainWindow::onWallpaperLaunched(const WallpaperInfo& wallpaper)
{
    qCDebug(mainWindow) << "onWallpaperLaunched - START:" << wallpaper.name << "ID:" << wallpaper.id;
    
    try {
        if (m_config.wallpaperEnginePath().isEmpty()) {
            qCWarning(mainWindow) << "Wallpaper Engine binary path not configured";
            QMessageBox::warning(this, "Wallpaper Engine Not Configured",
                "Please configure the path to linux-wallpaperengine binary in Settings first.");
            openSettings();
            return;
        }
        
        qCDebug(mainWindow) << "Binary path configured, switching to output tab";
        // Switch to output tab to show launch progress
        m_rightTabWidget->setCurrentIndex(1); // Output tab
        
        // Add safety check for wallpaper manager
        if (!m_wallpaperManager) {
            QString errorMsg = "Wallpaper manager is not initialized";
            qCCritical(mainWindow) << errorMsg;
            QMessageBox::critical(this, "Internal Error", errorMsg);
            return;
        }
        
        qCDebug(mainWindow) << "About to call wallpaper manager launch method";
        
        // Get custom settings from the properties panel
        QStringList additionalArgs;
        if (m_propertiesPanel) {
            // Create temporary file to load settings
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
            settingsPath += "/settings/" + wallpaper.id + ".json";
            
            // Check if settings file exists
            QFile settingsFile(settingsPath);
            if (settingsFile.exists() && settingsFile.open(QIODevice::ReadOnly)) {
                // Parse settings
                QJsonDocument doc = QJsonDocument::fromJson(settingsFile.readAll());
                settingsFile.close();
                
                if (doc.isObject()) {
                    QJsonObject settings = doc.object();
                    
                    // Add command line args based on settings
                    if (settings["silent"].toBool()) additionalArgs << "--silent";
                    
                    int volume = settings["volume"].toInt(15);
                    if (volume != 15) additionalArgs << "--volume" << QString::number(volume);
                    
                    if (settings["noAutoMute"].toBool()) additionalArgs << "--noautomute";
                    if (settings["noAudioProcessing"].toBool()) additionalArgs << "--no-audio-processing";
                    
                    int fps = settings["fps"].toInt(30);
                    if (fps != 30) additionalArgs << "--fps" << QString::number(fps);
                    
                    QString windowGeometry = settings["windowGeometry"].toString();
                    if (!windowGeometry.isEmpty()) additionalArgs << "--window" << windowGeometry;
                    
                    QString screenRoot = settings["screenRoot"].toString();
                    if (!screenRoot.isEmpty()) {
                        additionalArgs << "--screen-root" << screenRoot;
                        
                        QString backgroundId = settings["backgroundId"].toString();
                        if (!backgroundId.isEmpty()) additionalArgs << "--bg" << backgroundId;
                    }
                    
                    QString scaling = settings["scaling"].toString();
                    if (scaling != "default") additionalArgs << "--scaling" << scaling;
                    
                    QString clamping = settings["clamping"].toString();
                    if (clamping != "clamp") additionalArgs << "--clamping" << clamping;
                    
                    if (settings["disableMouse"].toBool()) additionalArgs << "--disable-mouse";
                    if (settings["disableParallax"].toBool()) additionalArgs << "--disable-parallax";
                    if (settings["noFullscreenPause"].toBool()) additionalArgs << "--no-fullscreen-pause";
                }
            } else {
                // No settings file exists, apply default values that are shown in GUI
                // This ensures the displayed defaults are actually applied on first launch
                
                // Volume default is 15% - apply it explicitly
                additionalArgs << "--volume" << "15";
                
                // FPS default is 30 - apply it explicitly  
                additionalArgs << "--fps" << "30";
                
                // Screen root default is HDMI-A-1 - apply it explicitly
                additionalArgs << "--screen-root" << "HDMI-A-1";
                
                // Other defaults (silent=false, scaling=default, clamping=clamp) don't need explicit args
                // as they are the wallpaper engine's own defaults
            }
        }
        
        // Add assets directory if configured
        QString assetsDir = ConfigManager::instance().getAssetsDir();
        if (!assetsDir.isEmpty()) {
            additionalArgs << "--assets-dir" << assetsDir;
        }
        
        // Launch wallpaper with error handling and custom settings
        bool success = m_wallpaperManager->launchWallpaper(wallpaper.id, additionalArgs);
        
        qCDebug(mainWindow) << "Wallpaper manager launch result:" << success;
        
        if (success) {
            m_statusLabel->setText(QString("Launched: %1").arg(wallpaper.name));
            qCInfo(mainWindow) << "Successfully launched wallpaper:" << wallpaper.name;
        } else {
            QString errorMsg = QString("Failed to launch wallpaper: %1").arg(wallpaper.name);
            qCWarning(mainWindow) << errorMsg;
            QMessageBox::warning(this, "Launch Failed",
                errorMsg + "\n\nCheck the Output tab for details.");
            m_statusLabel->setText("Launch failed");
        }
        
    } catch (const std::exception& e) {
        QString errorMsg = QString("Exception occurred while launching wallpaper: %1").arg(e.what());
        qCCritical(mainWindow) << errorMsg;
        if (m_wallpaperManager) {
            emit m_wallpaperManager->outputReceived("ERROR: " + errorMsg);
        }
        QMessageBox::critical(this, "Launch Error", errorMsg);
        m_statusLabel->setText("Launch error");
    } catch (...) {
        QString errorMsg = "Unknown error occurred while launching wallpaper";
        qCCritical(mainWindow) << errorMsg;
        if (m_wallpaperManager) {
            emit m_wallpaperManager->outputReceived("ERROR: " + errorMsg);
        }
        QMessageBox::critical(this, "Launch Error", errorMsg);
        m_statusLabel->setText("Launch error");
    }
    
    qCDebug(mainWindow) << "onWallpaperLaunched - END:" << wallpaper.name;
}

void MainWindow::updateStatusBar()
{
    if (m_config.steamPath().isEmpty()) {
        m_statusLabel->setText("Steam path not configured");
    } else if (m_config.wallpaperEnginePath().isEmpty()) {
        m_statusLabel->setText("Wallpaper Engine binary not configured");
    } else {
        m_statusLabel->setText("Ready");
    }
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "About Wallpaper Engine GUI",
        "<h3>Wallpaper Engine GUI</h3>"
        "<p>Version 1.0.0</p>"
        "<p>A graphical user interface for linux-wallpaperengine, providing easy access "
        "to Steam Workshop wallpapers on Linux.</p>"
        "<p><b>Features:</b></p>"
        "<ul>"
        "<li>Automatic Steam installation detection</li>"
        "<li>Wallpaper preview and management</li>"
        "<li>Configurable rendering and audio settings</li>"
        "<li>Support for various wallpaper types</li>"
        "</ul>"
        "<p>Built with Qt6 and C++</p>"
        "<p><a href=\"https://github.com/Almamu/linux-wallpaperengine\">linux-wallpaperengine project</a></p>");
}

void MainWindow::onOutputReceived(const QString& output)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formattedOutput = QString("[%1] %2").arg(timestamp, output.trimmed());
    
    m_outputTextEdit->append(formattedOutput);
    
    // Auto-scroll to bottom
    QTextCursor cursor = m_outputTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_outputTextEdit->setTextCursor(cursor);
    
    // Force immediate update
    m_outputTextEdit->update();
    QApplication::processEvents();
    
    // Switch to output tab for important messages (errors, launches, warnings)
    // Don't switch for normal LOG messages which are just operational info
    if (output.contains("ERROR") || output.contains("FAILED") || output.contains("WARNING") ||
        output.contains("Launching") || output.contains("Command:") || 
        output.contains("process finished") || output.contains("Stopping")) {
        m_rightTabWidget->setCurrentIndex(1); // Output tab
    }
}

void MainWindow::clearOutput()
{
    m_outputTextEdit->clear();
    m_outputTextEdit->append(QString("[%1] Output cleared").arg(
        QDateTime::currentDateTime().toString("hh:mm:ss")));
}

void MainWindow::saveOutput()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Output Log",
        QString("wallpaperengine-log-%1.txt").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd-hhmmss")),
        "Text Files (*.txt);;All Files (*)");
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << m_outputTextEdit->toPlainText();
            m_statusLabel->setText("Log saved to: " + fileName);
        } else {
            QMessageBox::warning(this, "Save Failed", "Could not save log file: " + file.errorString());
        }
    }
}
