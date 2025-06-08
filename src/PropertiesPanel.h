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

#include "WallpaperManager.h"
#include "SteamApiManager.h" // Add this include for WorkshopItemInfo type

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

    // Convert settings to command line arguments
    QStringList toCommandLineArgs() const;
};

class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget* parent = nullptr);
    
    void setWallpaper(const WallpaperInfo& wallpaper);
    void clear();

signals:
    void launchWallpaper(const WallpaperInfo& wallpaper);
    void propertiesChanged(const QString& wallpaperId, const QJsonObject& properties);
    void settingsChanged(const QString& wallpaperId, const WallpaperSettings& settings);

private slots:
    void onPropertyChanged();
    void onSavePropertiesClicked();
    void restartWallpaperWithChanges();
    void onUserProfileReceived(const QString& steamId, const SteamUserProfile& profile);
    // New slots for custom settings
    void onSettingChanged();
    void onSaveSettingsClicked();
    void onScreenRootChanged(const QString& screenRoot);

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
    bool loadCachedProperties(const QString& wallpaperId);
    bool saveCachedProperties(const QString& wallpaperId, const QJsonObject& properties);
    QString getCacheFilePath(const QString& wallpaperId) const;  // Added const qualifier to match implementation
    
    // Modified method to take a settings tab as parameter
    void setupSettingsUI(QWidget* settingsTab);
    void updateSettingsControls();
    bool loadWallpaperSettings(const QString& wallpaperId);
    bool saveWallpaperSettings(const QString& wallpaperId);
    QString getSettingsFilePath(const QString& wallpaperId);
    QStringList getAvailableScreens() const;
    
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
    QTextEdit* m_descriptionEdit;
    QPushButton* m_launchButton;
    QPushButton* m_savePropertiesButton;
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
    
    // Current wallpaper
    WallpaperInfo m_currentWallpaper;
    WallpaperSettings m_currentSettings;
    
    // Animation support for preview
    QMovie* m_previewMovie;
    
    // Track modified properties
    QMap<QString, QWidget*> m_propertyWidgets;
    QMap<QString, QJsonValue> m_originalValues;
    bool m_propertiesModified;
    bool m_settingsModified;
    bool m_isWallpaperRunning;
    
    // Helper methods for Steam API data
    void updateSteamApiMetadata(const WallpaperInfo& wallpaper);
    void refreshWallpaperMetadata();
    void onApiMetadataReceived(const QString& itemId, const WorkshopItemInfo& info);
    
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
