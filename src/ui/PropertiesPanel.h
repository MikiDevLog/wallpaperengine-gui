#ifndef PROPERTIESPANEL_H
#define PROPERTIESPANEL_H

// Put all Qt includes first - before any usage of Qt types
#include <QWidget>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QScrollArea>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QPixmap>
#include <QMap>
#include <QVariant>
#include <QTabWidget>
#include <QTabBar>
#include <QMessageBox>
#include <QEvent>

// These were missing or in wrong order - add them before the WallpaperSettings struct
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QGroupBox>
#include <QProcess>
#include <QMovie>

#include "../core/WallpaperManager.h"
#include "../steam/SteamApiManager.h" // Add this include for WorkshopItemInfo type

// New struct to hold custom wallpaper settings
struct WallpaperSettings {
    // Audio settings
    bool silent = false;
    int volume = 15;
    bool noAutoMute = false;
    bool noAudioProcessing = false;

    // Performance settings
    int fps = 30;

    // Display settings
    QString windowGeometry = "";
    QString screenRoot = "";
    QString backgroundId = "";
    QString scaling = "default";
    QString clamping = "clamp";
    
    // Behavior settings
    bool disableMouse = false;
    bool disableParallax = false;
    bool noFullscreenPause = false;
    
    // WNEL-specific settings
    bool noLoop = false;
    bool noHardwareDecode = false;
    bool forceX11 = false;
    bool forceWayland = false;
    bool verbose = false;
    QString logLevel = "info";  // debug, info, warn, error
    QString mpvOptions = "";

    // Convert settings to command line arguments
    QStringList toCommandLineArgs() const;
};

class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget* parent = nullptr);
    
    void setWallpaper(const WallpaperInfo& wallpaper);
    void setWallpaperManager(WallpaperManager* manager);
    void clear();
    
    // Getter for current wallpaper info
    const WallpaperInfo& getCurrentWallpaper() const { return m_currentWallpaper; }
    
    // Unsaved changes handling - made public for MainWindow access
    bool hasUnsavedChanges() const;
    bool showUnsavedChangesDialog();
    void resetUnsavedChanges();
    
    // Tab interaction tracking
    bool isUserInteractingWithTabs() const { return m_userInteractingWithTabs; }

signals:
    void launchWallpaper(const WallpaperInfo& wallpaper);
    void propertiesChanged(const QString& wallpaperId, const QJsonObject& properties);
    void settingsChanged(const QString& wallpaperId, const WallpaperSettings& settings);
    void wallpaperSelectionRejected(const QString& wallpaperId);

