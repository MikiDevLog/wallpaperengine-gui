#include "MainWindow.h"
#include "../widgets/WallpaperPreview.h"
#include "PropertiesPanel.h"
#include "../widgets/PlaylistPreview.h"
#include "../playlist/WallpaperPlaylist.h"
#include "SettingsDialog.h"
#include "../core/ConfigManager.h"
#include "../core/WallpaperManager.h"
#include "../steam/SteamDetector.h"
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
#include <QSystemTrayIcon>
#include <QMouseEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QPixmap>
#include <QFile>
#include <QCheckBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDragMoveEvent>
#include <QWidget>

Q_LOGGING_CATEGORY(mainWindow, "app.mainwindow")

// DropTabWidget implementation
DropTabWidget::DropTabWidget(QWidget* parent) : QTabWidget(parent)
{
    setAcceptDrops(true);
    // Also set accept drops on the tab bar and enable hover events
    tabBar()->setAcceptDrops(true);
    tabBar()->setAttribute(Qt::WA_Hover, true);
}

void DropTabWidget::dragEnterEvent(QDragEnterEvent* event)
{
    qDebug() << "Drag enter event with formats:" << event->mimeData()->formats();
    if (event->mimeData()->hasFormat("application/x-wallpaper-id") || 
        event->mimeData()->hasText()) {
        qDebug() << "Drag enter: accepting wallpaper drag";
        event->acceptProposedAction();
    } else {
        qDebug() << "Drag enter: ignoring non-wallpaper drag";
        event->ignore();
    }
}

void DropTabWidget::dragMoveEvent(QDragMoveEvent* event)
{
    qDebug() << "Drag move event at position:" << event->position().toPoint();
    if (event->mimeData()->hasFormat("application/x-wallpaper-id") || 
        event->mimeData()->hasText()) {
        
        // Try multiple coordinate mapping approaches to handle different Qt themes
        QPoint eventPos = event->position().toPoint();
        
        // First approach: map from parent (tab widget) to tab bar
        QPoint tabBarPos1 = tabBar()->mapFromParent(eventPos);
        
        // Second approach: use tab bar geometry within the widget
        QRect tabBarGeometry = tabBar()->geometry();
        QPoint tabBarPos2 = eventPos - tabBarGeometry.topLeft();
        
        qDebug() << "Event position:" << eventPos;
        qDebug() << "Tab bar geometry:" << tabBarGeometry;
        qDebug() << "Mapped position 1 (mapFromParent):" << tabBarPos1;
        qDebug() << "Mapped position 2 (geometry offset):" << tabBarPos2;
        
        // Check both coordinate mappings
        QPoint positionsToCheck[] = {tabBarPos1, tabBarPos2, eventPos};
        const char* methodNames[] = {"mapFromParent", "geometry offset", "direct position"};
        
        for (int method = 0; method < 3; method++) {
            QPoint checkPos = positionsToCheck[method];
            qDebug() << "Checking with method" << methodNames[method] << "position:" << checkPos;
            
            for (int i = 0; i < count(); ++i) {
                QRect tabRect = tabBar()->tabRect(i);
                qDebug() << "Tab" << i << "rect:" << tabRect;
                if (tabRect.contains(checkPos)) {
                    qDebug() << "Hit detected with method" << methodNames[method] << "on tab" << i;
                    if (i == 1) { // Playlist tab
                        qDebug() << "Drag move: over playlist tab, accepting";
                        event->acceptProposedAction();
                        return;
                    } else {
                        qDebug() << "Drag move: over tab" << i << ", ignoring";
                        event->ignore();
                        return;
                    }
                }
            }
        }
        qDebug() << "Drag move: not over any tab with any method";
    }
    event->ignore();
}

