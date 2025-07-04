#include "SettingsDialog.h"
#include "../core/ConfigManager.h"
#include "../steam/SteamDetector.h"
#include "../steam/SteamApiManager.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QProcess>
#include <QStyleFactory>
#include <QClipboard>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_config(ConfigManager::instance())
{
    setWindowTitle("Wallpaper Engine Settings");
    setModal(true);
    resize(600, 500);
    
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    
    // Create tab widget
    auto *tabWidget = new QTabWidget;
    layout->addWidget(tabWidget);
    
    // Create Paths tab
    tabWidget->addTab(createPathsTab(), "Paths");
    
    // Create Steam API tab
    tabWidget->addTab(createApiTab(), "Steam API");
    
    // Create Theme tab
    tabWidget->addTab(createThemeTab(), "Theme");
    
    // Create Extra tab
    tabWidget->addTab(createExtraTab(), "Extra");
    
    // Create button box
    auto *buttonLayout = new QHBoxLayout;
    
    auto *resetButton = new QPushButton("Reset to Defaults");
    auto *cancelButton = new QPushButton("Cancel");
    auto *okButton = new QPushButton("OK");
    
    okButton->setDefault(true);
    
    buttonLayout->addWidget(resetButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    
    layout->addLayout(buttonLayout);
    
    // Connect buttons
    connect(resetButton, &QPushButton::clicked, this, &SettingsDialog::resetToDefaults);
    connect(cancelButton, &QPushButton::clicked, this, &SettingsDialog::reject);
    connect(okButton, &QPushButton::clicked, this, &SettingsDialog::accept);
}

QWidget* SettingsDialog::createPathsTab()
{
    auto *widget = new QWidget;
    auto *mainLayout = new QVBoxLayout(widget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create scroll area for paths tab
    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    auto *scrollWidget = new QWidget;
    auto *layout = new QVBoxLayout(scrollWidget);
    layout->setContentsMargins(12, 12, 12, 12);
    
    // Wallpaper Engine Binary
    auto *engineGroup = new QGroupBox("Wallpaper Engine Binary");
    auto *engineLayout = new QFormLayout(engineGroup);
    
    m_enginePathEdit = new QLineEdit;
    auto *engineBrowseButton = new QPushButton("Browse...");
    auto *engineTestButton = new QPushButton("Test");
    
    auto *enginePathLayout = new QHBoxLayout;
    enginePathLayout->addWidget(m_enginePathEdit);
    enginePathLayout->addWidget(engineBrowseButton);
    enginePathLayout->addWidget(engineTestButton);
    
    engineLayout->addRow("Binary Path:", enginePathLayout);
    
    connect(engineBrowseButton, &QPushButton::clicked, this, &SettingsDialog::browseEnginePath);
    connect(engineTestButton, &QPushButton::clicked, this, &SettingsDialog::testEnginePath);
    
    layout->addWidget(engineGroup);
    
    // Assets Directory (manual override)
    auto *assetsGroup = new QGroupBox("Assets Directory (Optional)");
    auto *assetsLayout = new QFormLayout(assetsGroup);
    
    m_assetsDirEdit = new QLineEdit;
    m_assetsDirEdit->setPlaceholderText("Leave empty to auto-detect from Steam libraries");
    auto *assetsBrowseButton = new QPushButton("Browse...");
    
    auto *assetsPathLayout = new QHBoxLayout;
    assetsPathLayout->addWidget(m_assetsDirEdit);
    assetsPathLayout->addWidget(assetsBrowseButton);
    
    assetsLayout->addRow("Assets Path:", assetsPathLayout);
    
    auto *assetsInfoLabel = new QLabel(
        "This should point to the 'assets' folder inside Wallpaper Engine.\n"
        "Correct path: .../steamapps/common/wallpaper_engine/assets/\n"
        "Must contain 'shaders' and 'materials' subdirectories.");
    assetsInfoLabel->setStyleSheet("color: #666; font-size: 10px;");
    assetsInfoLabel->setWordWrap(true);
    assetsLayout->addRow(assetsInfoLabel);
    
    connect(assetsBrowseButton, &QPushButton::clicked, this, [this]() {
        QString startPath = "/home/miki/general/steamlibrary/steamapps/common/wallpaper_engine/assets";
        if (!QDir(startPath).exists()) {
            startPath = "/home/miki/general/steamlibrary/steamapps/common/wallpaper_engine";
        }
        
        QString path = QFileDialog::getExistingDirectory(this,
            "Select Wallpaper Engine Assets Directory",
            startPath);
        
        if (!path.isEmpty()) {
            // Validate that this contains shaders
            QString shadersPath = QDir(path).filePath("shaders");
            if (!QDir(shadersPath).exists()) {
                QMessageBox::warning(this, "Invalid Assets Directory",
                    "The selected directory does not contain a 'shaders' folder.\n\n"
                    "Please select the correct assets directory:\n"
                    ".../steamapps/common/wallpaper_engine/assets/");
                return;
            }
            
            m_assetsDirEdit->setText(path);
            QMessageBox::information(this, "Valid Assets Directory",
                "✓ Valid assets directory selected with shaders folder found.");
        }
    });
    
    layout->addWidget(assetsGroup);
    
    // Steam Detection
    auto *steamGroup = new QGroupBox("Steam Detection");
    auto *steamLayout = new QVBoxLayout(steamGroup);
    
    m_steamPathEdit = new QLineEdit;
    m_steamPathEdit->setReadOnly(true);
    auto *steamDetectButton = new QPushButton("Auto-Detect Steam");
    auto *steamBrowseButton = new QPushButton("Browse...");
    
    auto *steamPathLayout = new QHBoxLayout;
    steamPathLayout->addWidget(m_steamPathEdit);
    steamPathLayout->addWidget(steamDetectButton);
    steamPathLayout->addWidget(steamBrowseButton);
    
    auto *steamFormLayout = new QFormLayout;
    steamFormLayout->addRow("Steam Root:", steamPathLayout);
    steamLayout->addLayout(steamFormLayout);
    
    m_steamStatusLabel = new QLabel;
    steamLayout->addWidget(m_steamStatusLabel);
    
    connect(steamDetectButton, &QPushButton::clicked, this, &SettingsDialog::detectSteam);
    connect(steamBrowseButton, &QPushButton::clicked, this, &SettingsDialog::browseSteamPath);
    
    layout->addWidget(steamGroup);
    
    // Steam Libraries
    auto *libraryGroup = new QGroupBox("Steam Libraries");
    auto *libraryLayout = new QVBoxLayout(libraryGroup);
    
    m_steamLibraryList = new QListWidget;
    m_steamLibraryList->setMaximumHeight(150);
    libraryLayout->addWidget(m_steamLibraryList);
    
    auto *libraryButtonLayout = new QHBoxLayout;
    m_addLibraryButton = new QPushButton("Add Library");
    m_removeLibraryButton = new QPushButton("Remove");
    m_browseLibraryButton = new QPushButton("Browse...");
    
    libraryButtonLayout->addWidget(m_addLibraryButton);
    libraryButtonLayout->addWidget(m_removeLibraryButton);
    libraryButtonLayout->addWidget(m_browseLibraryButton);
    libraryButtonLayout->addStretch();
    
    libraryLayout->addLayout(libraryButtonLayout);
    
    connect(m_addLibraryButton, &QPushButton::clicked, this, &SettingsDialog::addSteamLibrary);
    connect(m_removeLibraryButton, &QPushButton::clicked, this, &SettingsDialog::removeSteamLibrary);
    connect(m_browseLibraryButton, &QPushButton::clicked, this, &SettingsDialog::browseSteamLibrary);
    connect(m_steamLibraryList, &QListWidget::itemSelectionChanged, this, &SettingsDialog::onSteamLibraryChanged);
    
    layout->addWidget(libraryGroup);
    
    layout->addStretch();
    
    // Set the scroll widget
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);
    
    return widget;
}

QWidget* SettingsDialog::createApiTab()
{
    auto* widget = new QWidget;
    auto* mainLayout = new QVBoxLayout(widget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create scroll area for API tab
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    auto* scrollWidget = new QWidget;
    auto* layout = new QVBoxLayout(scrollWidget);
    layout->setContentsMargins(12, 12, 12, 12);
    
    // Steam API Key section
    auto* apiGroup = new QGroupBox("Steam API Key");
    auto* apiLayout = new QVBoxLayout(apiGroup);
    
    auto* apiInfoLabel = new QLabel(
        "Enter your Steam Web API key to fetch detailed information about wallpapers.\n"
        "You can get a free API key from: <a href=\"https://steamcommunity.com/dev/apikey\">https://steamcommunity.com/dev/apikey</a>\n"
        "This allows the application to show metadata such as author, description, and update dates."
    );
    apiInfoLabel->setWordWrap(true);
    apiInfoLabel->setTextFormat(Qt::RichText);
    apiInfoLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    apiInfoLabel->setOpenExternalLinks(false);
    apiInfoLabel->setToolTip("Click the link to copy the URL to clipboard");
    apiLayout->addWidget(apiInfoLabel);
    connect(apiInfoLabel, &QLabel::linkActivated,
            this, &SettingsDialog::copyApiKeyUrlToClipboard);
    
    auto* apiKeyLayout = new QHBoxLayout;
    m_apiKeyEdit = new QLineEdit;
    m_apiKeyEdit->setPlaceholderText("Enter Steam API Key");
    m_apiKeyEdit->setEchoMode(QLineEdit::Password); // Hide by default for security
    
    m_showApiKeyCheckbox = new QCheckBox("Show");
    connect(m_showApiKeyCheckbox, &QCheckBox::toggled, this, &SettingsDialog::toggleApiKeyVisibility);
    
    m_testApiButton = new QPushButton("Test Key");
    connect(m_testApiButton, &QPushButton::clicked, this, &SettingsDialog::testApiKey);
    
    apiKeyLayout->addWidget(m_apiKeyEdit);
    apiKeyLayout->addWidget(m_showApiKeyCheckbox);
    apiKeyLayout->addWidget(m_testApiButton);
    apiLayout->addLayout(apiKeyLayout);
    
    m_testResultLabel = new QLabel("");
    m_testResultLabel->setWordWrap(true);
    apiLayout->addWidget(m_testResultLabel);
    
    m_apiStatusLabel = new QLabel("Steam API key not configured");
    apiLayout->addWidget(m_apiStatusLabel);
    
    // Use Steam API option
    m_useApiCheckbox = new QCheckBox("Enable Steam API integration");
    m_useApiCheckbox->setToolTip("When enabled, the app will fetch metadata from Steam Workshop");
    apiLayout->addWidget(m_useApiCheckbox);
    
    // Add some explanation about API usage
    auto* usageInfoLabel = new QLabel(
        "When enabled, the application will use the Steam API to fetch detailed information\n"
        "about your wallpapers such as author details, descriptions, ratings, and update dates.\n\n"
        "This information will be cached locally and only refreshed when needed."
    );
    usageInfoLabel->setWordWrap(true);
    apiLayout->addWidget(usageInfoLabel);
    
    layout->addWidget(apiGroup);
    
    // Test wallpaper section
    auto* testGroup = new QGroupBox("Test Wallpaper");
    auto* testLayout = new QVBoxLayout(testGroup);
    
    auto* testWallpaperInfo = new QLabel(
        "To test your API key, we'll fetch information for this popular Wallpaper Engine wallpaper:\n"
        "ID: 1081733658 - 'Cat Roommates'"
    );
    testWallpaperInfo->setWordWrap(true);
    testLayout->addWidget(testWallpaperInfo);
    
    auto* testApiKeyButton = new QPushButton("Test With Sample Wallpaper");
    connect(testApiKeyButton, &QPushButton::clicked, this, [this]() {
        // First save any changes to the API key
        saveApiKey();
        // Then test with the sample wallpaper ID
        SteamApiManager::instance().testApiKey("1081733658");
    });
    testLayout->addWidget(testApiKeyButton);
    
    layout->addWidget(testGroup);
    
    // Connect with SteamApiManager signals
    connect(&SteamApiManager::instance(), &SteamApiManager::apiKeyTestSucceeded,
            this, &SettingsDialog::onApiKeyTestSucceeded);
    connect(&SteamApiManager::instance(), &SteamApiManager::apiKeyTestFailed,
            this, &SettingsDialog::onApiKeyTestFailed);
    
    layout->addStretch();
    
    // Set the scroll widget
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);
    
    return widget;
}

QWidget* SettingsDialog::createThemeTab()
{
    auto *widget = new QWidget;
    auto *mainLayout = new QVBoxLayout(widget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create scroll area for theme tab
    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    auto *scrollWidget = new QWidget;
    auto *layout = new QVBoxLayout(scrollWidget);
    layout->setContentsMargins(12, 12, 12, 12);
    
    // Create theme selection group
    auto *themeGroup = new QGroupBox("Application Theme");
    auto *themeLayout = new QVBoxLayout(themeGroup);
    
    // Add combobox with available styles
    auto *themeFormLayout = new QFormLayout;
    m_themeComboBox = new QComboBox();
    
    // Get available styles from QStyleFactory
    QStringList availableThemes = QStyleFactory::keys();
    availableThemes.prepend("System Default");
    m_themeComboBox->addItems(availableThemes);
    
    themeFormLayout->addRow(QString("Theme:"), m_themeComboBox);
    themeLayout->addLayout(themeFormLayout);
    
    // Add theme description and note
    auto *themeDescription = new QLabel(
        "Select the application theme for Wallpaper Engine GUI.\n\n"
        "The 'System Default' option will use your system's native style.\n"
        "Other options provide alternative looks that may integrate better with specific desktop environments.\n\n"
        "Note: Theme changes require restarting the application to take full effect."
    );
    themeDescription->setWordWrap(true);
    themeLayout->addWidget(themeDescription);
    
    // Add theme preview (placeholder)
    m_themePreviewLabel = new QLabel("Theme Preview");
    m_themePreviewLabel->setAlignment(Qt::AlignCenter);
    m_themePreviewLabel->setFrameShape(QFrame::StyledPanel);
    m_themePreviewLabel->setMinimumHeight(150);
    themeLayout->addWidget(m_themePreviewLabel);
    
    // Connect signals - update the preview text when theme changes
    connect(m_themeComboBox, &QComboBox::currentTextChanged, 
            [this](const QString &text) {
                m_themePreviewLabel->setText("Theme: " + text);
            });
    
    layout->addWidget(themeGroup);
    layout->addStretch();
    
    // Set the scroll widget
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);
    
    return widget;
}

QWidget* SettingsDialog::createExtraTab()
{
    auto* widget = new QWidget;
    auto* mainLayout = new QVBoxLayout(widget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create scroll area for extra tab
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    auto* scrollWidget = new QWidget;
    auto* layout = new QVBoxLayout(scrollWidget);
    layout->setContentsMargins(12, 12, 12, 12);
    
    // wallpaper_not-engine_linux addon section
    auto* wnelGroup = new QGroupBox("wallpaper_not-engine_linux Addon");
    auto* wnelLayout = new QVBoxLayout(wnelGroup);
    
    // Enable checkbox
    m_enableWNELCheckbox = new QCheckBox("Enable support for wallpaper_not-engine_linux addon");
    wnelLayout->addWidget(m_enableWNELCheckbox);
    
    // Description
    m_wnelDescriptionLabel = new QLabel(
        "This addon adds support for custom wallpapers (images, GIFs, and videos) using the lightweight "
        "wallpaper_not-engine_linux binary. It provides GPU-accelerated video playback with audio support, "
        "multi-monitor compatibility, and works on both X11 and Wayland."
    );
    m_wnelDescriptionLabel->setWordWrap(true);
    m_wnelDescriptionLabel->setStyleSheet("QLabel { color: #666; margin: 8px 0px; }");
    wnelLayout->addWidget(m_wnelDescriptionLabel);
    
    // Addon URL
    auto* urlLayout = new QHBoxLayout;
    auto* urlLabel = new QLabel("Addon repository:");
    m_copyWNELUrlButton = new QPushButton("Copy URL to clipboard");
    m_copyWNELUrlButton->setToolTip("https://github.com/MikiDevLog/wallpaper_not-engine_linux");
    urlLayout->addWidget(urlLabel);
    urlLayout->addWidget(m_copyWNELUrlButton);
    urlLayout->addStretch();
    wnelLayout->addLayout(urlLayout);
    
    // External wallpapers path
    auto* pathLayout = new QHBoxLayout;
    auto* pathLabel = new QLabel("External wallpapers folder:");
    m_externalWallpapersPathEdit = new QLineEdit;
    m_externalWallpapersPathEdit->setPlaceholderText("Path where custom wallpapers will be stored");
    m_browseExternalPathButton = new QPushButton("Browse...");
    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(m_externalWallpapersPathEdit);
    pathLayout->addWidget(m_browseExternalPathButton);
    wnelLayout->addLayout(pathLayout);
    
    // WNEL binary path
    auto* binaryLayout = new QHBoxLayout;
    auto* binaryLabel = new QLabel("wallpaper_ne_linux binary:");
    m_wnelBinaryPathEdit = new QLineEdit;
    m_wnelBinaryPathEdit->setPlaceholderText("Path to wallpaper_ne_linux binary");
    m_browseWNELBinaryButton = new QPushButton("Browse...");
    m_testWNELBinaryButton = new QPushButton("Test");
    binaryLayout->addWidget(binaryLabel);
    binaryLayout->addWidget(m_wnelBinaryPathEdit);
    binaryLayout->addWidget(m_browseWNELBinaryButton);
    binaryLayout->addWidget(m_testWNELBinaryButton);
    wnelLayout->addLayout(binaryLayout);
    
    // Connect signals
    connect(m_enableWNELCheckbox, &QCheckBox::toggled, this, &SettingsDialog::onWNELEnabledChanged);
    connect(m_copyWNELUrlButton, &QPushButton::clicked, this, &SettingsDialog::copyWNELUrlToClipboard);
    connect(m_browseExternalPathButton, &QPushButton::clicked, this, &SettingsDialog::browseExternalWallpapersPath);
    connect(m_browseWNELBinaryButton, &QPushButton::clicked, this, &SettingsDialog::browseWNELBinaryPath);
    connect(m_testWNELBinaryButton, &QPushButton::clicked, this, &SettingsDialog::testWNELBinary);
    
    layout->addWidget(wnelGroup);
    layout->addStretch();
    
    // Set the scroll widget
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);
    
    return widget;
}

void SettingsDialog::loadSettings()
{
    // Paths
    m_enginePathEdit->setText(m_config.wallpaperEnginePath());
    m_steamPathEdit->setText(m_config.steamPath());
    updateSteamStatus();
    
    // Steam libraries
    updateSteamLibraryList();
    
    // Load existing library paths from config
    QStringList libraryPaths = m_config.steamLibraryPaths();
    m_steamLibraryList->clear();
    
    // Add the user's custom library path if not already present
    QString customLibraryPath = "/home/miki/general/steamlibrary";
    if (QDir(customLibraryPath).exists() && !libraryPaths.contains(customLibraryPath)) {
        libraryPaths.prepend(customLibraryPath);
    }
    
    for (const QString& path : libraryPaths) {
        if (QDir(path).exists()) {
            m_steamLibraryList->addItem(path);
        }
    }
    
    // Assets directory
    m_assetsDirEdit->setText(m_config.getAssetsDir());
    
    // Assets directory validation and auto-suggestion
    QString assetsDir = m_config.getAssetsDir();
    
    // If no assets directory is configured, try to auto-detect the correct one
    if (assetsDir.isEmpty()) {
        QStringList libraryPaths = m_config.steamLibraryPaths();
        if (!libraryPaths.isEmpty()) {
            SteamDetector detector;
            for (const QString& libraryPath : libraryPaths) {
                QString detectedAssetsPath = detector.getWallpaperEngineAssetsPath(libraryPath);
                if (!detectedAssetsPath.isEmpty() && QDir(detectedAssetsPath + "/shaders").exists()) {
                    assetsDir = detectedAssetsPath;
                    m_config.setAssetsDir(assetsDir);
                    break;
                }
            }
        }
    }
    
    m_assetsDirEdit->setText(assetsDir);
    
    // Load Steam API settings
    m_apiKeyEdit->setText(m_config.steamApiKey());
    m_useApiCheckbox->setChecked(m_config.useSteamApi());
    
    // Update API status message
    if (!m_config.steamApiKey().isEmpty()) {
        m_apiStatusLabel->setText("Steam API key configured");
        m_apiStatusLabel->setStyleSheet("color: green;");
    } else {
        m_apiStatusLabel->setText("Steam API key not configured");
        m_apiStatusLabel->setStyleSheet("color: red;");
    }
    
    // Load theme settings
    QString currentTheme = m_config.theme();
    if (currentTheme.isEmpty()) {
        m_themeComboBox->setCurrentText("System Default");
    } else {
        int themeIndex = m_themeComboBox->findText(currentTheme);
        if (themeIndex >= 0) {
            m_themeComboBox->setCurrentIndex(themeIndex);
        } else {
            m_themeComboBox->setCurrentText("System Default");
        }
    }
    
    // Load WNEL settings
    m_enableWNELCheckbox->setChecked(m_config.isWNELAddonEnabled());
    m_externalWallpapersPathEdit->setText(m_config.externalWallpapersPath());
    m_wnelBinaryPathEdit->setText(m_config.wnelBinaryPath());
    
    // Update UI state based on WNEL enabled state
    onWNELEnabledChanged(m_enableWNELCheckbox->isChecked());
}

void SettingsDialog::saveSettings()
{
    // Paths
    m_config.setWallpaperEnginePath(m_enginePathEdit->text());
    m_config.setSteamPath(m_steamPathEdit->text());
    
    // Assets directory
    m_config.setAssetsDir(m_assetsDirEdit->text());
    
    // Save Steam libraries
    QStringList libraryPaths;
    for (int i = 0; i < m_steamLibraryList->count(); ++i) {
        libraryPaths.append(m_steamLibraryList->item(i)->text());
    }
    m_config.setSteamLibraryPaths(libraryPaths);
    
    // Save Steam API settings
    saveApiKey();
    m_config.setUseSteamApi(m_useApiCheckbox->isChecked());
    
    // Save theme settings
    QString selectedTheme = m_themeComboBox->currentText();
    if (selectedTheme == "System Default") {
        m_config.setTheme("");
    } else {
        m_config.setTheme(selectedTheme);
    }
    
    // Save WNEL settings
    m_config.setWNELAddonEnabled(m_enableWNELCheckbox->isChecked());
    m_config.setExternalWallpapersPath(m_externalWallpapersPathEdit->text());
    m_config.setWNELBinaryPath(m_wnelBinaryPathEdit->text());
    
    // Mark first run as complete if configuration is now valid
    if (m_config.isConfigurationValid()) {
        m_config.setFirstRun(false);
    }
}

void SettingsDialog::resetToDefaults()
{
    auto result = QMessageBox::question(this, "Reset Settings",
        "Are you sure you want to reset all settings to defaults?");
    
    if (result == QMessageBox::Yes) {
        m_config.resetToDefaults();
        loadSettings();
    }
}

void SettingsDialog::browseEnginePath()
{
    QString path = QFileDialog::getOpenFileName(this,
        "Select Wallpaper Engine Binary",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        "Executable Files (*)");
    
    if (!path.isEmpty()) {
        m_enginePathEdit->setText(path);
    }
}

void SettingsDialog::testEnginePath()
{
    QString path = m_enginePathEdit->text();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "Test Failed", "Please specify a binary path first.");
        return;
    }
    
    QProcess process;
    process.start(path, QStringList() << "--help");
    bool finished = process.waitForFinished(3000);
    
    if (finished && process.exitCode() == 0) {
        QMessageBox::information(this, "Test Successful", 
            "Wallpaper Engine binary is working correctly.");
    } else {
        QMessageBox::warning(this, "Test Failed", 
            "Failed to execute the wallpaper engine binary.\n"
            "Please check the path and ensure the file is executable.");
    }
}