private slots:
    void onPropertyChanged();
    void onSavePropertiesClicked();
    void onResetPropertiesClicked();
    void restartWallpaperWithChanges();
    void onUserProfileReceived(const QString& steamId, const SteamUserProfile& profile);
    // New slots for custom settings
    void onSettingChanged();
    void onSaveSettingsClicked();
    void onScreenRootChanged(const QString& screenRoot);
    
    // New slots for unsaved changes handling
    void onTabBarClicked(int index);
    void copyWallpaperIdToClipboard();
    void onSaveExternalNameClicked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUI();
    void updatePreview(const WallpaperInfo& wallpaper);
    void updateProperties(const QJsonObject& properties);
    void addPropertiesFromObject(QFormLayout* layout, const QJsonObject& properties, const QString& prefix);
    QWidget* createPropertyWidget(const QString& propName, const QString& type, const QJsonValue& value, const QJsonObject& propertyObj);
    QString formatFileSize(qint64 bytes);
    QPixmap scalePixmapKeepAspectRatio(const QPixmap& original, const QSize& targetSize);
    void setPlaceholderPreview(const QString& text);
    QJsonObject saveCurrentProperties();
        QJsonObject loadPropertiesFromProjectJson(const QString& wallpaperId);
    bool loadCachedProperties(const QString& wallpaperId);
    bool saveCachedProperties(const QString& wallpaperId, const QJsonObject& properties);
    bool savePropertiesToProjectJson(const QString& wallpaperId, const QJsonObject& properties);
    bool resetPropertiesFromBackup(const QString& wallpaperId);
    QString getProjectJsonPath(const QString& wallpaperId) const;
    QString getBackupProjectJsonPath(const QString& wallpaperId) const;
    QString getCacheFilePath(const QString& wallpaperId) const;  // Added const qualifier to match implementation
    
    // Modified method to take a settings tab as parameter
    void setupSettingsUI(QWidget* settingsTab);
    void setupExternalWallpaperUI();
    void updateSettingsControls();
    void updateWNELSettingsVisibility(bool isExternalWallpaper);
    bool loadWallpaperSettings(const QString& wallpaperId);
    bool saveWallpaperSettings(const QString& wallpaperId);
    QString getSettingsFilePath(const QString& wallpaperId);
    QStringList getAvailableScreens() const;
    
    // New methods for unsaved changes handling
    bool checkUnsavedChangesBeforeAction();
    bool handleTabClickWithUnsavedCheck(int index);
    
    // UI Components - reordered to match constructor initialization order
    QLabel* m_nameLabel;
    QLabel* m_authorLabel;
    QLabel* m_typeLabel;
    QLabel* m_fileSizeLabel;
    QLabel* m_postedLabel;
    QLabel* m_updatedLabel;
    QLabel* m_viewsLabel;
    QLabel* m_subscriptionsLabel;
    QLabel* m_favoritesLabel;
    QLabel* m_previewLabel;
    QLabel* m_previewIdLabel;  // Displays current wallpaper ID
    QPushButton* m_copyIdButton;  // Button to copy wallpaper ID
    QTextEdit* m_descriptionEdit;
    QPushButton* m_launchButton;
    QPushButton* m_savePropertiesButton;
    QPushButton* m_resetPropertiesButton;
    QWidget* m_propertiesWidget;
    QScrollArea* m_scrollArea;
    
    // New UI components for settings
    QWidget* m_settingsWidget;
    QPushButton* m_saveSettingsButton;
    
    // Audio settings controls
    QCheckBox* m_silentCheckBox;
    QSlider* m_volumeSlider;
    QLabel* m_volumeLabel;
    QCheckBox* m_noAutoMuteCheckBox;
    QCheckBox* m_noAudioProcessingCheckBox;
    
    // Performance settings controls
    QSpinBox* m_fpsSpinBox;
    
    // Display settings controls
    QLineEdit* m_windowGeometryEdit;
    QComboBox* m_screenRootCombo;
    QLineEdit* m_backgroundIdEdit;
    QComboBox* m_scalingCombo;
    QComboBox* m_clampingCombo;
    
    // Behavior settings controls
    QCheckBox* m_disableMouseCheckBox;
    QCheckBox* m_disableParallaxCheckBox;
    QCheckBox* m_noFullscreenPauseCheckBox;
    
    // External wallpaper settings controls
    QLineEdit* m_externalNameEdit;
    QPushButton* m_saveExternalNameButton;
    
    // WNEL-specific settings controls
    QCheckBox* m_noLoopCheckBox;
    QCheckBox* m_noHardwareDecodeCheckBox;
    QCheckBox* m_forceX11CheckBox;
    QCheckBox* m_forceWaylandCheckBox;
    QCheckBox* m_verboseCheckBox;
    QComboBox* m_logLevelCombo;
    QLineEdit* m_mpvOptionsEdit;
    
    // Current wallpaper
    WallpaperInfo m_currentWallpaper;
    WallpaperSettings m_currentSettings;
    
    // Wallpaper manager reference
    WallpaperManager* m_wallpaperManager;
    
    // Animation support for preview
    QMovie* m_previewMovie;
    
    // Track modified properties
    QMap<QString, QWidget*> m_propertyWidgets;
    QMap<QString, QJsonValue> m_originalValues;
    QMap<QString, QJsonObject> m_originalPropertyObjects;
    bool m_propertiesModified;
    bool m_settingsModified;
    bool m_isWallpaperRunning;
    
    // New member variables for unsaved changes handling
    bool m_ignoreTabChange;
    bool m_userInteractingWithTabs;  // Track when user is actively switching tabs
    
    // UI elements that need to be hidden/shown based on wallpaper type
    QWidget* m_idSection;  // ID section with copy button
    QWidget* m_steamSection;  // Steam-specific metadata section
    QWidget* m_regularEngineSection;  // Regular engine settings not supported by WNEL
    
    // Individual setting widgets and their labels for visibility control
    QLabel* m_windowGeometryLabel;
    QWidget* m_windowGeometryWidget;
    QLabel* m_backgroundIdLabel;
    QWidget* m_backgroundIdWidget;
    QLabel* m_clampingLabel;
    QWidget* m_clampingWidget;
    QWidget* m_noAudioProcessingWidget;
    
    // Helper methods for Steam API data
    void updateSteamApiMetadata(const WallpaperInfo& wallpaper);
    void refreshWallpaperMetadata();
    void onApiMetadataReceived(const QString& itemId, const WorkshopItemInfo& info);
    
    // UI visibility management
    void updateUIVisibilityForWallpaperType(bool isExternal);
    QString getExternalWallpaperFilePath(const QString& wallpaperId);
    
    // Animation helper methods for preview
    void startPreviewAnimation();
    void stopPreviewAnimation();
    void loadAnimatedPreview(const QString& previewPath);
    bool hasAnimatedPreview(const QString& previewPath) const;

    // Tab widget is now public to fix segmentation fault issues
public:
    QTabWidget* m_innerTabWidget;    // Made public for MainWindow access

    // expose each of the 4 tabs
    QWidget* infoTab() const          { return m_innerTabWidget->widget(0); }
    QWidget* wallpaperSettingsTab() const { return m_innerTabWidget->widget(1); }
    QWidget* engineSettingsTab() const    { return m_innerTabWidget->widget(2); }
    QWidget* engineLogTab() const         { return m_innerTabWidget->widget(3); }
};

#endif // PROPERTIESPANEL_H