void DropTabWidget::dropEvent(QDropEvent* event)
{
    qDebug() << "Drop event received at position:" << event->position().toPoint() << "with mime data formats:" << event->mimeData()->formats();
    
    if (event->mimeData()->hasFormat("application/x-wallpaper-id") || 
        event->mimeData()->hasText()) {
        qDebug() << "Drop event has wallpaper ID format";
        
        // Try multiple coordinate mapping approaches to handle different Qt themes
        QPoint eventPos = event->position().toPoint();
        
        // First approach: map from parent (tab widget) to tab bar
        QPoint tabBarPos1 = tabBar()->mapFromParent(eventPos);
        
        // Second approach: use tab bar geometry within the widget
        QRect tabBarGeometry = tabBar()->geometry();
        QPoint tabBarPos2 = eventPos - tabBarGeometry.topLeft();
        
        qDebug() << "Event position:" << eventPos;
        qDebug() << "Tab bar geometry:" << tabBarGeometry;
        qDebug() << "Mapped position 1 (mapFromParent):" << tabBarPos1;
        qDebug() << "Mapped position 2 (geometry offset):" << tabBarPos2;
        
        // Check both coordinate mappings
        QPoint positionsToCheck[] = {tabBarPos1, tabBarPos2, eventPos};
        const char* methodNames[] = {"mapFromParent", "geometry offset", "direct position"};
        
        for (int method = 0; method < 3; method++) {
            QPoint checkPos = positionsToCheck[method];
            qDebug() << "Checking with method" << methodNames[method] << "position:" << checkPos;
            
            for (int i = 0; i < count(); ++i) {
                QRect tabRect = tabBar()->tabRect(i);
                qDebug() << "Tab" << i << "rect:" << tabRect;
                if (tabRect.contains(checkPos)) {
                    qDebug() << "Hit detected with method" << methodNames[method] << "on tab" << i;
                    if (i == 1) { // Playlist tab
                        QString wallpaperId;
                        if (event->mimeData()->hasFormat("application/x-wallpaper-id")) {
                            wallpaperId = QString::fromUtf8(event->mimeData()->data("application/x-wallpaper-id"));
                        } else if (event->mimeData()->hasText()) {
                            wallpaperId = event->mimeData()->text();
                        }
                        qDebug() << "Dropping wallpaper with ID:" << wallpaperId << "on playlist tab";
                        emit wallpaperDroppedOnPlaylistTab(wallpaperId);
                        event->acceptProposedAction();
                        
                        // Switch to the playlist tab
                        setCurrentIndex(1);
                        return;
                    } else {
                        qDebug() << "Drop not on playlist tab, ignoring";
                        event->ignore();
                        return;
                    }
                }
            }
        }
        qDebug() << "Drop not on any tab with any method, ignoring";
    } else {
        qDebug() << "Drop event does not have wallpaper ID format";
    }
    event->ignore();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_mainTabWidget(nullptr)
    , m_splitter(nullptr)
    , m_wallpaperPreview(nullptr)
    , m_propertiesPanel(nullptr)
    , m_playlistPreview(nullptr)
    , m_addToPlaylistButton(nullptr)
    , m_removeFromPlaylistButton(nullptr)
    , m_refreshAction(nullptr)
    , m_settingsAction(nullptr)
    , m_aboutAction(nullptr)
    , m_exitAction(nullptr)
    , m_statusLabel(nullptr)
    , m_wallpaperCountLabel(nullptr)
    , m_progressBar(nullptr)
    , m_config(ConfigManager::instance())
    , m_wallpaperManager(new WallpaperManager(this))
    , m_wallpaperPlaylist(new WallpaperPlaylist(this))
    , m_refreshing(false)
    , m_isClosing(false)
    , m_startMinimized(false)
    , m_isLaunchingWallpaper(false)
    , m_lastLaunchSource(LaunchSource::Manual)
    , m_ignoreMainTabChange(false)
    , m_pendingPlaylistRestore(false)
    , m_pendingRestoreWallpaperId("")
    , m_pendingRestoreFromPlaylist(false)
    , m_systemTrayIcon(nullptr)
    , m_trayMenu(nullptr)
    , m_showAction(nullptr)
    , m_hideAction(nullptr)
    , m_quitAction(nullptr)
{
    qCDebug(mainWindow) << "=== MAINWINDOW CONSTRUCTOR START ===";
    setWindowTitle("Wallpaper Engine GUI");
    setWindowIcon(QIcon(":/icons/icons/wallpaper.png"));
    
    setupUI();
    setupSystemTray();
    loadSettings();
    
    // Check for first run after UI is set up
    QTimer::singleShot(100, this, &MainWindow::checkFirstRun);
}

MainWindow::~MainWindow()
{
    qCDebug(mainWindow) << "MainWindow destructor starting";
    
    // Set closing flag to prevent further operations
    m_isClosing = true;
    
    // Stop any running wallpapers first
    if (m_wallpaperManager) {
        m_wallpaperManager->stopWallpaper();
    }
    
    // Hide and cleanup system tray icon
    if (m_systemTrayIcon) {
        m_systemTrayIcon->hide();
        m_systemTrayIcon = nullptr;  // Will be deleted by Qt parent-child relationship
    }
    
    // Save settings after stopping wallpapers but before cleanup
    saveSettings();
    
    qCDebug(mainWindow) << "MainWindow destructor completed";
}

void MainWindow::setupUI()
{
    qCDebug(mainWindow) << "=== ENTERING setupUI() ===";
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    qCDebug(mainWindow) << "=== About to call createCentralWidget() ===";
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
    toolBar->setObjectName("MainToolBar");  // Set object name to avoid Qt warnings
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
    qCDebug(mainWindow) << "=== ENTERING createCentralWidget() ===";
    // Create main tab widget for "All Wallpapers" and "Wallpaper Playlist"
    m_mainTabWidget = new DropTabWidget;
    setCentralWidget(m_mainTabWidget);
    
    // Install event filter on the main tab bar to intercept clicks for unsaved changes
    m_mainTabWidget->tabBar()->installEventFilter(this);
    
    // Connect drop signal
    connect(m_mainTabWidget, &DropTabWidget::wallpaperDroppedOnPlaylistTab,
            this, &MainWindow::onWallpaperDroppedOnPlaylistTab);
    
    // Create "All Wallpapers" tab
    QWidget* allWallpapersTab = new QWidget;
    QHBoxLayout* allWallpapersLayout = new QHBoxLayout(allWallpapersTab);
    allWallpapersLayout->setContentsMargins(0, 0, 0, 0);
    
    m_splitter = new QSplitter(Qt::Horizontal);
    allWallpapersLayout->addWidget(m_splitter);

    // left: wallpaper preview with playlist buttons
    QWidget* leftWidget = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(5, 5, 5, 5);
    leftLayout->setSpacing(5);
    
    m_wallpaperPreview = new WallpaperPreview;
    leftLayout->addWidget(m_wallpaperPreview, 1);
    
    // Add playlist control buttons
    QHBoxLayout* playlistButtonsLayout = new QHBoxLayout;
    playlistButtonsLayout->setContentsMargins(0, 0, 0, 0);
    
    m_addToPlaylistButton = new QPushButton("Add to Playlist");
    m_addToPlaylistButton->setEnabled(false);
    m_removeFromPlaylistButton = new QPushButton("Remove from Playlist");
    m_removeFromPlaylistButton->setEnabled(false);
    
    playlistButtonsLayout->addWidget(m_addToPlaylistButton);
    playlistButtonsLayout->addWidget(m_removeFromPlaylistButton);
    playlistButtonsLayout->addStretch();
    
    leftLayout->addLayout(playlistButtonsLayout);
    m_splitter->addWidget(leftWidget);

    // connect preview to manager so grid updates
    m_wallpaperPreview->setWallpaperManager(m_wallpaperManager);
    
    // connect playlist to manager so it can launch wallpapers
    m_wallpaperPlaylist->setWallpaperManager(m_wallpaperManager);

    // right: properties panel with 4 tabs
    m_propertiesPanel = new PropertiesPanel;
    m_splitter->addWidget(m_propertiesPanel);

    // connect properties panel to manager for automatic restart functionality
    m_propertiesPanel->setWallpaperManager(m_wallpaperManager);

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
    
    // Add the "All Wallpapers" tab
    m_mainTabWidget->addTab(allWallpapersTab, "All Wallpapers");
    
    // Load playlist from config before creating preview
    qCDebug(mainWindow) << "MainWindow::createCentralWidget() - About to load playlist from config";
    m_wallpaperPlaylist->loadFromConfig();
    qCDebug(mainWindow) << "MainWindow::createCentralWidget() - Playlist loaded, about to create PlaylistPreview";
    
    // Create "Wallpaper Playlist" tab
    m_playlistPreview = new PlaylistPreview(m_wallpaperPlaylist, m_wallpaperManager);
    qCDebug(mainWindow) << "MainWindow::createCentralWidget() - PlaylistPreview created successfully";
    m_mainTabWidget->addTab(m_playlistPreview, "Wallpaper Playlist");
    qCDebug(mainWindow) << "MainWindow::createCentralWidget() - PlaylistPreview added to tab widget";

    // Connect playlist button signals
    connect(m_addToPlaylistButton, &QPushButton::clicked, this, &MainWindow::onAddToPlaylistClicked);
    connect(m_removeFromPlaylistButton, &QPushButton::clicked, this, &MainWindow::onRemoveFromPlaylistClicked);
    
    // Connect playlist preview signals
    connect(m_playlistPreview, &PlaylistPreview::wallpaperSelected, this, &MainWindow::onPlaylistWallpaperSelected);
    connect(m_playlistPreview, &PlaylistPreview::removeFromPlaylistRequested, this, &MainWindow::onRemoveFromPlaylistRequested);

    // preview → selection
    connect(m_wallpaperPreview, &WallpaperPreview::wallpaperSelected,
            this, &MainWindow::onWallpaperSelected);
    connect(m_wallpaperPreview, &WallpaperPreview::wallpaperDoubleClicked,
            this, [this](const WallpaperInfo& wallpaper) {
                launchWallpaperWithSource(wallpaper, LaunchSource::Manual);
            });

    // properties panel → launch
    connect(m_propertiesPanel, &PropertiesPanel::launchWallpaper,
            this, [this](const WallpaperInfo& wallpaper) {
                launchWallpaperWithSource(wallpaper, LaunchSource::Manual);
            });

    // properties panel → wallpaper selection rejected due to unsaved changes
    connect(m_propertiesPanel, &PropertiesPanel::wallpaperSelectionRejected,
            this, &MainWindow::onWallpaperSelectionRejected);

    // wallpaper manager → output log
    connect(m_wallpaperManager, &WallpaperManager::outputReceived,
            this, &MainWindow::onOutputReceived);
    
    // wallpaper manager → clear last wallpaper on stop
    connect(m_wallpaperManager, &WallpaperManager::wallpaperStopped,
            this, &MainWindow::onWallpaperStopped);

    // playlist → launch wallpaper with proper source tracking
    connect(m_wallpaperPlaylist, &WallpaperPlaylist::playlistLaunchRequested,
            this, [this](const QString& wallpaperId, const QStringList& args) {
                Q_UNUSED(args) // Args already handled in playlist settings loading
                // Find wallpaper info and launch with playlist source
                if (m_wallpaperManager) {
                    auto wallpaperInfo = m_wallpaperManager->getWallpaperInfo(wallpaperId);
                    if (wallpaperInfo.has_value()) {
                        launchWallpaperWithSource(wallpaperInfo.value(), LaunchSource::Playlist);
                    }
                }
            });

    // initial splitter sizing
    m_splitter->setSizes({840, 360});
}

void MainWindow::loadSettings()
{
    // Restore window geometry - but ensure window remains resizable
    QByteArray savedGeometry = m_config.windowGeometry();
    if (!savedGeometry.isEmpty()) {
        restoreGeometry(savedGeometry);
        
        // Explicitly ensure the window can be resized after geometry restoration
        setMinimumSize(400, 300);  // Set reasonable minimum size
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);  // Remove any maximum size constraints
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }
    
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
    // If system tray is available and enabled, minimize to tray instead of closing
    if (m_systemTrayIcon && m_systemTrayIcon->isVisible()) {
        if (isVisible()) {
            // Check if user wants to see the tray warning
            if (m_config.showTrayWarning()) {
                QMessageBox msgBox(this);
                msgBox.setWindowTitle("Wallpaper Engine GUI");
                msgBox.setIcon(QMessageBox::Information);
                msgBox.setText("The application was minimized to the system tray.");
                msgBox.setInformativeText("To restore the window, click the tray icon or use the context menu.");
                
                // Add "Don't warn me again" checkbox
                QCheckBox *dontWarnCheckBox = new QCheckBox("Don't warn me again");
                msgBox.setCheckBox(dontWarnCheckBox);
                
                msgBox.setStandardButtons(QMessageBox::Ok);
                msgBox.exec();
                
                // Save the preference if user checked the box
                if (dontWarnCheckBox->isChecked()) {
                    m_config.setShowTrayWarning(false);
                    qCInfo(mainWindow) << "User disabled tray warning notifications";
                }
            }
            
            hideToTray();
            event->ignore();
            return;
        }
    }
    
    // Normal application exit
    m_isClosing = true;
    saveSettings();
    event->accept();
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized() && m_systemTrayIcon && m_systemTrayIcon->isVisible()) {
            hideToTray();
            event->ignore();
            return;
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::setStartMinimized(bool minimized)
{
    m_startMinimized = minimized;
    if (minimized && m_systemTrayIcon && m_systemTrayIcon->isVisible()) {
        QTimer::singleShot(100, this, &MainWindow::hideToTray);
    }
}

void MainWindow::setupSystemTray()
{
    // Check if system tray is available
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qCWarning(mainWindow) << "System tray is not available on this system";
        return;
    }
    
    // Create system tray icon
    m_systemTrayIcon = new QSystemTrayIcon(this);
    
    // Use the wallpaper.png icon from resources
    QIcon trayIcon(":/icons/icons/wallpaper.png");
    
    // Debug: Check if icon loaded successfully
    qCInfo(mainWindow) << "Attempting to load system tray icon from resources: :/icons/icons/wallpaper.png";
    qCInfo(mainWindow) << "Icon is null:" << trayIcon.isNull();
    qCInfo(mainWindow) << "Available icon sizes:" << trayIcon.availableSizes();
    
    // Check if icon loaded properly by testing available sizes
    if (trayIcon.isNull() || trayIcon.availableSizes().isEmpty()) {
        qCWarning(mainWindow) << "Resource icon failed to load, trying window icon fallback";
        trayIcon = windowIcon();
        
        if (trayIcon.isNull() || trayIcon.availableSizes().isEmpty()) {
            qCWarning(mainWindow) << "Window icon also failed, creating fallback icon";
            // Fallback to a simple colored circle if no icon is available
            QPixmap pixmap(22, 22);  // Slightly larger for better visibility
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(QColor(52, 152, 219)); // Nice blue color
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(3, 3, 16, 16);
            trayIcon = QIcon(pixmap);
            qCInfo(mainWindow) << "Created fallback blue circle icon";
        } else {
            qCInfo(mainWindow) << "Using window icon for system tray";
        }
    } else {
        qCInfo(mainWindow) << "Successfully loaded wallpaper.png icon for system tray";
    }
    
    m_systemTrayIcon->setIcon(trayIcon);
    
    // Create tray menu
    createTrayMenu();
    
    // Set tooltip
    m_systemTrayIcon->setToolTip("Wallpaper Engine GUI");
    
    // Connect signals
    connect(m_systemTrayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayIconActivated);
    
    // Show the tray icon
    m_systemTrayIcon->show();
    
    qCInfo(mainWindow) << "System tray icon initialized successfully";
}

