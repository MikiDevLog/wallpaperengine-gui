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

class WallpaperPreview;
class PropertiesPanel;
class ConfigManager;
class WallpaperManager;
struct WallpaperInfo;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    // Methods for system tray
    void setStartMinimized(bool minimized);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void openSettings();
    void refreshWallpapers();
    void showAbout();
    void onWallpaperSelected(const WallpaperInfo& wallpaper);
    void onWallpaperLaunched(const WallpaperInfo& wallpaper);
    void onWallpaperStopped();
    void onRefreshProgress(int current, int total);
    void onRefreshFinished();
    void checkFirstRun();
    void initializeWithValidConfig();
    void showConfigurationIssuesDialog(const QString& issues);
    void onOutputReceived(const QString& output);
    void clearOutput();
    void saveOutput();
    
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

    // UI Components
    QSplitter *m_splitter;
    QTabWidget *m_rightTabWidget;
    WallpaperPreview *m_wallpaperPreview;
    PropertiesPanel *m_propertiesPanel;
    QTextEdit *m_outputTextEdit;
    
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
    
    // State
    QString m_currentWallpaperId;
    bool m_refreshing;
    bool m_isClosing;
    bool m_startMinimized;
    
    // System tray
    QSystemTrayIcon *m_systemTrayIcon;
    QMenu *m_trayMenu;
    QAction *m_showAction;
    QAction *m_hideAction;
    QAction *m_quitAction;
};

#endif // MAINWINDOW_H