void SettingsDialog::detectSteam()
{
    SteamDetector detector;
    auto installations = detector.detectSteamInstallations();
    
    if (!installations.isEmpty()) {
        QString steamPath = installations.first().path;
        m_steamPathEdit->setText(steamPath);
        updateSteamStatus();
        QMessageBox::information(this, "Steam Detected", 
            QString("Steam installation found at:\n%1").arg(steamPath));
    } else {
        QMessageBox::warning(this, "Steam Not Found",
            "Could not automatically detect Steam installation.\n"
            "Please browse for the Steam root directory manually.");
    }
}

void SettingsDialog::browseSteamPath()
{
    QString path = QFileDialog::getExistingDirectory(this,
        "Select Steam Root Directory",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    
    if (!path.isEmpty()) {
        m_steamPathEdit->setText(path);
        updateSteamStatus();
    }
}

void SettingsDialog::browseSteamLibrary()
{
    QString path = QFileDialog::getExistingDirectory(this,
        "Select Steam Library Directory",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    
    if (!path.isEmpty()) {
        // Validate that this looks like a Steam library
        QString steamappsPath = QDir(path).filePath("steamapps");
        if (QDir(steamappsPath).exists()) {
            m_steamLibraryList->addItem(path);
            updateSteamLibraryList();
        } else {
            QMessageBox::warning(this, "Invalid Steam Library",
                "The selected directory does not appear to be a valid Steam library.\n"
                "It should contain a 'steamapps' subdirectory.");
        }
    }
}

void SettingsDialog::updateSteamStatus()
{
    QString steamPath = m_steamPathEdit->text();
    if (steamPath.isEmpty()) {
        m_steamStatusLabel->setText("❌ No Steam path configured");
        return;
    }
    
    QDir steamDir(steamPath);
    if (!steamDir.exists()) {
        m_steamStatusLabel->setText("❌ Steam directory does not exist");
        return;
    }
    
    // Check for steamapps directory
    QString steamappsPath = steamDir.absoluteFilePath("steamapps");
    if (!QDir(steamappsPath).exists()) {
        m_steamStatusLabel->setText("❌ steamapps directory not found");
        return;
    }
    
    // Simply show "Steam detected" without wallpaper count
    m_steamStatusLabel->setText("✅ Steam detected");
}

void SettingsDialog::updateSteamLibraryList()
{
    // Empty implementation - removed "Total wallpapers found" display logic
}

void SettingsDialog::accept()
{
    // Remember current theme before saving
    QString oldTheme = m_config.theme();
    
    // Save settings
    saveSettings();
    
    // Check if theme changed
    QString newTheme = m_config.theme();
    if (oldTheme != newTheme) {
        QMessageBox::information(this, "Theme Changed",
            "The application theme has been changed. The change will take full effect after restarting the application.");
    }
    
    QDialog::accept();
}

void SettingsDialog::addSteamLibrary()
{
    SteamDetector detector;
    QStringList detectedPaths = detector.findSteamLibraryPaths();  // Changed from detectSteamLibraryPaths
    
    // Always add the user's known custom path
    QString customLibraryPath = "/home/miki/general/steamlibrary";
    if (QDir(customLibraryPath).exists() && !detectedPaths.contains(customLibraryPath)) {
        detectedPaths.prepend(customLibraryPath);
    }
    
    if (!detectedPaths.isEmpty()) {
        for (const QString& path : detectedPaths) {
            // Check if already in list
            bool found = false;
            for (int i = 0; i < m_steamLibraryList->count(); ++i) {
                if (m_steamLibraryList->item(i)->text() == path) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                m_steamLibraryList->addItem(path);
            }
        }
        updateSteamLibraryList();
    } else {
        QMessageBox::information(this, "Auto-Detect Libraries",
            QString("No additional Steam libraries found. Use 'Browse...' to add custom paths.\n\n"
                   "Hint: Your Steam library might be at %1").arg(customLibraryPath));
        browseSteamLibrary();
    }
}

void SettingsDialog::removeSteamLibrary()
{
    int currentRow = m_steamLibraryList->currentRow();
    if (currentRow >= 0) {
        delete m_steamLibraryList->takeItem(currentRow);
        updateSteamLibraryList();
    }
}

void SettingsDialog::onSteamLibraryChanged()
{
    m_removeLibraryButton->setEnabled(m_steamLibraryList->currentRow() >= 0);
}

void SettingsDialog::saveApiKey()
{
    QString apiKey = m_apiKeyEdit->text().trimmed();
    SteamApiManager::instance().setApiKey(apiKey);
}

void SettingsDialog::testApiKey()
{
    // Save first to ensure we're testing the current key
    saveApiKey();
    
    m_testResultLabel->setText("Testing API key...");
    m_testResultLabel->setStyleSheet("color: blue;");
    
    // Use a test wallpaper ID
    SteamApiManager::instance().testApiKey("1081733658");
}

void SettingsDialog::toggleApiKeyVisibility(bool show)
{
    m_apiKeyEdit->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
}

void SettingsDialog::onApiKeyTestSucceeded()
{
    m_testResultLabel->setText("✓ API key is valid! Test successful.");
    m_testResultLabel->setStyleSheet("color: green; font-weight: bold;");
    m_apiStatusLabel->setText("Steam API key configured and validated");
    m_apiStatusLabel->setStyleSheet("color: green;");
}

void SettingsDialog::onApiKeyTestFailed(const QString& error)
{
    m_testResultLabel->setText("✗ API key test failed: " + error);
    m_testResultLabel->setStyleSheet("color: red;");
    m_apiStatusLabel->setText("Steam API key configuration issue");
    m_apiStatusLabel->setStyleSheet("color: red;");
}

// Slot to copy the Steam API key URL to clipboard
void SettingsDialog::copyApiKeyUrlToClipboard(const QString& url)
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(url);
    QMessageBox::information(this, tr("Link Copied"), tr("Steam API key URL copied to clipboard."));
}

// Extra tab slots implementation
void SettingsDialog::onWNELEnabledChanged(bool enabled)
{
    // Enable/disable related controls
    m_externalWallpapersPathEdit->setEnabled(enabled);
    m_browseExternalPathButton->setEnabled(enabled);
    m_wnelBinaryPathEdit->setEnabled(enabled);
    m_browseWNELBinaryButton->setEnabled(enabled);
    
    if (enabled) {
        if (m_externalWallpapersPathEdit->text().isEmpty()) {
            // Set default path if none is set
            QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/external_wallpapers";
            m_externalWallpapersPathEdit->setText(defaultPath);
        }
        
        if (m_wnelBinaryPathEdit->text().isEmpty()) {
            // Try to find wallpaper_ne_linux in PATH
            QString binaryPath = QStandardPaths::findExecutable("wallpaper_ne_linux");
            if (!binaryPath.isEmpty()) {
                m_wnelBinaryPathEdit->setText(binaryPath);
            } else {
                // Set default expected path
                m_wnelBinaryPathEdit->setText("wallpaper_ne_linux");
            }
        }
    }
}

void SettingsDialog::copyWNELUrlToClipboard()
{
    QClipboard* clipboard = QApplication::clipboard();
    QString url = "https://github.com/MikiDevLog/wallpaper_not-engine_linux";
    clipboard->setText(url);
    QMessageBox::information(this, "URL Copied", "wallpaper_not-engine_linux repository URL copied to clipboard.");
}

void SettingsDialog::browseExternalWallpapersPath()
{
    QString currentPath = m_externalWallpapersPathEdit->text();
    if (currentPath.isEmpty()) {
        currentPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    
    QString path = QFileDialog::getExistingDirectory(this, "Select External Wallpapers Directory", currentPath);
    if (!path.isEmpty()) {
        m_externalWallpapersPathEdit->setText(path);
    }
}

void SettingsDialog::browseWNELBinaryPath()
{
    QString currentPath = m_wnelBinaryPathEdit->text();
    if (currentPath.isEmpty()) {
        currentPath = QStandardPaths::findExecutable("wallpaper_ne_linux");
        if (currentPath.isEmpty()) {
            currentPath = "/usr/local/bin";
        }
    }
    
    QString path = QFileDialog::getOpenFileName(this, "Select wallpaper_ne_linux Binary", 
                                               QFileInfo(currentPath).absolutePath(),
                                               "Executable files (wallpaper_ne_linux);;All files (*)");
    if (!path.isEmpty()) {
        m_wnelBinaryPathEdit->setText(path);
    }
}

void SettingsDialog::testWNELBinary()
{
    QString binaryPath = m_wnelBinaryPathEdit->text().trimmed();
    if (binaryPath.isEmpty()) {
        QMessageBox::warning(this, "Test Failed", "Please specify the path to wallpaper_ne_linux binary first.");
        return;
    }
    
    // Test if the binary exists and is executable
    QFileInfo binaryInfo(binaryPath);
    if (!binaryInfo.exists()) {
        QMessageBox::warning(this, "Test Failed", QString("Binary not found at: %1").arg(binaryPath));
        return;
    }
    
    if (!binaryInfo.isExecutable()) {
        QMessageBox::warning(this, "Test Failed", QString("File is not executable: %1").arg(binaryPath));
        return;
    }
    
    // Run the binary with --help flag to test it
    QProcess testProcess;
    testProcess.start(binaryPath, QStringList() << "--help");
    
    if (!testProcess.waitForStarted(3000)) {
        QMessageBox::warning(this, "Test Failed", "Failed to start the binary. Check if it's a valid executable.");
        return;
    }
    
    if (!testProcess.waitForFinished(5000)) {
        testProcess.kill();
        QMessageBox::warning(this, "Test Failed", "Binary test timed out.");
        return;
    }
    
    QString output = testProcess.readAllStandardOutput();
    QString error = testProcess.readAllStandardError();
    
    if (testProcess.exitCode() == 0 || output.contains("wallpaper_ne_linux") || output.contains("Usage:")) {
        QMessageBox::information(this, "Test Successful", 
            QString("wallpaper_ne_linux binary is working correctly!\n\nPath: %1").arg(binaryPath));
    } else {
        QString errorMsg = QString("Binary test failed.\nExit code: %1\nOutput: %2\nError: %3")
                          .arg(testProcess.exitCode())
                          .arg(output.left(200))
                          .arg(error.left(200));
        QMessageBox::warning(this, "Test Failed", errorMsg);
    }
}