void MainWindow::createTrayMenu()
{
    m_trayMenu = new QMenu(this);
    
    // Show/Hide action
    m_showAction = new QAction("Show Window", this);
    connect(m_showAction, &QAction::triggered, this, &MainWindow::showWindow);
    m_trayMenu->addAction(m_showAction);
    
    m_hideAction = new QAction("Hide Window", this);
    connect(m_hideAction, &QAction::triggered, this, &MainWindow::hideToTray);
    m_trayMenu->addAction(m_hideAction);
    
    m_trayMenu->addSeparator();
    
    // Add some useful actions
    QAction *refreshAction = new QAction("Refresh Wallpapers", this);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshWallpapers);
    m_trayMenu->addAction(refreshAction);
    
    QAction *settingsAction = new QAction("Settings", this);
    connect(settingsAction, &QAction::triggered, this, [this]() {
        showWindow();
        openSettings();
    });
    m_trayMenu->addAction(settingsAction);
    
    m_trayMenu->addSeparator();
    
    // Quit action
    m_quitAction = new QAction("Quit", this);
    connect(m_quitAction, &QAction::triggered, this, &MainWindow::quitApplication);
    m_trayMenu->addAction(m_quitAction);
    
    // Set the menu
    m_systemTrayIcon->setContextMenu(m_trayMenu);
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        if (isVisible() && !isMinimized()) {
            hideToTray();
        } else {
            showWindow();
        }
        break;
    case QSystemTrayIcon::MiddleClick:
        showWindow();
        break;
    default:
        break;
    }
}

