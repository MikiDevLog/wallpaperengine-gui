#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QTabWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QEvent>

class WallpaperPreview;
class PropertiesPanel;
class ConfigManager;
class WallpaperManager;
class WallpaperPlaylist;
class PlaylistPreview;
struct WallpaperInfo;

// Custom QTabWidget that accepts drops on tab buttons
class DropTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit DropTabWidget(QWidget* parent = nullptr);

signals:
    void wallpaperDroppedOnPlaylistTab(const QString& wallpaperId);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    // Launch source tracking enum
    enum class LaunchSource {
        Manual,           // User double-click, launch button, etc.
        Playlist,         // Playlist timer, next/previous buttons
        StartupRestore    // Application startup restoration
    };
    
    // Methods for system tray
    void setStartMinimized(bool minimized);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void openSettings();
    void refreshWallpapers();
    void showAbout();
    void onWallpaperSelected(const WallpaperInfo& wallpaper);
    void onWallpaperLaunched(const WallpaperInfo& wallpaper);
    void onWallpaperStopped();
    void onWallpaperSelectionRejected(const QString& wallpaperId);
    void onRefreshProgress(int current, int total);
    void onRefreshFinished();
    void checkFirstRun();
    void initializeWithValidConfig();
    void showConfigurationIssuesDialog(const QString& issues);
    void onOutputReceived(const QString& output);
    void clearOutput();
    void saveOutput();
    
    // Playlist slots
    void onAddToPlaylistClicked();
    void onRemoveFromPlaylistClicked();
    void onPlaylistWallpaperSelected(const QString& wallpaperId);
    void onRemoveFromPlaylistRequested(const QString& wallpaperId);
    void onWallpaperDroppedOnPlaylistTab(const QString& wallpaperId);
    
    // Main tab change handling for unsaved changes
    void onMainTabBarClicked(int index);
    
    // System tray slots
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void showWindow();
    void hideToTray();
    void quitApplication();

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void createCentralWidget();
    void loadSettings();
    void saveSettings();
    void updateStatusBar();
    void showFirstRunDialog();
    
    // System tray methods
    void setupSystemTray();
    void createTrayMenu();
    void updatePlaylistButtonStates();
    
    // Main tab unsaved changes handling
    bool handleMainTabClickWithUnsavedCheck(int index);
    
    // Launch helper method
    void launchWallpaperWithSource(const WallpaperInfo& wallpaper, LaunchSource source);

    // UI Components
    DropTabWidget* m_mainTabWidget;
    QSplitter *m_splitter;
    QTabWidget *m_rightTabWidget;
    WallpaperPreview *m_wallpaperPreview;
    PropertiesPanel *m_propertiesPanel;
    PlaylistPreview *m_playlistPreview;
    QTextEdit *m_outputTextEdit;
    
    // Playlist UI components
    QPushButton *m_addToPlaylistButton;
    QPushButton *m_removeFromPlaylistButton;
    
    // Menu and toolbar
    QAction *m_refreshAction;
    QAction *m_settingsAction;
    QAction *m_aboutAction;
    QAction *m_exitAction;
    
    // Status bar
    QLabel *m_statusLabel;
    QLabel *m_wallpaperCountLabel;
    QProgressBar *m_progressBar;
    
    // Output tab actions
    QPushButton *m_clearOutputButton;
    QPushButton *m_saveOutputButton;
    
    // Managers
    ConfigManager& m_config;
    WallpaperManager *m_wallpaperManager;
    WallpaperPlaylist *m_wallpaperPlaylist;
    
    // State
    QString m_currentWallpaperId;
    bool m_refreshing;
    bool m_isClosing;
    bool m_startMinimized;
    bool m_isLaunchingWallpaper; // Track when we're launching a new wallpaper
    LaunchSource m_lastLaunchSource; // Track the source of the last wallpaper launch
    
    // Main tab unsaved changes handling
    bool m_ignoreMainTabChange;
    
    // Playlist restoration state (for timing fix)
    bool m_pendingPlaylistRestore;
    QString m_pendingRestoreWallpaperId;
    bool m_pendingRestoreFromPlaylist;
    
    // System tray
    QSystemTrayIcon *m_systemTrayIcon;
    QMenu *m_trayMenu;
    QAction *m_showAction;
    QAction *m_hideAction;
    QAction *m_quitAction;
};

#endif // MAINWINDOW_H