void MainWindow::showWindow()
{
    show();
    raise();
    activateWindow();
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    
    // Update menu actions
    if (m_showAction && m_hideAction) {
        m_showAction->setEnabled(false);
        m_hideAction->setEnabled(true);
    }
    
    qCDebug(mainWindow) << "Window restored from system tray";
}

void MainWindow::hideToTray()
{
    hide();
    
    // Update menu actions
    if (m_showAction && m_hideAction) {
        m_showAction->setEnabled(true);
        m_hideAction->setEnabled(false);
    }
    
    qCDebug(mainWindow) << "Window hidden to system tray";
}

void MainWindow::quitApplication()
{
    qCDebug(mainWindow) << "quitApplication() called";
    m_isClosing = true;
    
    // Stop any running wallpapers
    if (m_wallpaperManager) {
        m_wallpaperManager->stopWallpaper();
    }
    
    // Hide tray icon before quitting
    if (m_systemTrayIcon) {
        m_systemTrayIcon->hide();
    }
    
    // Save settings
    saveSettings();
    
    // Close application
    QApplication::quit();
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
    
    // Auto-restore last wallpaper or playlist state
    QString lastWallpaper = m_config.lastSelectedWallpaper();
    bool lastSessionUsedPlaylist = m_config.lastSessionUsedPlaylist();
    qCDebug(mainWindow) << "Checking for last state to restore. Wallpaper:" << (lastWallpaper.isEmpty() ? "NONE" : lastWallpaper) 
                       << "Used playlist:" << lastSessionUsedPlaylist;
    
    // Check if we need to restore state (either specific wallpaper or playlist-only)
    if (!lastWallpaper.isEmpty() || lastSessionUsedPlaylist) {
        if (!lastWallpaper.isEmpty()) {
            qCInfo(mainWindow) << "Will restore last wallpaper:" << lastWallpaper << "from" << (lastSessionUsedPlaylist ? "playlist" : "individual selection");
        } else if (lastSessionUsedPlaylist) {
            qCInfo(mainWindow) << "Will restore playlist playback (no specific wallpaper ID saved)";
        }
        
        // Store restoration state to be processed after WallpaperManager::refreshFinished signal
        m_pendingPlaylistRestore = true;
        m_pendingRestoreWallpaperId = lastWallpaper; // May be empty for playlist-only restoration
        m_pendingRestoreFromPlaylist = lastSessionUsedPlaylist;
        
        qCDebug(mainWindow) << "Restoration state stored, will restore after wallpapers are loaded";
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
    
    // Handle pending playlist restoration after wallpapers are loaded (timing fix)
    if (m_pendingPlaylistRestore) {
        qCDebug(mainWindow) << "Processing pending playlist restoration. Wallpaper ID:" << (m_pendingRestoreWallpaperId.isEmpty() ? "NONE" : m_pendingRestoreWallpaperId) 
                           << "From playlist:" << m_pendingRestoreFromPlaylist;
        
        if (m_pendingRestoreFromPlaylist && m_wallpaperPlaylist) {
            // Check if playlist is enabled and has wallpapers
            PlaylistSettings playlistSettings = m_wallpaperPlaylist->getSettings();
            if (playlistSettings.enabled && m_wallpaperPlaylist->size() > 0) {
                qCInfo(mainWindow) << "Restoring playlist playback";
                
                // Switch to playlist tab to show the playlist is active
                m_mainTabWidget->setCurrentIndex(1); // Wallpaper Playlist tab
                
                // Start playlist playback (this will launch the current/first wallpaper)
                qCDebug(mainWindow) << "Calling m_wallpaperPlaylist->startPlayback()";
                m_wallpaperPlaylist->startPlayback();
                
                // Update status
                m_statusLabel->setText("Restored playlist playback");
            } else {
                qCWarning(mainWindow) << "Playlist was used last session but is now disabled or empty";
                // Clear the playlist usage from config since it's no longer valid
                m_config.setLastSessionUsedPlaylist(false);
            }
        } else if (!m_pendingRestoreWallpaperId.isEmpty()) {
            // Find the wallpaper by ID for individual wallpaper restoration
            WallpaperInfo wallpaperToRestore = m_wallpaperManager->getWallpaperById(m_pendingRestoreWallpaperId);
            if (!wallpaperToRestore.id.isEmpty()) {
                qCInfo(mainWindow) << "Found wallpaper to restore:" << wallpaperToRestore.name;
                
                // Both playlist and manual wallpapers should auto-launch + be visually selected on startup
                qCInfo(mainWindow) << "Restoring wallpaper with auto-launch:" << wallpaperToRestore.name 
                                   << "(from" << (m_pendingRestoreFromPlaylist ? "playlist" : "manual launch") << ")";
                
                // Launch the wallpaper automatically (this will mark it as startup restoration)
                launchWallpaperWithSource(wallpaperToRestore, LaunchSource::StartupRestore);
                
                // Update the UI to show the selected wallpaper (with pagination navigation and scrolling)
                // This needs to happen after launch because the grid gets refreshed after wallpaper manager signals
                QTimer::singleShot(200, this, [this, wallpaperToRestore]() {
                    if (m_wallpaperPreview) {
                        qCDebug(mainWindow) << "Selecting restored wallpaper in grid:" << wallpaperToRestore.name;
                        m_wallpaperPreview->selectWallpaper(wallpaperToRestore.id);
                    }
                    
                    // Update properties panel to show the wallpaper details
                    if (m_propertiesPanel) {
                        m_propertiesPanel->setWallpaper(wallpaperToRestore);
                    }
                    
                    // Update playlist button states for the restored wallpaper
                    updatePlaylistButtonStates();
                });
                
                // Update status to indicate the wallpaper was launched
                m_statusLabel->setText(QString("Restored: %1").arg(wallpaperToRestore.name));
            } else {
                qCWarning(mainWindow) << "Could not find wallpaper with ID:" << m_pendingRestoreWallpaperId;
                // Clear the invalid wallpaper ID from config
                m_config.setLastSelectedWallpaper("");
                m_config.setLastSessionUsedPlaylist(false);
            }
        }
        
        // Clear the pending restoration state
        m_pendingPlaylistRestore = false;
        m_pendingRestoreWallpaperId.clear();
        m_pendingRestoreFromPlaylist = false;
    }
}

void MainWindow::onWallpaperSelected(const WallpaperInfo& wallpaper)
{
    qCDebug(mainWindow) << "onWallpaperSelected - START:" << wallpaper.name;
    
    try {
        if (!wallpaper.id.isEmpty()) {
            qCDebug(mainWindow) << "Setting wallpaper on properties panel";
            m_propertiesPanel->setWallpaper(wallpaper);
            m_statusLabel->setText(QString("Selected: %1").arg(wallpaper.name));
            
            // Update playlist button states
            updatePlaylistButtonStates();
        } else {
            qCDebug(mainWindow) << "Clearing properties panel";
            m_propertiesPanel->clear();
            m_statusLabel->setText("Ready");
            
            // Disable playlist buttons when no wallpaper is selected
            m_addToPlaylistButton->setEnabled(false);
            m_removeFromPlaylistButton->setEnabled(false);
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
    
    // Set flag to indicate we're launching a wallpaper (prevents clearing last selected wallpaper on stop)
    m_isLaunchingWallpaper = true;
    
    try {
        if (m_config.wallpaperEnginePath().isEmpty()) {
            qCWarning(mainWindow) << "Wallpaper Engine binary path not configured";
            QMessageBox::warning(this, "Wallpaper Engine Not Configured",
                "Please configure the path to linux-wallpaperengine binary in Settings first.");
            openSettings();
            return;
        }
        
        qCDebug(mainWindow) << "Binary path configured";
        // Note: Tab switching removed - user requested to stay on current tab when launching wallpaper
        // m_rightTabWidget->setCurrentIndex(3); // Would switch to Engine Log tab
        
        // Add safety check for wallpaper manager
        if (!m_wallpaperManager) {
            QString errorMsg = "Wallpaper manager is not initialized";
            qCCritical(mainWindow) << errorMsg;
            QMessageBox::critical(this, "Internal Error", errorMsg);
            return;
        }
        
        // Track if this launch is from playlist based on the explicitly set launch source
        bool launchedFromPlaylist = (m_lastLaunchSource == LaunchSource::Playlist);
        
        qCDebug(mainWindow) << "Launch source:" << static_cast<int>(m_lastLaunchSource) 
                           << "-> Launch from playlist:" << launchedFromPlaylist;
        
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
            
            // Check if launched wallpaper is in playlist and manage playlist accordingly
            if (m_wallpaperPlaylist) {
                bool wallpaperInPlaylist = m_wallpaperPlaylist->containsWallpaper(wallpaper.id);
                PlaylistSettings playlistSettings = m_wallpaperPlaylist->getSettings();
                
                qCDebug(mainWindow) << "Wallpaper in playlist:" << wallpaperInPlaylist 
                                   << "Playlist enabled:" << playlistSettings.enabled;
                
                if (wallpaperInPlaylist) {
                    // Wallpaper IS in playlist - ensure playlist is running
                    if (!playlistSettings.enabled) {
                        qCInfo(mainWindow) << "Starting playlist - launched wallpaper is in playlist:" << wallpaper.id;
                        m_wallpaperPlaylist->setEnabled(true);
                    } else {
                        qCDebug(mainWindow) << "Playlist continues - launched wallpaper is in playlist:" << wallpaper.id;
                    }
                } else {
                    // Wallpaper is NOT in playlist - stop the playlist if it's running
                    if (playlistSettings.enabled) {
                        qCInfo(mainWindow) << "Stopping playlist - launched wallpaper not in playlist:" << wallpaper.id;
                        m_wallpaperPlaylist->setEnabled(false);
                    } else {
                        qCDebug(mainWindow) << "Playlist already stopped - launched wallpaper not in playlist:" << wallpaper.id;
                    }
                }
            }
            
            // Save configuration based on launch source
            if (launchedFromPlaylist) {
                // Playlist launch: clear individual wallpaper ID and mark as playlist session
                qCDebug(mainWindow) << "Playlist launch - clearing last wallpaper and marking as playlist session";
                m_config.setLastSelectedWallpaper("");
                m_config.setLastSessionUsedPlaylist(true);
            } else if (m_lastLaunchSource == LaunchSource::StartupRestore) {
                // Startup restoration: preserve the existing configuration (don't change it)
                qCDebug(mainWindow) << "Startup restoration - preserving existing configuration";
                // Don't change the config during startup restoration
            } else {
                // Manual launch: save wallpaper ID and mark as individual session
                qCDebug(mainWindow) << "Manual launch - saving wallpaper ID:" << wallpaper.id;
                m_config.setLastSelectedWallpaper(wallpaper.id);
                m_config.setLastSessionUsedPlaylist(false);
            }
            
            // Verify it was saved
            QString verification = m_config.lastSelectedWallpaper();
            bool playlistVerification = m_config.lastSessionUsedPlaylist();
            qCDebug(mainWindow) << "Configuration saved - wallpaper ID:" << verification 
                               << "playlist session:" << playlistVerification;
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

void MainWindow::launchWallpaperWithSource(const WallpaperInfo& wallpaper, LaunchSource source)
{
    qCDebug(mainWindow) << "launchWallpaperWithSource called with source:" << static_cast<int>(source) << "wallpaper:" << wallpaper.name;
    m_lastLaunchSource = source;
    onWallpaperLaunched(wallpaper);
}

void MainWindow::onWallpaperStopped()
{
    qCDebug(mainWindow) << "Wallpaper stopped - isClosing:" << m_isClosing << "isLaunchingWallpaper:" << m_isLaunchingWallpaper;
    
    // Only clear the last selected wallpaper if this is a manual stop (user clicked stop button)
    // NOT when application is closing or when launching a new wallpaper (which stops the previous one)
    if (!m_isClosing && !m_isLaunchingWallpaper) {
        qCDebug(mainWindow) << "Manual stop - clearing last selected wallpaper";
        m_config.setLastSelectedWallpaper("");
    } else {
        qCDebug(mainWindow) << "Wallpaper stopped but not clearing last selected wallpaper (closing:" << m_isClosing << ", launching:" << m_isLaunchingWallpaper << ")";
    }
    
    // Reset the launching flag
    m_isLaunchingWallpaper = false;
    
    // Update status
    m_statusLabel->setText("Wallpaper stopped");
}

void MainWindow::onWallpaperSelectionRejected(const QString& wallpaperId)
{
    qCDebug(mainWindow) << "Wallpaper selection rejected due to unsaved changes, reverting to:" << wallpaperId;
    
    // Revert the wallpaper selection in the preview to the wallpaper that the user wants to keep
    if (m_wallpaperPreview) {
        m_wallpaperPreview->selectWallpaper(wallpaperId);
    }
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
        "<p>Version 1.1.0</p>"
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
        m_rightTabWidget->setCurrentIndex(0); // Output tab
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

// Event filter to intercept main tab clicks for unsaved changes handling
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_mainTabWidget->tabBar() && 
        event->type() == QEvent::MouseButtonPress) {
        
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            // Get the tab index at the click position
            QTabBar *tabBar = m_mainTabWidget->tabBar();
            int clickedIndex = tabBar->tabAt(mouseEvent->pos());
            
            if (clickedIndex >= 0) {
                return handleMainTabClickWithUnsavedCheck(clickedIndex);
            }
        }
    }
    
    return QMainWindow::eventFilter(watched, event);
}

// Handle main tab click with unsaved changes check
bool MainWindow::handleMainTabClickWithUnsavedCheck(int index)
{
    if (m_ignoreMainTabChange) {
        // Allow the tab change to proceed without checking
        return false; // Don't consume the event
    }
    
    int currentIndex = m_mainTabWidget->currentIndex();
    
    // If clicking the same tab, no need to check
    if (currentIndex == index) {
        return false; // Don't consume the event
    }
    
    // Check if the properties panel has unsaved changes
    bool hasUnsaved = false;
    if (m_propertiesPanel) {
        hasUnsaved = m_propertiesPanel->hasUnsavedChanges();
    }
    
    if (hasUnsaved) {
        // Show confirmation dialog using the PropertiesPanel's dialog
        if (m_propertiesPanel->showUnsavedChangesDialog()) {
            // User chose to discard changes
            m_propertiesPanel->resetUnsavedChanges();
            
            // Allow the tab change by programmatically setting it
            m_ignoreMainTabChange = true;
            m_mainTabWidget->setCurrentIndex(index);
            m_ignoreMainTabChange = false;
        }
        // If user chose to stay, consume the event to prevent the tab change
        return true; // Consume the event to prevent default behavior
    } else {
        // No unsaved changes, allow the click to proceed normally
        return false; // Don't consume the event
    }
}

// Legacy method for compatibility - no longer used
void MainWindow::onMainTabBarClicked(int index)
{
    // This method is no longer used since we're using event filtering
    Q_UNUSED(index)
}

// Playlist-related slot implementations
void MainWindow::onAddToPlaylistClicked()
{
    if (!m_wallpaperPreview || !m_wallpaperPlaylist) {
        return;
    }
    
    QString selectedWallpaperId = m_wallpaperPreview->getSelectedWallpaperId();
    if (selectedWallpaperId.isEmpty()) {
        QMessageBox::information(this, "Add to Playlist", "Please select a wallpaper first.");
        return;
    }
    
    if (m_wallpaperPlaylist->containsWallpaper(selectedWallpaperId)) {
        QMessageBox::information(this, "Add to Playlist", "This wallpaper is already in the playlist.");
        return;
    }
    
    m_wallpaperPlaylist->addWallpaper(selectedWallpaperId);
    
    // Update button states
    updatePlaylistButtonStates();
    
    m_statusLabel->setText("Wallpaper added to playlist");
}

void MainWindow::onRemoveFromPlaylistClicked()
{
    if (!m_wallpaperPreview || !m_wallpaperPlaylist) {
        return;
    }
    
    QString selectedWallpaperId = m_wallpaperPreview->getSelectedWallpaperId();
    if (selectedWallpaperId.isEmpty()) {
        QMessageBox::information(this, "Remove from Playlist", "Please select a wallpaper first.");
        return;
    }
    
    if (!m_wallpaperPlaylist->containsWallpaper(selectedWallpaperId)) {
        QMessageBox::information(this, "Remove from Playlist", "This wallpaper is not in the playlist.");
        return;
    }
    
    auto result = QMessageBox::question(this, "Remove from Playlist", 
        "Are you sure you want to remove this wallpaper from the playlist?");
    
    if (result == QMessageBox::Yes) {
        m_wallpaperPlaylist->removeWallpaper(selectedWallpaperId);
        
        // Update button states
        updatePlaylistButtonStates();
        
        m_statusLabel->setText("Wallpaper removed from playlist");
    }
}

void MainWindow::onPlaylistWallpaperSelected(const QString& wallpaperId)
{
    if (!m_wallpaperManager) {
        return;
    }
    
    // Find wallpaper info and update properties panel
    auto wallpaperInfo = m_wallpaperManager->getWallpaperInfo(wallpaperId);
    if (wallpaperInfo.has_value()) {
        // Convert to the WallpaperInfo struct format expected by onWallpaperSelected
        WallpaperInfo info = wallpaperInfo.value();
        onWallpaperSelected(info);
        
        // Also update the wallpaper selection in WallpaperPreview to sync visual selection
        if (m_wallpaperPreview) {
            m_wallpaperPreview->selectWallpaper(wallpaperId);
        }
        
        // Switch to "All Wallpapers" tab to show details
        m_mainTabWidget->setCurrentIndex(0);
    }
}

void MainWindow::onRemoveFromPlaylistRequested(const QString& wallpaperId)
{
    if (!m_wallpaperPlaylist) {
        return;
    }
    
    auto result = QMessageBox::question(this, "Remove from Playlist", 
        "Are you sure you want to remove this wallpaper from the playlist?");
    
    if (result == QMessageBox::Yes) {
        m_wallpaperPlaylist->removeWallpaper(wallpaperId);
        m_statusLabel->setText("Wallpaper removed from playlist");
    }
}

void MainWindow::onWallpaperDroppedOnPlaylistTab(const QString& wallpaperId)
{
    if (!m_wallpaperPlaylist) {
        return;
    }
    
    if (m_wallpaperPlaylist->containsWallpaper(wallpaperId)) {
        QMessageBox::information(this, "Add to Playlist", "This wallpaper is already in the playlist.");
        return;
    }
    
    m_wallpaperPlaylist->addWallpaper(wallpaperId);
    
    // Update button states
    updatePlaylistButtonStates();
    
    m_statusLabel->setText("Wallpaper added to playlist via drag and drop");
}

void MainWindow::updatePlaylistButtonStates()
{
    if (!m_wallpaperPreview || !m_wallpaperPlaylist) {
        return;
    }
    
    QString selectedWallpaperId = m_wallpaperPreview->getSelectedWallpaperId();
    bool hasSelection = !selectedWallpaperId.isEmpty();
    bool isInPlaylist = hasSelection && m_wallpaperPlaylist->containsWallpaper(selectedWallpaperId);
    
    // Update the main playlist buttons using member variables
    if (m_addToPlaylistButton) {
        m_addToPlaylistButton->setEnabled(hasSelection && !isInPlaylist);
        m_addToPlaylistButton->setText(isInPlaylist ? "Already in Playlist" : "Add to Playlist");
    }
    
    if (m_removeFromPlaylistButton) {
        m_removeFromPlaylistButton->setEnabled(hasSelection && isInPlaylist);
        m_removeFromPlaylistButton->setText(isInPlaylist ? "Remove from Playlist" : "Remove from Playlist");
    }
    
    // Update any menu actions if they exist
    auto addToPlaylistAction = findChild<QAction*>("actionAddToPlaylist");
    if (addToPlaylistAction) {
        addToPlaylistAction->setEnabled(hasSelection && !isInPlaylist);
    }
    
    auto removeFromPlaylistAction = findChild<QAction*>("actionRemoveFromPlaylist");
    if (removeFromPlaylistAction) {
        removeFromPlaylistAction->setEnabled(hasSelection && isInPlaylist);
    }
}
