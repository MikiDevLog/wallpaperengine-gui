#include "PropertiesPanel.h"
#include "../core/ConfigManager.h"
#include "../steam/SteamApiManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QScrollArea>
#include <QPixmap>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>
#include <QProcess>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QClipboard>
#include <QTimer>

Q_LOGGING_CATEGORY(propertiesPanel, "app.propertiespanel")

// Implementation of WallpaperSettings::toCommandLineArgs()
QStringList WallpaperSettings::toCommandLineArgs(bool isExternalWallpaper) const
{
    QStringList args;
    
    // Audio settings
    if (silent) {
        args << "--silent";
    }
    
    if (volume != 15) { // Convert from 0-100 to 0.0-1.0 for WNEL
        double volumeDecimal = volume / 100.0;
        args << "--volume" << QString::number(volumeDecimal, 'f', 2);
    }
    
    if (noAutoMute) {
        args << "--noautomute";
    }
    
    // Note: noAudioProcessing is not supported by WNEL, so it's removed
    
    // Performance settings
    if (fps != 30) { // Only add if different from default
        args << "--fps" << QString::number(fps);
    }
     // Display settings - Use different flags for WNEL vs wallpaper engine
    if (!customScreenRoot.isEmpty() || !screenRoot.isEmpty()) {
        QString screenValue = !customScreenRoot.isEmpty() ? customScreenRoot : screenRoot;
        if (isExternalWallpaper) {
            args << "--output" << screenValue;  // WNEL uses --output
        } else {
            args << "--screen-root" << screenValue;  // wallpaper engine uses --screen-root
        }
    }

    if (scaling != "default") {
        // WNEL scaling modes: stretch, fit, fill, default
        args << "--scaling" << scaling;
    }
    
    // WNEL-specific settings
    if (noLoop) {
        args << "--no-loop";
    }
    
    if (noHardwareDecode) {
        args << "--no-hardware-decode";
    }
    
    if (forceX11) {
        args << "--force-x11";
    }
    
    if (forceWayland) {
        args << "--force-wayland";
    }
    
    if (verbose) {
        args << "--verbose";
    }
    
    if (logLevel != "info") {
        args << "--log-level" << logLevel;
    }
    
    if (!mpvOptions.isEmpty()) {
        args << "--mpv-options" << mpvOptions;
    }

    // Note: Other arguments like clamping, windowGeometry, backgroundId, 
    // disableMouse, disableParallax, noFullscreenPause are not supported 
    // by WNEL and are removed

    return args;
}

PropertiesPanel::PropertiesPanel(QWidget* parent)
    : QWidget(parent)
    , m_nameLabel(new QLabel)
    , m_authorLabel(new QLabel)
    , m_typeLabel(new QLabel)
    , m_fileSizeLabel(new QLabel)
    , m_postedLabel(new QLabel)
    , m_updatedLabel(new QLabel)
    , m_viewsLabel(new QLabel)
    , m_subscriptionsLabel(new QLabel)
    , m_favoritesLabel(new QLabel)
    , m_previewLabel(new QLabel)
    , m_previewIdLabel(nullptr)
    , m_copyIdButton(nullptr)
    , m_descriptionEdit(new QTextEdit)
    , m_launchButton(new QPushButton("Launch Wallpaper"))
    , m_savePropertiesButton(new QPushButton("Save"))
    , m_resetPropertiesButton(new QPushButton("Reset"))
    , m_propertiesWidget(nullptr)
    , m_scrollArea(new QScrollArea)
    , m_settingsWidget(nullptr)
    , m_saveSettingsButton(new QPushButton("Save Settings"))
    , m_useGlobalDefaultsCheckBox(new QCheckBox("Use system-wide engine defaults"))
    , m_silentCheckBox(new QCheckBox("Silent mode"))
    , m_volumeSlider(new QSlider(Qt::Horizontal))
    , m_volumeSpinBox(new QSpinBox)
    , m_noAutoMuteCheckBox(new QCheckBox("Don't mute when other apps play audio"))
    , m_noAudioProcessingCheckBox(new QCheckBox("Disable audio processing"))
    , m_fpsSpinBox(new QSpinBox)
    , m_windowGeometryEdit(new QLineEdit)
    , m_screenRootCombo(new QComboBox)
    , m_customScreenRootEdit(new QLineEdit)
    , m_backgroundIdEdit(new QLineEdit)
    , m_scalingCombo(new QComboBox)
    , m_clampingCombo(new QComboBox)
    , m_disableMouseCheckBox(new QCheckBox("Disable mouse interaction"))
    , m_disableParallaxCheckBox(new QCheckBox("Disable parallax effects"))
    , m_noFullscreenPauseCheckBox(new QCheckBox("Don't pause when apps go fullscreen"))
    , m_externalNameEdit(new QLineEdit)
    , m_saveExternalNameButton(new QPushButton("Save Name"))
    , m_noLoopCheckBox(new QCheckBox("Don't loop video"))
    , m_noHardwareDecodeCheckBox(new QCheckBox("Disable hardware decoding"))
    , m_forceX11CheckBox(new QCheckBox("Force X11 backend"))
    , m_forceWaylandCheckBox(new QCheckBox("Force Wayland backend"))
    , m_verboseCheckBox(new QCheckBox("Verbose output"))
    , m_logLevelCombo(new QComboBox)
    , m_mpvOptionsEdit(new QLineEdit)
    , m_currentWallpaper()
    , m_currentSettings()
    , m_wallpaperManager(nullptr)
    , m_previewMovie(nullptr)
    , m_propertyWidgets()
    , m_originalValues()
    , m_originalPropertyObjects()
    , m_propertiesModified(false)
    , m_settingsModified(false)
    , m_isWallpaperRunning(false)
    , m_ignoreTabChange(false)
    , m_userInteractingWithTabs(false)
    , m_idSection(nullptr)
    , m_steamSection(nullptr)
    , m_regularEngineSection(nullptr)
    , m_windowGeometryLabel(nullptr)
    , m_windowGeometryWidget(nullptr)
    , m_backgroundIdLabel(nullptr)
    , m_backgroundIdWidget(nullptr)
    , m_clampingLabel(nullptr)
    , m_clampingWidget(nullptr)
    , m_noAudioProcessingWidget(nullptr)
    , m_innerTabWidget(nullptr)
{
    setupUI();
    // Connect copy button signal - only if it was created
    if (m_copyIdButton) {
        connect(m_copyIdButton, &QPushButton::clicked, this, &PropertiesPanel::copyWallpaperIdToClipboard);
    }
    // Connect signals for property changes
    connect(m_savePropertiesButton, &QPushButton::clicked, this, &PropertiesPanel::onSavePropertiesClicked);
    connect(m_resetPropertiesButton, &QPushButton::clicked, this, &PropertiesPanel::onResetPropertiesClicked);
    connect(m_saveSettingsButton, &QPushButton::clicked, this, &PropertiesPanel::onSaveSettingsClicked);
    connect(m_launchButton, &QPushButton::clicked, this, [this]() {
        if (!m_currentWallpaper.id.isEmpty()) {
            // Check for unsaved changes before launching
            if (checkUnsavedChangesBeforeAction()) {
                emit launchWallpaper(m_currentWallpaper);
            }
        }
    });
    
    // Connect settings change signals
    connect(m_silentCheckBox, &QCheckBox::toggled, this, &PropertiesPanel::onSettingChanged);
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_volumeSpinBox->setValue(value);
        onSettingChanged();
    });
    connect(m_volumeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_volumeSlider->setValue(value);
        onSettingChanged();
    });
    connect(m_noAutoMuteCheckBox, &QCheckBox::toggled, this, &PropertiesPanel::onSettingChanged);
    connect(m_noAudioProcessingCheckBox, &QCheckBox::toggled, this, &PropertiesPanel::onSettingChanged);
    connect(m_fpsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PropertiesPanel::onSettingChanged);
    connect(m_windowGeometryEdit, &QLineEdit::textChanged, this, &PropertiesPanel::onSettingChanged);
    connect(m_screenRootCombo, &QComboBox::currentTextChanged, this, &PropertiesPanel::onScreenRootChanged);
    connect(m_backgroundIdEdit, &QLineEdit::textChanged, this, &PropertiesPanel::onSettingChanged);
    connect(m_scalingCombo, &QComboBox::currentTextChanged, this, &PropertiesPanel::onSettingChanged);
    connect(m_clampingCombo, &QComboBox::currentTextChanged, this, &PropertiesPanel::onSettingChanged);
    connect(m_disableMouseCheckBox, &QCheckBox::toggled, this, &PropertiesPanel::onSettingChanged);
    connect(m_disableParallaxCheckBox, &QCheckBox::toggled, this, &PropertiesPanel::onSettingChanged);
    connect(m_noFullscreenPauseCheckBox, &QCheckBox::toggled, this, &PropertiesPanel::onSettingChanged);
    
    // Connect to Steam API manager
    connect(&SteamApiManager::instance(), &SteamApiManager::userProfileReceived,
            this, &PropertiesPanel::onUserProfileReceived);
    connect(&SteamApiManager::instance(), &SteamApiManager::itemDetailsReceived,
            this, &PropertiesPanel::onApiMetadataReceived);
}

void PropertiesPanel::setupUI()
{
    m_innerTabWidget = new QTabWidget;
    
    // Install event filter on the tab bar to intercept mouse clicks BEFORE they change tabs
    m_innerTabWidget->tabBar()->installEventFilter(this);
    
    // Info page (just the preview & basic info)
    QWidget* infoPage = new QWidget;
    {
       QVBoxLayout* mainInfoLayout = new QVBoxLayout(infoPage);
       mainInfoLayout->setContentsMargins(0, 0, 0, 0);
       mainInfoLayout->setSpacing(0);
       
       // Create scroll area for info tab
       auto* infoScrollArea = new QScrollArea;
       infoScrollArea->setWidgetResizable(true);
       infoScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
       infoScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
       infoScrollArea->setFrameShape(QFrame::NoFrame);
       
       auto* infoScrollWidget = new QWidget;
       QVBoxLayout* l = new QVBoxLayout(infoScrollWidget);
       l->setContentsMargins(12, 12, 12, 12);
       l->setSpacing(16);
       
       // Preview section - remove fixed height to allow flexibility
       auto* previewSection = new QGroupBox("Preview");
       auto* previewLayout = new QVBoxLayout(previewSection);
       previewLayout->setContentsMargins(12, 16, 12, 12);
       
       m_previewLabel->setFixedSize(256, 144);
       m_previewLabel->setAlignment(Qt::AlignCenter);
       m_previewLabel->setStyleSheet("border: 1px solid gray; background-color: #2a2a2a;");
       m_previewLabel->setScaledContents(false);
       setPlaceholderPreview("No wallpaper selected");
       
       previewLayout->addWidget(m_previewLabel, 0, Qt::AlignCenter);
       // Add ID display and copy button below preview
       {
           m_idSection = new QWidget;
           QHBoxLayout* idLayout = new QHBoxLayout(m_idSection);
           idLayout->setContentsMargins(0, 8, 0, 0);
           idLayout->addWidget(new QLabel("ID:"));
           m_previewIdLabel = new QLabel("-");
           idLayout->addWidget(m_previewIdLabel);
           m_copyIdButton = new QPushButton("Copy");
           idLayout->addWidget(m_copyIdButton);
           idLayout->addStretch();
           previewLayout->addWidget(m_idSection);
       }
       // Remove fixed height to allow natural sizing
       l->addWidget(previewSection);
       
       // Basic info section with proper spacing
       auto* infoSection = new QGroupBox("Wallpaper Information");
       auto* infoLayout = new QFormLayout(infoSection);
       infoLayout->setContentsMargins(12, 16, 12, 12);
       infoLayout->setVerticalSpacing(12);
       infoLayout->setHorizontalSpacing(20);
       infoLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
       infoLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
       infoLayout->setLabelAlignment(Qt::AlignLeft);
       
       // Configure labels with proper sizing and word wrap
       m_nameLabel->setWordWrap(true);
       m_nameLabel->setMinimumHeight(24);
       m_nameLabel->setMaximumHeight(48);
       m_nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
       
       m_authorLabel->setMinimumHeight(24);
       m_authorLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
       
       m_typeLabel->setMinimumHeight(24);
       m_typeLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
       
       m_fileSizeLabel->setMinimumHeight(24);
       m_fileSizeLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
       
       m_postedLabel->setMinimumHeight(24);
       m_postedLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
       
       m_updatedLabel->setMinimumHeight(24);
       m_updatedLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
       
       m_viewsLabel->setMinimumHeight(24);
       m_viewsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
       
       m_subscriptionsLabel->setMinimumHeight(24);
       m_subscriptionsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
       
       m_favoritesLabel->setMinimumHeight(24);
       m_favoritesLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
       
       // Create labels for form layout with proper styling
       auto createFormLabel = [](const QString& text) {
           auto* label = new QLabel(text);
           label->setMinimumWidth(80);
           label->setMaximumWidth(100);
           label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
           label->setAlignment(Qt::AlignRight | Qt::AlignTop);
           label->setStyleSheet("font-weight: bold; padding-top: 2px;");
           return label;
       };
       
       infoLayout->addRow(createFormLabel("Name:"), m_nameLabel);
       infoLayout->addRow(createFormLabel("Author:"), m_authorLabel);
       infoLayout->addRow(createFormLabel("Type:"), m_typeLabel);
       infoLayout->addRow(createFormLabel("File Size:"), m_fileSizeLabel);
       
       l->addWidget(infoSection);
       
       // Steam-specific metadata section (hidden for external wallpapers)
       m_steamSection = new QGroupBox("Steam Workshop Information");
       auto* steamLayout = new QFormLayout(m_steamSection);
       steamLayout->setContentsMargins(12, 16, 12, 12);
       steamLayout->setVerticalSpacing(12);
       steamLayout->setHorizontalSpacing(20);
       steamLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
       steamLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
       steamLayout->setLabelAlignment(Qt::AlignLeft);
       
       steamLayout->addRow(createFormLabel("Posted:"), m_postedLabel);
       steamLayout->addRow(createFormLabel("Updated:"), m_updatedLabel);
       steamLayout->addRow(createFormLabel("Views:"), m_viewsLabel);
       steamLayout->addRow(createFormLabel("Subscriptions:"), m_subscriptionsLabel);
       steamLayout->addRow(createFormLabel("Favorites:"), m_favoritesLabel);
       
       l->addWidget(m_steamSection);
       
       // Description section with proper sizing
       auto* descSection = new QGroupBox("Description");
       auto* descLayout = new QVBoxLayout(descSection);
       descLayout->setContentsMargins(12, 16, 12, 12);
       
       m_descriptionEdit->setMinimumHeight(80);
       m_descriptionEdit->setMaximumHeight(120);
       m_descriptionEdit->setReadOnly(true);
       m_descriptionEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
       descLayout->addWidget(m_descriptionEdit);
       
       l->addWidget(descSection);
       
       // Launch button section
       auto* launchLayout = new QHBoxLayout;
       launchLayout->setContentsMargins(0, 8, 0, 0);
       m_launchButton->setMinimumHeight(36);
       m_launchButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
       launchLayout->addStretch();
       launchLayout->addWidget(m_launchButton);
       launchLayout->addStretch();
       
       l->addLayout(launchLayout);
       
       // Set the scroll widget and add it to the main layout
       infoScrollArea->setWidget(infoScrollWidget);
       mainInfoLayout->addWidget(infoScrollArea);
    }
    
    // Wallpaper Settings (old “Properties” tab)
    QWidget* propsPage = new QWidget;
    {
       QVBoxLayout* l = new QVBoxLayout(propsPage);
       l->setContentsMargins(12, 12, 12, 12);
       l->setSpacing(16);

       auto* propsSection = new QGroupBox("Wallpaper Properties");
       auto* propsLayout = new QVBoxLayout(propsSection);
       propsLayout->setContentsMargins(12, 16, 12, 12);

       // Create scroll area for properties
       m_scrollArea->setWidgetResizable(true);
       m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
       m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
       // removed fixed min/max height to allow full expansion
       m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
       
       // Fix ghosting/transparency issues
       m_scrollArea->setFrameShape(QFrame::NoFrame);
       m_scrollArea->setAutoFillBackground(true);
       QPalette pal = m_scrollArea->palette();
       pal.setColor(QPalette::Window, palette().color(QPalette::Base));
       m_scrollArea->setPalette(pal);

       // Create placeholder widget for properties
       m_propertiesWidget = new QWidget;
       m_propertiesWidget->setAutoFillBackground(true); // Ensure proper background painting
       QPalette widgetPal = m_propertiesWidget->palette();
       widgetPal.setColor(QPalette::Window, palette().color(QPalette::Base));
       m_propertiesWidget->setPalette(widgetPal);
       
       auto* propsFormLayout = new QVBoxLayout(m_propertiesWidget);
       propsFormLayout->setContentsMargins(8, 8, 8, 8);
       propsFormLayout->addWidget(new QLabel("Select a wallpaper to view properties"));

       m_scrollArea->setWidget(m_propertiesWidget);
       propsLayout->addWidget(m_scrollArea);
       // Properties buttons
       auto* propsButtonLayout = new QHBoxLayout;
       propsButtonLayout->setContentsMargins(0, 8, 0, 0);
       propsButtonLayout->addStretch();
       propsButtonLayout->addWidget(m_resetPropertiesButton);
       propsButtonLayout->addWidget(m_savePropertiesButton);
       propsLayout->addLayout(propsButtonLayout);

       l->addWidget(propsSection, /*stretch=*/1);
       l->addStretch();   // ensure any leftover space is used
    }
    
    // Engine Settings (old “Settings” tab)
    QWidget* enginePage = new QWidget;
    setupSettingsUI(enginePage);
    
    // Engine Log (empty placeholder – MainWindow will wire its QTextEdit into here)
    QWidget* logPage = new QWidget;
    {
      QVBoxLayout* l = new QVBoxLayout(logPage);
      l->addWidget(new QLabel("Logs will appear here"));
    }

    m_innerTabWidget->addTab(infoPage,               "Info");
    m_innerTabWidget->addTab(propsPage,              "Wallpaper Settings");
    m_innerTabWidget->addTab(enginePage,             "Engine Settings");
    m_innerTabWidget->addTab(logPage,                "Engine Log");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_innerTabWidget);
}

void PropertiesPanel::setupSettingsUI(QWidget* settingsTab)
{
    auto* mainLayout = new QVBoxLayout(settingsTab);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Create scroll area for settings
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    auto* scrollWidget = new QWidget;
    auto* scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(16, 16, 16, 16);
    scrollLayout->setSpacing(20);
    
    // Add "Use Global Defaults" checkbox at the top
    auto* globalDefaultsGroup = new QGroupBox("Settings Mode");
    auto* globalDefaultsLayout = new QVBoxLayout(globalDefaultsGroup);
    globalDefaultsLayout->setContentsMargins(12, 16, 12, 12);
    
    m_useGlobalDefaultsCheckBox->setChecked(true);  // Default to using global defaults
    m_useGlobalDefaultsCheckBox->setToolTip(
        "When checked, this wallpaper will use the system-wide engine defaults.\n"
        "Uncheck to customize settings for this specific wallpaper.");
    globalDefaultsLayout->addWidget(m_useGlobalDefaultsCheckBox);
    
    auto* infoLabel = new QLabel(
        "✓ Using system-wide defaults - Settings are managed in Settings → Engine Defaults\n"
        "  Uncheck to customize settings for this wallpaper only");
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("QLabel { color: #666; font-size: 10px; margin-top: 4px; }");
    infoLabel->setObjectName("globalDefaultsInfoLabel");
    globalDefaultsLayout->addWidget(infoLabel);
    
    scrollLayout->addWidget(globalDefaultsGroup);
    
    // Connect the checkbox to enable/disable all settings controls
    connect(m_useGlobalDefaultsCheckBox, &QCheckBox::toggled, this, [this, infoLabel](bool useGlobal) {
        // Enable/disable all settings controls based on checkbox
        m_silentCheckBox->setEnabled(!useGlobal);
        m_volumeSlider->setEnabled(!useGlobal);
        m_noAutoMuteCheckBox->setEnabled(!useGlobal);
        m_noAudioProcessingCheckBox->setEnabled(!useGlobal);
        m_fpsSpinBox->setEnabled(!useGlobal);
        m_windowGeometryEdit->setEnabled(!useGlobal);
        m_screenRootCombo->setEnabled(!useGlobal && m_customScreenRootEdit->text().isEmpty());
        m_customScreenRootEdit->setEnabled(!useGlobal);
        m_backgroundIdEdit->setEnabled(!useGlobal);
        m_scalingCombo->setEnabled(!useGlobal);
        m_clampingCombo->setEnabled(!useGlobal);
        m_disableMouseCheckBox->setEnabled(!useGlobal);
        m_disableParallaxCheckBox->setEnabled(!useGlobal);
        m_noFullscreenPauseCheckBox->setEnabled(!useGlobal);
        m_noLoopCheckBox->setEnabled(!useGlobal);
        m_noHardwareDecodeCheckBox->setEnabled(!useGlobal);
        m_forceX11CheckBox->setEnabled(!useGlobal);
        m_forceWaylandCheckBox->setEnabled(!useGlobal);
        m_verboseCheckBox->setEnabled(!useGlobal);
        m_logLevelCombo->setEnabled(!useGlobal);
        m_mpvOptionsEdit->setEnabled(!useGlobal);
        
        // Update info label
        if (useGlobal) {
            infoLabel->setText(
                "✓ Using system-wide defaults - Settings are managed in Settings → Engine Defaults\n"
                "  Uncheck to customize settings for this wallpaper only");
            infoLabel->setStyleSheet("QLabel { color: #666; font-size: 10px; margin-top: 4px; }");
        } else {
            infoLabel->setText(
                "✓ Custom settings enabled - Changes apply only to this wallpaper\n"
                "  Check the box above to revert to system-wide defaults");
            infoLabel->setStyleSheet("QLabel { color: #0066cc; font-size: 10px; margin-top: 4px; }");
        }
        
        // Mark as modified
        onSettingChanged();
    });
    
    // Audio Settings Group
    auto* audioGroup = new QGroupBox("Audio Settings");
    auto* audioLayout = new QFormLayout(audioGroup);
    audioLayout->setContentsMargins(12, 16, 12, 12);
    audioLayout->setVerticalSpacing(16);
    audioLayout->setHorizontalSpacing(24);
    audioLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    
    // Silent mode
    m_silentCheckBox->setMinimumHeight(28);
    audioLayout->addRow("", m_silentCheckBox);
    
    // Volume slider with proper layout
    auto* volumeWidget = new QWidget;
    auto* volumeLayout = new QHBoxLayout(volumeWidget);
    volumeLayout->setContentsMargins(0, 0, 0, 0);
    volumeLayout->setSpacing(12);
    
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(15);
    m_volumeSlider->setMinimumWidth(200);
    m_volumeSlider->setMinimumHeight(28);
    m_volumeSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    m_volumeSpinBox->setRange(0, 100);
    m_volumeSpinBox->setValue(15);
    m_volumeSpinBox->setSuffix("%");
    m_volumeSpinBox->setMinimumWidth(80);
    m_volumeSpinBox->setMinimumHeight(28);
    m_volumeSpinBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    volumeLayout->addWidget(m_volumeSlider);
    volumeLayout->addWidget(m_volumeSpinBox);
    
    auto* volumeLabel = new QLabel("Volume:");
    volumeLabel->setMinimumWidth(80);
    volumeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    volumeLabel->setStyleSheet("font-weight: bold;");
    audioLayout->addRow(volumeLabel, volumeWidget);
    
    // Other audio checkboxes
    m_noAutoMuteCheckBox->setMinimumHeight(28);
    audioLayout->addRow("", m_noAutoMuteCheckBox);
    
    // Audio processing - not supported by WNEL, will be hidden for external wallpapers
    m_noAudioProcessingCheckBox->setMinimumHeight(28);
    m_noAudioProcessingWidget = new QWidget;
    auto* noAudioProcessingLayout = new QHBoxLayout(m_noAudioProcessingWidget);
    noAudioProcessingLayout->setContentsMargins(0, 0, 0, 0);
    noAudioProcessingLayout->addWidget(m_noAudioProcessingCheckBox);
    audioLayout->addRow("", m_noAudioProcessingWidget);
    
    // Store reference for hiding later
    m_noAudioProcessingWidget->setObjectName("noAudioProcessingWidget");
    
    scrollLayout->addWidget(audioGroup);
    
    // Performance Settings Group
    auto* perfGroup = new QGroupBox("Performance Settings");
    auto* perfLayout = new QFormLayout(perfGroup);
    perfLayout->setContentsMargins(12, 16, 12, 12);
    perfLayout->setVerticalSpacing(16);
    perfLayout->setHorizontalSpacing(24);
    perfLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    
    m_fpsSpinBox->setRange(1, 144);
    m_fpsSpinBox->setValue(30);
    m_fpsSpinBox->setSuffix(" FPS");
    m_fpsSpinBox->setMinimumWidth(120);
    m_fpsSpinBox->setMinimumHeight(28);
    m_fpsSpinBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    auto* fpsLabel = new QLabel("Target FPS:");
    fpsLabel->setMinimumWidth(80);
    fpsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    fpsLabel->setStyleSheet("font-weight: bold;");
    perfLayout->addRow(fpsLabel, m_fpsSpinBox);
    
    scrollLayout->addWidget(perfGroup);
    
    // Display Settings Group
    auto* displayGroup = new QGroupBox("Display Settings");
    auto* displayLayout = new QFormLayout(displayGroup);
    displayLayout->setContentsMargins(12, 16, 12, 12);
    displayLayout->setVerticalSpacing(16);
    displayLayout->setHorizontalSpacing(24);
    displayLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    
    auto createDisplayLabel = [](const QString& text) {
        auto* label = new QLabel(text);
        label->setMinimumWidth(100);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setStyleSheet("font-weight: bold;");
        return label;
    };
    
    // Window Geometry - not supported by WNEL, will be hidden for external wallpapers
    m_windowGeometryEdit->setPlaceholderText("e.g., 1920x1080+0+0");
    m_windowGeometryEdit->setMinimumWidth(200);
    m_windowGeometryEdit->setMinimumHeight(28);
    m_windowGeometryWidget = new QWidget;
    auto* windowGeometryLayout = new QHBoxLayout(m_windowGeometryWidget);
    windowGeometryLayout->setContentsMargins(0, 0, 0, 0);
    windowGeometryLayout->addWidget(m_windowGeometryEdit);
    m_windowGeometryLabel = createDisplayLabel("Window Geometry:");
    displayLayout->addRow(m_windowGeometryLabel, m_windowGeometryWidget);
    m_windowGeometryWidget->setObjectName("windowGeometryWidget");
    
    // Screen root with proper sizing
    m_screenRootCombo->setMinimumWidth(200);
    m_screenRootCombo->setMinimumHeight(28);
    QStringList screens = getAvailableScreens();
    m_screenRootCombo->addItems(screens);
    displayLayout->addRow(createDisplayLabel("Screen Root:"), m_screenRootCombo);
    
    // Custom screen root override
    m_customScreenRootEdit = new QLineEdit(this);
    m_customScreenRootEdit->setPlaceholderText("Custom screen (overrides selection above)");
    m_customScreenRootEdit->setMinimumWidth(200);
    m_customScreenRootEdit->setMinimumHeight(28);
    connect(m_customScreenRootEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_screenRootCombo->setEnabled(text.isEmpty());
        m_currentSettings.customScreenRoot = text;  // Update the setting immediately
        onSettingChanged();  // Trigger settings update
    });
    displayLayout->addRow(createDisplayLabel("Custom Screen:"), m_customScreenRootEdit);

    // Background ID - not supported by WNEL, will be hidden for external wallpapers
    m_backgroundIdEdit->setPlaceholderText("Background ID");
    m_backgroundIdEdit->setMinimumWidth(200);
    m_backgroundIdEdit->setMinimumHeight(28);
    m_backgroundIdWidget = new QWidget;
    auto* backgroundIdLayout = new QHBoxLayout(m_backgroundIdWidget);
    backgroundIdLayout->setContentsMargins(0, 0, 0, 0);
    backgroundIdLayout->addWidget(m_backgroundIdEdit);
    m_backgroundIdLabel = createDisplayLabel("Background ID:");
    displayLayout->addRow(m_backgroundIdLabel, m_backgroundIdWidget);
    m_backgroundIdWidget->setObjectName("backgroundIdWidget");
    
    // Scaling combo
    m_scalingCombo->addItems({"default", "stretch", "fit", "fill"});
    m_scalingCombo->setMinimumWidth(150);
    m_scalingCombo->setMinimumHeight(28);
    displayLayout->addRow(createDisplayLabel("Scaling:"), m_scalingCombo);
    
    // Clamping combo - not supported by WNEL, will be hidden for external wallpapers
    m_clampingCombo->addItems({"clamp", "border", "repeat"});
    m_clampingCombo->setMinimumWidth(150);
    m_clampingCombo->setMinimumHeight(28);
    m_clampingWidget = new QWidget;
    auto* clampingLayout = new QHBoxLayout(m_clampingWidget);
    clampingLayout->setContentsMargins(0, 0, 0, 0);
    clampingLayout->addWidget(m_clampingCombo);
    m_clampingLabel = createDisplayLabel("Clamping:");
    displayLayout->addRow(m_clampingLabel, m_clampingWidget);
    m_clampingWidget->setObjectName("clampingWidget");
    
    scrollLayout->addWidget(displayGroup);
    
    // Behavior Settings Group
    auto* behaviorGroup = new QGroupBox("Behavior Settings");
    auto* behaviorLayout = new QVBoxLayout(behaviorGroup);
    behaviorLayout->setContentsMargins(12, 16, 12, 12);
    behaviorLayout->setSpacing(12);
    
    m_disableMouseCheckBox->setMinimumHeight(28);
    behaviorLayout->addWidget(m_disableMouseCheckBox);
    
    m_disableParallaxCheckBox->setMinimumHeight(28);
    behaviorLayout->addWidget(m_disableParallaxCheckBox);
    
    m_noFullscreenPauseCheckBox->setMinimumHeight(28);
    behaviorLayout->addWidget(m_noFullscreenPauseCheckBox);
    
    scrollLayout->addWidget(behaviorGroup);
    
    // WNEL-Specific Settings Group (only visible for external wallpapers)
    auto* wnelGroup = new QGroupBox("WNEL-Specific Settings");
    auto* wnelLayout = new QFormLayout(wnelGroup);
    wnelLayout->setContentsMargins(12, 16, 12, 12);
    wnelLayout->setVerticalSpacing(16);
    wnelLayout->setHorizontalSpacing(24);
    wnelLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    
    // Video settings
    m_noLoopCheckBox->setMinimumHeight(28);
    wnelLayout->addRow("", m_noLoopCheckBox);
    
    m_noHardwareDecodeCheckBox->setMinimumHeight(28);
    wnelLayout->addRow("", m_noHardwareDecodeCheckBox);
    
    // Backend settings - make them mutually exclusive
    m_forceX11CheckBox->setMinimumHeight(28);
    m_forceWaylandCheckBox->setMinimumHeight(28);
    wnelLayout->addRow("", m_forceX11CheckBox);
    wnelLayout->addRow("", m_forceWaylandCheckBox);
    
    // Make X11 and Wayland checkboxes mutually exclusive
    connect(m_forceX11CheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) m_forceWaylandCheckBox->setChecked(false);
        onSettingChanged();
    });
    connect(m_forceWaylandCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) m_forceX11CheckBox->setChecked(false);
        onSettingChanged();
    });
    
    // Debug settings
    m_verboseCheckBox->setMinimumHeight(28);
    wnelLayout->addRow("", m_verboseCheckBox);
    
    // Log level
    m_logLevelCombo->addItems({"debug", "info", "warn", "error"});
    m_logLevelCombo->setCurrentText("info");
    m_logLevelCombo->setMinimumHeight(28);
    auto* logLevelLabel = new QLabel("Log Level:");
    logLevelLabel->setMinimumWidth(80);
    logLevelLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    logLevelLabel->setStyleSheet("font-weight: bold;");
    wnelLayout->addRow(logLevelLabel, m_logLevelCombo);
    
    // MPV options
    m_mpvOptionsEdit->setPlaceholderText("Additional MPV options (advanced)");
    m_mpvOptionsEdit->setMinimumHeight(28);
    auto* mpvLabel = new QLabel("MPV Options:");
    mpvLabel->setMinimumWidth(80);
    mpvLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mpvLabel->setStyleSheet("font-weight: bold;");
    wnelLayout->addRow(mpvLabel, m_mpvOptionsEdit);
    
    scrollLayout->addWidget(wnelGroup);
    
    // Connect WNEL-specific signals
    connect(m_noLoopCheckBox, &QCheckBox::toggled, this, &PropertiesPanel::onSettingChanged);
    connect(m_noHardwareDecodeCheckBox, &QCheckBox::toggled, this, &PropertiesPanel::onSettingChanged);
    connect(m_verboseCheckBox, &QCheckBox::toggled, this, &PropertiesPanel::onSettingChanged);
    connect(m_logLevelCombo, &QComboBox::currentTextChanged, this, &PropertiesPanel::onSettingChanged);
    connect(m_mpvOptionsEdit, &QLineEdit::textChanged, this, &PropertiesPanel::onSettingChanged);
    
    // Store reference to WNEL group for visibility control
    wnelGroup->setObjectName("wnelGroup");
    
    // Add stretch to push everything to the top
    scrollLayout->addStretch();
    
    // Set the scroll widget
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);
    
    // Settings buttons - add them to the scroll widget so they scroll with content
    auto* settingsButtonLayout = new QHBoxLayout;
    settingsButtonLayout->setContentsMargins(0, 16, 0, 16);
    settingsButtonLayout->addStretch();
    m_saveSettingsButton->setMinimumHeight(32);
    settingsButtonLayout->addWidget(m_saveSettingsButton);
    
    scrollLayout->addLayout(settingsButtonLayout);
}

void PropertiesPanel::setWallpaper(const WallpaperInfo& wallpaper)
{
    qCDebug(propertiesPanel) << "setWallpaper called for:" << wallpaper.name;
    
    // Check for unsaved changes before switching wallpapers
    if (!m_currentWallpaper.id.isEmpty() && m_currentWallpaper.id != wallpaper.id) {
        if (!checkUnsavedChangesBeforeAction()) {
            // User chose to stay with current wallpaper, don't switch
            // Emit signal to notify MainWindow that selection was rejected
            emit wallpaperSelectionRejected(m_currentWallpaper.id);
            return;
        }
    }
    
    m_currentWallpaper = wallpaper;
    bool isExternalWallpaper = (wallpaper.type == "External");
    
    // For external wallpapers, validate that the wallpaper still exists
    if (isExternalWallpaper) {
        QString filePath = getExternalWallpaperFilePath(wallpaper.id);
        if (filePath.isEmpty()) {
            // External wallpaper files are missing, show warning and clear panel
            qCWarning(propertiesPanel) << "External wallpaper files missing for:" << wallpaper.id;
            clear();
            m_nameLabel->setText("External wallpaper not found");
            m_nameLabel->setToolTip("The external wallpaper files are missing or corrupted");
            m_authorLabel->setText("Local");
            m_typeLabel->setText("External (Missing)");
            m_descriptionEdit->setPlainText("This external wallpaper appears to be missing or corrupted. The wallpaper files may have been deleted or moved.");
            setPlaceholderPreview("External wallpaper not found");
            return;
        }
    }
    
    // Update tab visibility based on wallpaper type
    if (isExternalWallpaper) {
        // For external wallpapers, show only Info and External Settings tabs
        m_innerTabWidget->setTabText(1, "External Settings");
        m_innerTabWidget->setTabEnabled(1, true);
        m_innerTabWidget->setTabEnabled(2, true); // Engine Settings for WNEL
        m_innerTabWidget->setTabEnabled(3, true); // Engine Log
    } else {
        // For regular wallpapers, show all tabs normally
        m_innerTabWidget->setTabText(1, "Wallpaper Settings");
        m_innerTabWidget->setTabEnabled(1, true);
        m_innerTabWidget->setTabEnabled(2, true);
        m_innerTabWidget->setTabEnabled(3, true);
    }
    
    // Update WNEL-specific settings visibility
    updateWNELSettingsVisibility(isExternalWallpaper);
    
    // Update UI visibility based on wallpaper type
    updateUIVisibilityForWallpaperType(isExternalWallpaper);
    
    if (isExternalWallpaper) {
        // For external wallpapers, show the file path instead of ID
        QString filePath = getExternalWallpaperFilePath(wallpaper.id);
        m_previewIdLabel->setText(filePath.isEmpty() ? "File path not available" : filePath);
        
        // Set author to "Local" for external wallpapers
        m_authorLabel->setText("Local");
    } else {
        // Update ID label for regular wallpapers
        m_previewIdLabel->setText(wallpaper.id.isEmpty() ? "-" : wallpaper.id);
        m_authorLabel->setText(wallpaper.author.isEmpty() ? "Unknown" : wallpaper.author);
    }

    // Update basic info
    m_nameLabel->setText(wallpaper.name.isEmpty() ? "Unknown" : wallpaper.name);
    m_typeLabel->setText(wallpaper.type.isEmpty() ? "Unknown" : wallpaper.type);
    m_fileSizeLabel->setText(formatFileSize(wallpaper.fileSize));
    
    if (!isExternalWallpaper) {
        // Steam-specific data only for regular wallpapers
        // Update dates
        if (wallpaper.created.isValid()) {
            m_postedLabel->setText(wallpaper.created.toString("yyyy-MM-dd"));
        } else {
            m_postedLabel->setText("Unknown");
        }
        
        if (wallpaper.updated.isValid()) {
            m_updatedLabel->setText(wallpaper.updated.toString("yyyy-MM-dd"));
        } else {
            m_updatedLabel->setText("Unknown");
        }
        
        // Update stats
        m_viewsLabel->setText("Unknown");
        m_subscriptionsLabel->setText("Unknown");
        m_favoritesLabel->setText("Unknown");
    }
    
    // Update description
    if (!wallpaper.description.isEmpty()) {
        m_descriptionEdit->setText(wallpaper.description);
    } else {
        m_descriptionEdit->setText("No description available.");
    }
    
    // Update preview
    updatePreview(wallpaper);
    
    // Update properties - reload fresh from project.json file to get any saved changes
    QJsonObject freshProperties = loadPropertiesFromProjectJson(wallpaper.id);
    if (!freshProperties.isEmpty()) {
        updateProperties(freshProperties);
    } else {
        // Fallback to cached properties if reading from file fails
        updateProperties(wallpaper.properties);
    }
    
    // Load and update settings for this wallpaper
    loadWallpaperSettings(wallpaper.id);
    
    // Enable launch button
    m_launchButton->setEnabled(!wallpaper.id.isEmpty());
    
    // Check if backup exists to enable/disable reset button
    QString backupPath = getBackupProjectJsonPath(wallpaper.id);
    m_resetPropertiesButton->setEnabled(!wallpaper.id.isEmpty() && QFileInfo::exists(backupPath));
    
    // Fetch Steam API metadata if available
    updateSteamApiMetadata(wallpaper);
    
    qCDebug(propertiesPanel) << "setWallpaper completed for:" << wallpaper.name;
}

void PropertiesPanel::setWallpaperManager(WallpaperManager* manager)
{
    m_wallpaperManager = manager;
}

void PropertiesPanel::updatePreview(const WallpaperInfo& wallpaper)
{
    qCDebug(propertiesPanel) << "updatePreview called for preview path:" << wallpaper.previewPath;
    
    // Stop any existing animation first
    stopPreviewAnimation();
    
    if (!wallpaper.previewPath.isEmpty() && QFileInfo::exists(wallpaper.previewPath)) {
        // Check if it's an animated preview first
        if (hasAnimatedPreview(wallpaper.previewPath)) {
            loadAnimatedPreview(wallpaper.previewPath);
            return;
        }
        
        // Load static image
        QPixmap originalPixmap(wallpaper.previewPath);
        
        if (!originalPixmap.isNull()) {
            qCDebug(propertiesPanel) << "Loaded preview image, original size:" 
                                    << originalPixmap.width() << "x" << originalPixmap.height();
            
            // Scale the image properly while maintaining aspect ratio
            QSize labelSize = m_previewLabel->size();
            if (labelSize.width() < 50 || labelSize.height() < 50) {
                labelSize = QSize(256, 144); // Use default size if label isn't sized yet
            }
            
            QPixmap scaledPixmap = scalePixmapKeepAspectRatio(originalPixmap, labelSize);
            m_previewLabel->setPixmap(scaledPixmap);
            
            qCDebug(propertiesPanel) << "Preview image set successfully, scaled to:" 
                                    << scaledPixmap.width() << "x" << scaledPixmap.height();
        } else {
            qCWarning(propertiesPanel) << "Failed to load preview image:" << wallpaper.previewPath;
            setPlaceholderPreview("Failed to load preview");
        }
    } else {
        qCDebug(propertiesPanel) << "No valid preview path, setting placeholder";
        setPlaceholderPreview("No preview available");
    }
}

QPixmap PropertiesPanel::scalePixmapKeepAspectRatio(const QPixmap& original, const QSize& targetSize)
{
    if (original.isNull() || targetSize.isEmpty()) {
        return original;
    }
    
    // Calculate the best fit size while maintaining aspect ratio
    QSize originalSize = original.size();
    QSize scaledSize = originalSize.scaled(targetSize, Qt::KeepAspectRatio);
    
    // Scale the pixmap
    QPixmap scaled = original.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // Create a pixmap with the target size and center the scaled image
    QPixmap result(targetSize);
    result.fill(Qt::transparent);
    
    QPainter painter(&result);
    int x = (targetSize.width() - scaled.width()) / 2;
    int y = (targetSize.height() - scaled.height()) / 2;
    painter.drawPixmap(x, y, scaled);
    
    return result;
}

void PropertiesPanel::setPlaceholderPreview(const QString& text)
{
    QSize labelSize = m_previewLabel->size();
    if (labelSize.width() < 50 || labelSize.height() < 50) {
        labelSize = QSize(256, 144);
    }
    
    QPixmap placeholder(labelSize);
    placeholder.fill(QColor(245, 245, 245));
    
    QPainter painter(&placeholder);
    painter.setPen(QColor(149, 165, 166));
    painter.setFont(QFont("Arial", 12));
    
    QRect textRect = placeholder.rect().adjusted(10, 10, -10, -10);
    painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, text);
    
    m_previewLabel->setPixmap(placeholder);
}

void PropertiesPanel::updateProperties(const QJsonObject& properties)
{
    // Clear existing properties widget
    if (m_propertiesWidget) {
        m_propertiesWidget->deleteLater();
    }
    
    bool isExternalWallpaper = (m_currentWallpaper.type == "External");
    
    m_propertiesWidget = new QWidget;
    
    if (isExternalWallpaper) {
        // Create external wallpaper settings UI
        setupExternalWallpaperUI();
    } else {
        // Create regular properties UI
        auto* layout = new QFormLayout(m_propertiesWidget);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setVerticalSpacing(12);
        layout->setHorizontalSpacing(16);
        layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
        layout->setLabelAlignment(Qt::AlignLeft);
        
        m_propertyWidgets.clear();
        m_originalValues.clear();
        m_originalPropertyObjects.clear();
        
        if (properties.isEmpty()) {
            auto* noPropsLabel = new QLabel("No properties available");
            noPropsLabel->setStyleSheet("color: #666; font-style: italic; padding: 20px;");
            noPropsLabel->setAlignment(Qt::AlignCenter);
            layout->addRow(noPropsLabel);
        } else {
            addPropertiesFromObject(layout, properties, "");
        }
        
        m_propertiesModified = false;
        m_savePropertiesButton->setEnabled(false);
        
        // Update reset button based on backup availability
        if (!m_currentWallpaper.id.isEmpty()) {
            QString backupPath = getBackupProjectJsonPath(m_currentWallpaper.id);
            m_resetPropertiesButton->setEnabled(QFileInfo::exists(backupPath));
        } else {
            m_resetPropertiesButton->setEnabled(false);
        }
    }
    
    // Ensure proper sizing
    m_propertiesWidget->setMinimumSize(m_propertiesWidget->sizeHint());
    m_scrollArea->setWidget(m_propertiesWidget);
}

void PropertiesPanel::addPropertiesFromObject(QFormLayout* layout, const QJsonObject& properties, const QString& prefix)
{
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        QString propName = prefix.isEmpty() ? it.key() : prefix + "." + it.key();
        QJsonValue propValue = it.value();
        
        if (propValue.isObject()) {
            QJsonObject propObj = propValue.toObject();
            QString type = propObj.value("type").toString();
            QString text = propObj.value("text").toString();
            QJsonValue value = propObj.value("value");
            
            // Skip if no type or value
            if (type.isEmpty() || value.isUndefined()) {
                continue;
            }
            
            QString displayName = text.isEmpty() ? propName : text;
            
            QWidget* widget = createPropertyWidget(propName, type, value, propObj);
            if (widget) {
                layout->addRow(displayName + ":", widget);
                
                // Store original value and full property object for comparison
                m_originalValues[propName] = value;
                m_originalPropertyObjects[propName] = propObj;
            }
        }
    }
}

QWidget* PropertiesPanel::createPropertyWidget(const QString& propName, const QString& type, const QJsonValue& value, const QJsonObject& propertyObj)
{
    QWidget* widget = nullptr;
    
    if (type == "bool") {
        auto* checkBox = new QCheckBox;
        checkBox->setChecked(value.toBool());
        checkBox->setMinimumHeight(28);
        checkBox->setProperty("propertyType", type);  // Store the type
        connect(checkBox, &QCheckBox::toggled, this, &PropertiesPanel::onPropertyChanged);
        widget = checkBox;
        
    } else if (type == "slider") {
        auto* container = new QWidget;
        auto* layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);
        
        auto* slider = new QSlider(Qt::Horizontal);
        auto* label = new QLabel;
        
        double minVal = propertyObj.value("min").toDouble(0.0);
        double maxVal = propertyObj.value("max").toDouble(100.0);
        double step = propertyObj.value("step").toDouble(1.0);
        
        int steps = static_cast<int>((maxVal - minVal) / step);
        slider->setRange(0, steps);
        slider->setValue(static_cast<int>((value.toDouble() - minVal) / step));
        slider->setMinimumWidth(150);
        slider->setMinimumHeight(28);
        
        // Store metadata for value calculation
        slider->setProperty("minValue", minVal);
        slider->setProperty("maxValue", maxVal);
        slider->setProperty("step", step);
        
        label->setText(QString::number(value.toDouble(), 'f', 2));
        label->setMinimumWidth(60);
        label->setMinimumHeight(28);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("border: 1px solid #c0c0c0; padding: 4px; background: white;");
        
        connect(slider, &QSlider::valueChanged, this, [=](int val) {
            double realValue = minVal + (val * step);
            label->setText(QString::number(realValue, 'f', 2));
            onPropertyChanged();
        });
        
        layout->addWidget(slider);
        layout->addWidget(label);
        
        container->setProperty("propertyType", type);  // Store the type
        widget = container;
        
    } else if (type == "combo" || type == "textinput") {
        if (propertyObj.contains("options")) {
            auto* combo = new QComboBox;
            combo->setMinimumWidth(150);
            combo->setMinimumHeight(28);
            combo->setProperty("propertyType", type);  // Store the type
            
            QJsonArray options = propertyObj.value("options").toArray();
            for (const QJsonValue& option : options) {
                QJsonObject optionObj = option.toObject();
                QString label = optionObj.value("label").toString();
                QString optionValue = optionObj.value("value").toString();
                combo->addItem(label, optionValue);
                
                if (optionValue == value.toString()) {
                    combo->setCurrentIndex(combo->count() - 1);
                }
            }
            
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &PropertiesPanel::onPropertyChanged);
            widget = combo;
        } else {
            auto* lineEdit = new QLineEdit;
            lineEdit->setText(value.toString());
            lineEdit->setMinimumWidth(150);
            lineEdit->setMinimumHeight(28);
            lineEdit->setProperty("propertyType", type);  // Store the type
            connect(lineEdit, &QLineEdit::textChanged, this, &PropertiesPanel::onPropertyChanged);
            widget = lineEdit;
        }
    } else if (type == "color") {
        auto* lineEdit = new QLineEdit;
        lineEdit->setText(value.toString());
        lineEdit->setMinimumWidth(150);
        lineEdit->setMinimumHeight(28);
        lineEdit->setProperty("propertyType", type);  // Store the type
        lineEdit->setPlaceholderText("RGB color (e.g., 1.0 0.5 0.0)");
        connect(lineEdit, &QLineEdit::textChanged, this, &PropertiesPanel::onPropertyChanged);
        widget = lineEdit;
    } else if (type == "int") {
        auto* spinBox = new QSpinBox;
        spinBox->setRange(propertyObj.value("min").toInt(-999999), 
                         propertyObj.value("max").toInt(999999));
        spinBox->setValue(value.toInt());
        spinBox->setMinimumWidth(150);
        spinBox->setMinimumHeight(28);
        spinBox->setProperty("propertyType", type);  // Store the type
        connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
                this, &PropertiesPanel::onPropertyChanged);
        widget = spinBox;
    } else if (type == "float") {
        auto* doubleSpinBox = new QDoubleSpinBox;
        doubleSpinBox->setRange(propertyObj.value("min").toDouble(-999999.0), 
                               propertyObj.value("max").toDouble(999999.0));
        doubleSpinBox->setValue(value.toDouble());
        doubleSpinBox->setDecimals(3);
        doubleSpinBox->setMinimumWidth(150);
        doubleSpinBox->setMinimumHeight(28);
        doubleSpinBox->setProperty("propertyType", type);  // Store the type
        connect(doubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
                this, &PropertiesPanel::onPropertyChanged);
        widget = doubleSpinBox;
    } else {
        // Default to line edit for unknown types
        auto* lineEdit = new QLineEdit;
        lineEdit->setText(value.toString());
        lineEdit->setMinimumWidth(150);
        lineEdit->setMinimumHeight(28);
        lineEdit->setProperty("propertyType", "textinput");  // Default type
        connect(lineEdit, &QLineEdit::textChanged, this, &PropertiesPanel::onPropertyChanged);
        widget = lineEdit;
    }
    
    if (widget) {
        widget->setProperty("propertyName", propName);
        m_propertyWidgets[propName] = widget;
        m_originalValues[propName] = value;
        m_originalPropertyObjects[propName] = propertyObj;
    }
    
    return widget;
}

void PropertiesPanel::onPropertyChanged()
{
    // Mark properties as modified to enable the save button
    m_propertiesModified = true;
    m_savePropertiesButton->setEnabled(true);
    m_resetPropertiesButton->setEnabled(true);
    
    // Provide visual indication the properties have changed
    m_savePropertiesButton->setStyleSheet("QPushButton { background-color: #4CAF50; font-weight: bold; }");
}

void PropertiesPanel::onSavePropertiesClicked()
{
    QJsonObject modifiedProperties = saveCurrentProperties();
    
    // Save to original project.json file instead of cache
    bool saved = savePropertiesToProjectJson(m_currentWallpaper.id, modifiedProperties);
    if (saved) {
        qCDebug(propertiesPanel) << "Properties saved successfully to project.json for wallpaper:" << m_currentWallpaper.id;
        
        // Reset the modified state
        m_propertiesModified = false;
        m_savePropertiesButton->setEnabled(false);
        m_savePropertiesButton->setStyleSheet("");
        
        // Reload properties from the saved project.json to reflect changes in UI
        QJsonObject freshProperties = loadPropertiesFromProjectJson(m_currentWallpaper.id);
        if (!freshProperties.isEmpty()) {
            updateProperties(freshProperties);
        }
        
        // Automatically restart wallpaper if it's currently running and matches the modified wallpaper
        restartWallpaperWithChanges();
    } else {
        qCWarning(propertiesPanel) << "Failed to save properties to project.json for wallpaper:" << m_currentWallpaper.id;
    }
}

void PropertiesPanel::onResetPropertiesClicked()
{
    bool reset = resetPropertiesFromBackup(m_currentWallpaper.id);
    if (reset) {
        qCDebug(propertiesPanel) << "Properties reset successfully from backup for wallpaper:" << m_currentWallpaper.id;
        
        // Reload the wallpaper to refresh the properties panel
        setWallpaper(m_currentWallpaper);
        
        // Automatically restart wallpaper if it's currently running and matches the modified wallpaper
        restartWallpaperWithChanges();
    } else {
        qCWarning(propertiesPanel) << "Failed to reset properties from backup for wallpaper:" << m_currentWallpaper.id;
    }
}

void PropertiesPanel::onSettingChanged()
{
    // Update useGlobalDefaults flag
    m_currentSettings.useGlobalDefaults = m_useGlobalDefaultsCheckBox->isChecked();
    
    // Update current settings from UI controls
    m_currentSettings.silent = m_silentCheckBox->isChecked();
    m_currentSettings.volume = m_volumeSlider->value();
    m_currentSettings.noAutoMute = m_noAutoMuteCheckBox->isChecked();
    m_currentSettings.noAudioProcessing = m_noAudioProcessingCheckBox->isChecked();
    m_currentSettings.fps = m_fpsSpinBox->value();
    m_currentSettings.windowGeometry = m_windowGeometryEdit->text();
    QString selectedScreen = m_screenRootCombo->currentText();
    // Strip out the resolution info if present
    if (selectedScreen.contains("(")) {
        selectedScreen = selectedScreen.left(selectedScreen.indexOf("(")).trimmed();
    }
    m_currentSettings.screenRoot = selectedScreen == "Default" ? "" : selectedScreen;
    m_currentSettings.customScreenRoot = m_customScreenRootEdit->text();
    m_currentSettings.backgroundId = m_backgroundIdEdit->text();
    m_currentSettings.scaling = m_scalingCombo->currentText();
    m_currentSettings.clamping = m_clampingCombo->currentText();
    m_currentSettings.disableMouse = m_disableMouseCheckBox->isChecked();
    m_currentSettings.disableParallax = m_disableParallaxCheckBox->isChecked();
    m_currentSettings.noFullscreenPause = m_noFullscreenPauseCheckBox->isChecked();
    
    // Update WNEL-specific settings
    m_currentSettings.noLoop = m_noLoopCheckBox->isChecked();
    m_currentSettings.noHardwareDecode = m_noHardwareDecodeCheckBox->isChecked();
    m_currentSettings.forceX11 = m_forceX11CheckBox->isChecked();
    m_currentSettings.forceWayland = m_forceWaylandCheckBox->isChecked();
    m_currentSettings.verbose = m_verboseCheckBox->isChecked();
    m_currentSettings.logLevel = m_logLevelCombo->currentText();
    m_currentSettings.mpvOptions = m_mpvOptionsEdit->text();
    
    m_settingsModified = true;
    m_saveSettingsButton->setEnabled(true);
    
    // Emit signal to notify that settings have changed
    bool isExternalWallpaper = (m_currentWallpaper.type == "External");
    emit settingsChanged(m_currentWallpaper.id, m_currentSettings, isExternalWallpaper);
}

void PropertiesPanel::onSaveSettingsClicked()
{
    if (!m_currentWallpaper.id.isEmpty()) {
        if (saveWallpaperSettings(m_currentWallpaper.id)) {
            m_settingsModified = false;
            m_saveSettingsButton->setEnabled(false);
            qCDebug(propertiesPanel) << "Settings saved successfully for wallpaper:" << m_currentWallpaper.id;
            
            // Automatically restart wallpaper if it's currently running and matches the modified wallpaper
            restartWallpaperWithChanges();
        } else {
            qCWarning(propertiesPanel) << "Failed to save settings for wallpaper:" << m_currentWallpaper.id;
        }
    }
}

void PropertiesPanel::onScreenRootChanged(const QString& screenRoot)
{
    Q_UNUSED(screenRoot)  // Suppress unused parameter warning
    onSettingChanged();
}

void PropertiesPanel::onSaveExternalNameClicked()
{
    QString newName = m_externalNameEdit->text().trimmed();
    if (newName.isEmpty()) {
        QMessageBox::warning(this, "Invalid Name", "Please enter a valid name for the wallpaper.");
        return;
    }
    
    if (newName == m_currentWallpaper.name) {
        m_saveExternalNameButton->setEnabled(false);
        return;
    }
    
    // Update the wallpaper info
    m_currentWallpaper.name = newName;
    
    // Save the updated name to the external wallpaper's project.json file
    // The ID contains the file path for external wallpapers
    QString projectJsonPath = QDir(QFileInfo(m_currentWallpaper.id).absoluteDir()).absoluteFilePath("project.json");
    
    QJsonObject projectData;
    projectData["name"] = newName;
    projectData["description"] = m_currentWallpaper.description;
    projectData["file"] = QFileInfo(m_currentWallpaper.id).fileName();
    projectData["type"] = "External";
    
    QJsonDocument doc(projectData);
    QFile file(projectJsonPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        
        m_saveExternalNameButton->setEnabled(false);
        m_nameLabel->setText(newName); // Update the name label in the info section
        
        // Emit signal to notify that properties have changed
        emit propertiesChanged(m_currentWallpaper.id, projectData);
        
        qCDebug(propertiesPanel) << "External wallpaper name updated to:" << newName;
    } else {
        QMessageBox::warning(this, "Save Error", "Failed to save the wallpaper name. Please check file permissions.");
        qCWarning(propertiesPanel) << "Failed to save external wallpaper project.json:" << projectJsonPath;
    }
}

void PropertiesPanel::restartWallpaperWithChanges()
{
    if (m_currentWallpaper.id.isEmpty()) {
        qCWarning(propertiesPanel) << "Cannot restart wallpaper: no current wallpaper";
        return;
    }
    
    // Check if wallpaper manager is available
    if (!m_wallpaperManager) {
        qCWarning(propertiesPanel) << "Cannot restart wallpaper: no wallpaper manager";
        return;
    }
    
    // Only restart if the wallpaper is currently running and it's the same wallpaper being modified
    if (m_wallpaperManager->isWallpaperRunning() && 
        m_wallpaperManager->getCurrentWallpaper() == m_currentWallpaper.id) {
        
        qCDebug(propertiesPanel) << "Automatically restarting wallpaper with new changes:" << m_currentWallpaper.name;
        
        // Emit the launch signal to restart the wallpaper with new settings
        emit launchWallpaper(m_currentWallpaper);
    } else if (m_wallpaperManager->isWallpaperRunning()) {
        qCDebug(propertiesPanel) << "Wallpaper is running but it's not the current wallpaper being modified - no restart needed";
    } else {
        qCDebug(propertiesPanel) << "No wallpaper is currently running - no restart needed";
    }
}

bool PropertiesPanel::saveWallpaperSettings(const QString& wallpaperId)
{
    QString settingsPath = getSettingsFilePath(wallpaperId);
    
    // Create directory if it doesn't exist
    QDir settingsDir = QFileInfo(settingsPath).dir();
    if (!settingsDir.exists()) {
        settingsDir.mkpath(".");
    }
    
    QJsonObject settingsObj;
    settingsObj["silent"] = m_currentSettings.silent;
    settingsObj["volume"] = m_currentSettings.volume;
    settingsObj["noAutoMute"] = m_currentSettings.noAutoMute;
    settingsObj["noAudioProcessing"] = m_currentSettings.noAudioProcessing;
    settingsObj["fps"] = m_currentSettings.fps;
    settingsObj["windowGeometry"] = m_currentSettings.windowGeometry;
    settingsObj["screenRoot"] = m_currentSettings.screenRoot;
    settingsObj["customScreenRoot"] = m_currentSettings.customScreenRoot;
    settingsObj["backgroundId"] = m_currentSettings.backgroundId;
    settingsObj["scaling"] = m_currentSettings.scaling;
    settingsObj["clamping"] = m_currentSettings.clamping;
    settingsObj["disableMouse"] = m_currentSettings.disableMouse;
    settingsObj["disableParallax"] = m_currentSettings.disableParallax;
    settingsObj["noFullscreenPause"] = m_currentSettings.noFullscreenPause;
    
    // WNEL-specific settings
    settingsObj["noLoop"] = m_currentSettings.noLoop;
    settingsObj["noHardwareDecode"] = m_currentSettings.noHardwareDecode;
    settingsObj["forceX11"] = m_currentSettings.forceX11;
    settingsObj["forceWayland"] = m_currentSettings.forceWayland;
    settingsObj["verbose"] = m_currentSettings.verbose;
    settingsObj["logLevel"] = m_currentSettings.logLevel;
    settingsObj["mpvOptions"] = m_currentSettings.mpvOptions;
    
    // Save the useGlobalDefaults flag
    settingsObj["useGlobalDefaults"] = m_currentSettings.useGlobalDefaults;
    
    QJsonDocument doc(settingsObj);
    
    QFile file(settingsPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(propertiesPanel) << "Failed to open settings file for writing:" << settingsPath;
        return false;
    }
    
    file.write(doc.toJson());
    return true;
}

bool PropertiesPanel::loadWallpaperSettings(const QString& wallpaperId)
{
    QString settingsPath = getSettingsFilePath(wallpaperId);
    
    QFile file(settingsPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        // Use global defaults from ConfigManager
        ConfigManager& config = ConfigManager::instance();
        m_currentSettings.useGlobalDefaults = true;  // No settings file means use global defaults
        m_currentSettings.silent = config.globalSilent();
        m_currentSettings.volume = config.globalVolume();
        m_currentSettings.noAutoMute = config.globalNoAutoMute();
        m_currentSettings.noAudioProcessing = config.globalNoAudioProcessing();
        m_currentSettings.fps = config.globalFps();
        m_currentSettings.windowGeometry = config.globalWindowGeometry();
        m_currentSettings.screenRoot = config.globalScreenRoot();
        m_currentSettings.customScreenRoot = "";  // Custom screen root is per-wallpaper only
        m_currentSettings.backgroundId = config.globalBackgroundId();
        m_currentSettings.scaling = config.globalScaling();
        m_currentSettings.clamping = config.globalClamping();
        m_currentSettings.disableMouse = config.globalDisableMouse();
        m_currentSettings.disableParallax = config.globalDisableParallax();
        m_currentSettings.noFullscreenPause = config.globalNoFullscreenPause();
        m_currentSettings.noLoop = config.globalNoLoop();
        m_currentSettings.noHardwareDecode = config.globalNoHardwareDecode();
        m_currentSettings.forceX11 = config.globalForceX11();
        m_currentSettings.forceWayland = config.globalForceWayland();
        m_currentSettings.verbose = config.globalVerbose();
        m_currentSettings.logLevel = config.globalLogLevel();
        m_currentSettings.mpvOptions = config.globalMpvOptions();
        
        updateSettingsControls();
        return false;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        // Use global defaults from ConfigManager on parse error
        ConfigManager& config = ConfigManager::instance();
        m_currentSettings.useGlobalDefaults = true;  // Parse error means use global defaults
        m_currentSettings.silent = config.globalSilent();
        m_currentSettings.volume = config.globalVolume();
        m_currentSettings.noAutoMute = config.globalNoAutoMute();
        m_currentSettings.noAudioProcessing = config.globalNoAudioProcessing();
        m_currentSettings.fps = config.globalFps();
        m_currentSettings.windowGeometry = config.globalWindowGeometry();
        m_currentSettings.screenRoot = config.globalScreenRoot();
        m_currentSettings.customScreenRoot = "";
        m_currentSettings.backgroundId = config.globalBackgroundId();
        m_currentSettings.scaling = config.globalScaling();
        m_currentSettings.clamping = config.globalClamping();
        m_currentSettings.disableMouse = config.globalDisableMouse();
        m_currentSettings.disableParallax = config.globalDisableParallax();
        m_currentSettings.noFullscreenPause = config.globalNoFullscreenPause();
        m_currentSettings.noLoop = config.globalNoLoop();
        m_currentSettings.noHardwareDecode = config.globalNoHardwareDecode();
        m_currentSettings.forceX11 = config.globalForceX11();
        m_currentSettings.forceWayland = config.globalForceWayland();
        m_currentSettings.verbose = config.globalVerbose();
        m_currentSettings.logLevel = config.globalLogLevel();
        m_currentSettings.mpvOptions = config.globalMpvOptions();
        
        updateSettingsControls();
        return false;
    }
    
    // Load settings from JSON with fallback to global defaults
    ConfigManager& config = ConfigManager::instance();
    QJsonObject obj = doc.object();
    
    // Load useGlobalDefaults flag first (defaults to true if not present)
    m_currentSettings.useGlobalDefaults = obj.contains("useGlobalDefaults") ? obj["useGlobalDefaults"].toBool() : true;
    
    // If using global defaults, load all from config
    // If custom settings, load from JSON with fallback to global defaults for missing fields
    m_currentSettings.silent = obj.contains("silent") ? obj["silent"].toBool() : config.globalSilent();
    m_currentSettings.volume = obj.contains("volume") ? obj["volume"].toInt() : config.globalVolume();
    m_currentSettings.noAutoMute = obj.contains("noAutoMute") ? obj["noAutoMute"].toBool() : config.globalNoAutoMute();
    m_currentSettings.noAudioProcessing = obj.contains("noAudioProcessing") ? obj["noAudioProcessing"].toBool() : config.globalNoAudioProcessing();
    m_currentSettings.fps = obj.contains("fps") ? obj["fps"].toInt() : config.globalFps();
    m_currentSettings.windowGeometry = obj.contains("windowGeometry") ? obj["windowGeometry"].toString() : config.globalWindowGeometry();
    m_currentSettings.screenRoot = obj.contains("screenRoot") ? obj["screenRoot"].toString() : config.globalScreenRoot();
    m_currentSettings.customScreenRoot = obj["customScreenRoot"].toString();  // Always per-wallpaper
    m_currentSettings.backgroundId = obj.contains("backgroundId") ? obj["backgroundId"].toString() : config.globalBackgroundId();
    m_currentSettings.scaling = obj.contains("scaling") ? obj["scaling"].toString() : config.globalScaling();
    m_currentSettings.clamping = obj.contains("clamping") ? obj["clamping"].toString() : config.globalClamping();
    m_currentSettings.disableMouse = obj.contains("disableMouse") ? obj["disableMouse"].toBool() : config.globalDisableMouse();
    m_currentSettings.disableParallax = obj.contains("disableParallax") ? obj["disableParallax"].toBool() : config.globalDisableParallax();
    m_currentSettings.noFullscreenPause = obj.contains("noFullscreenPause") ? obj["noFullscreenPause"].toBool() : config.globalNoFullscreenPause();
    
    // WNEL-specific settings
    m_currentSettings.noLoop = obj.contains("noLoop") ? obj["noLoop"].toBool() : config.globalNoLoop();
    m_currentSettings.noHardwareDecode = obj.contains("noHardwareDecode") ? obj["noHardwareDecode"].toBool() : config.globalNoHardwareDecode();
    m_currentSettings.forceX11 = obj.contains("forceX11") ? obj["forceX11"].toBool() : config.globalForceX11();
    m_currentSettings.forceWayland = obj.contains("forceWayland") ? obj["forceWayland"].toBool() : config.globalForceWayland();
    m_currentSettings.verbose = obj.contains("verbose") ? obj["verbose"].toBool() : config.globalVerbose();
    m_currentSettings.logLevel = obj.contains("logLevel") ? obj["logLevel"].toString() : config.globalLogLevel();
    m_currentSettings.mpvOptions = obj.contains("mpvOptions") ? obj["mpvOptions"].toString() : config.globalMpvOptions();
    
    updateSettingsControls();
    return true;
}

QString PropertiesPanel::getSettingsFilePath(const QString& wallpaperId)
{
    QString settingsDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (settingsDir.isEmpty()) {
        settingsDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    }
    if (settingsDir.isEmpty()) {
        settingsDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.cache/wallpaperengine-gui";
    } else {
        settingsDir += "/wallpaperengine-gui";
    }
    settingsDir += "/settings";
    return settingsDir + "/" + wallpaperId + ".json";
}

QStringList PropertiesPanel::getAvailableScreens() const
{
    QStringList screens;
    screens << "Default";  // Always add Default option first

    // Get primary screen info
    QScreen* primaryScreen = qApp->primaryScreen();
    if (primaryScreen) {
        QString primaryName = primaryScreen->name();
        QString primaryInfo = QString("%1 (Primary - %2x%3)")
            .arg(primaryName)
            .arg(primaryScreen->geometry().width())
            .arg(primaryScreen->geometry().height());
        screens << primaryName;  // Add raw name for compatibility
        screens << primaryInfo;  // Add descriptive version
    }

    // Add other screens with resolution info
    for (auto screen : qApp->screens()) {
        if (screen != primaryScreen) {  // Skip primary screen as it's already added
            QString screenName = screen->name();
            QString screenInfo = QString("%1 (%2x%3)")
                .arg(screenName)
                .arg(screen->geometry().width())
                .arg(screen->geometry().height());
            screens << screenName;  // Add raw name for compatibility
            screens << screenInfo;  // Add descriptive version
        }
    }

    // Add common fallback names that wallpaper-engine might expect
    if (!screens.contains("HDMI-A-1")) screens << "HDMI-A-1";
    if (!screens.contains("HDMI-1")) screens << "HDMI-1";
    if (!screens.contains("DP-1")) screens << "DP-1";
    if (!screens.contains("eDP-1")) screens << "eDP-1";

    qCDebug(propertiesPanel) << "Available screens:" << screens;
    return screens;
}

void PropertiesPanel::updateSettingsControls()
{
    // Block signals to prevent triggering onSettingChanged
    m_useGlobalDefaultsCheckBox->blockSignals(true);
    m_silentCheckBox->blockSignals(true);
    m_volumeSlider->blockSignals(true);
    m_noAutoMuteCheckBox->blockSignals(true);
    m_noAudioProcessingCheckBox->blockSignals(true);
    m_fpsSpinBox->blockSignals(true);
    m_windowGeometryEdit->blockSignals(true);
    m_screenRootCombo->blockSignals(true);
    m_customScreenRootEdit->blockSignals(true);
    m_backgroundIdEdit->blockSignals(true);
    m_scalingCombo->blockSignals(true);
    m_clampingCombo->blockSignals(true);
    m_disableMouseCheckBox->blockSignals(true);
    m_disableParallaxCheckBox->blockSignals(true);
    m_noFullscreenPauseCheckBox->blockSignals(true);
    
    // Block WNEL-specific signals
    m_noLoopCheckBox->blockSignals(true);
    m_noHardwareDecodeCheckBox->blockSignals(true);
    m_forceX11CheckBox->blockSignals(true);
    m_forceWaylandCheckBox->blockSignals(true);
    m_verboseCheckBox->blockSignals(true);
    m_logLevelCombo->blockSignals(true);
    m_mpvOptionsEdit->blockSignals(true);
    
    // Update the useGlobalDefaults checkbox
    m_useGlobalDefaultsCheckBox->setChecked(m_currentSettings.useGlobalDefaults);
    
    // Enable/disable all controls based on useGlobalDefaults flag
    bool enableControls = !m_currentSettings.useGlobalDefaults;
    m_silentCheckBox->setEnabled(enableControls);
    m_volumeSlider->setEnabled(enableControls);
    m_noAutoMuteCheckBox->setEnabled(enableControls);
    m_noAudioProcessingCheckBox->setEnabled(enableControls);
    m_fpsSpinBox->setEnabled(enableControls);
    m_windowGeometryEdit->setEnabled(enableControls);
    m_customScreenRootEdit->setEnabled(enableControls);
    m_backgroundIdEdit->setEnabled(enableControls);
    m_scalingCombo->setEnabled(enableControls);
    m_clampingCombo->setEnabled(enableControls);
    m_disableMouseCheckBox->setEnabled(enableControls);
    m_disableParallaxCheckBox->setEnabled(enableControls);
    m_noFullscreenPauseCheckBox->setEnabled(enableControls);
    m_noLoopCheckBox->setEnabled(enableControls);
    m_noHardwareDecodeCheckBox->setEnabled(enableControls);
    m_forceX11CheckBox->setEnabled(enableControls);
    m_forceWaylandCheckBox->setEnabled(enableControls);
    m_verboseCheckBox->setEnabled(enableControls);
    m_logLevelCombo->setEnabled(enableControls);
    m_mpvOptionsEdit->setEnabled(enableControls);
    
    // Update controls with current settings
    m_silentCheckBox->setChecked(m_currentSettings.silent);
    m_volumeSlider->setValue(m_currentSettings.volume);
    m_volumeSpinBox->setValue(m_currentSettings.volume);
    m_noAutoMuteCheckBox->setChecked(m_currentSettings.noAutoMute);
    m_noAudioProcessingCheckBox->setChecked(m_currentSettings.noAudioProcessing);
    m_fpsSpinBox->setValue(m_currentSettings.fps);
    m_fpsSpinBox->setValue(m_currentSettings.fps);
    m_windowGeometryEdit->setText(m_currentSettings.windowGeometry);
    // Handle screen root settings in the correct order
    m_customScreenRootEdit->setText(m_currentSettings.customScreenRoot);
    // Find and select the correct screen option
    QString savedScreen = m_currentSettings.screenRoot;
    if (savedScreen.isEmpty()) {
        m_screenRootCombo->setCurrentText("Default");
    } else {
        // Try to find the screen with resolution info first
        int index = m_screenRootCombo->findText(savedScreen, Qt::MatchStartsWith);
        if (index >= 0) {
            m_screenRootCombo->setCurrentIndex(index);
        } else {
            // Fall back to exact match if not found
            m_screenRootCombo->setCurrentText(savedScreen);
        }
    }
    m_screenRootCombo->setEnabled(m_currentSettings.customScreenRoot.isEmpty() && enableControls);  // Must be after setting custom screen root
    m_backgroundIdEdit->setText(m_currentSettings.backgroundId);
    m_scalingCombo->setCurrentText(m_currentSettings.scaling);
    m_clampingCombo->setCurrentText(m_currentSettings.clamping);
    m_disableMouseCheckBox->setChecked(m_currentSettings.disableMouse);
    m_disableParallaxCheckBox->setChecked(m_currentSettings.disableParallax);
    m_noFullscreenPauseCheckBox->setChecked(m_currentSettings.noFullscreenPause);
    
    // Update WNEL-specific controls
    m_noLoopCheckBox->setChecked(m_currentSettings.noLoop);
    m_noHardwareDecodeCheckBox->setChecked(m_currentSettings.noHardwareDecode);
    m_forceX11CheckBox->setChecked(m_currentSettings.forceX11);
    m_forceWaylandCheckBox->setChecked(m_currentSettings.forceWayland);
    m_verboseCheckBox->setChecked(m_currentSettings.verbose);
    m_logLevelCombo->setCurrentText(m_currentSettings.logLevel);
    m_mpvOptionsEdit->setText(m_currentSettings.mpvOptions);
    
    // Re-enable signals
    m_useGlobalDefaultsCheckBox->blockSignals(false);
    m_silentCheckBox->blockSignals(false);
    m_volumeSlider->blockSignals(false);
    m_noAutoMuteCheckBox->blockSignals(false);
    m_noAudioProcessingCheckBox->blockSignals(false);
    m_fpsSpinBox->blockSignals(false);
    m_windowGeometryEdit->blockSignals(false);
    m_screenRootCombo->blockSignals(false);
    m_customScreenRootEdit->blockSignals(false);
    m_backgroundIdEdit->blockSignals(false);
    m_scalingCombo->blockSignals(false);
    m_clampingCombo->blockSignals(false);
    m_disableMouseCheckBox->blockSignals(false);
    m_disableParallaxCheckBox->blockSignals(false);
    m_noFullscreenPauseCheckBox->blockSignals(false);
    
    // Re-enable WNEL-specific signals
    m_noLoopCheckBox->blockSignals(false);
    m_noHardwareDecodeCheckBox->blockSignals(false);
    m_forceX11CheckBox->blockSignals(false);
    m_forceWaylandCheckBox->blockSignals(false);
    m_verboseCheckBox->blockSignals(false);
    m_logLevelCombo->blockSignals(false);
    m_mpvOptionsEdit->blockSignals(false);
    
    m_settingsModified = false;
    m_saveSettingsButton->setEnabled(false);
}

void PropertiesPanel::clear()
{
    qCDebug(propertiesPanel) << "Clearing properties panel";
    
    m_currentWallpaper = WallpaperInfo();
    
    // Reset settings to global defaults instead of hardcoded defaults
    ConfigManager& config = ConfigManager::instance();
    m_currentSettings.silent = config.globalSilent();
    m_currentSettings.volume = config.globalVolume();
    m_currentSettings.noAutoMute = config.globalNoAutoMute();
    m_currentSettings.noAudioProcessing = config.globalNoAudioProcessing();
    m_currentSettings.fps = config.globalFps();
    m_currentSettings.windowGeometry = config.globalWindowGeometry();
    m_currentSettings.screenRoot = config.globalScreenRoot();
    m_currentSettings.customScreenRoot = "";
    m_currentSettings.backgroundId = config.globalBackgroundId();
    m_currentSettings.scaling = config.globalScaling();
    m_currentSettings.clamping = config.globalClamping();
    m_currentSettings.disableMouse = config.globalDisableMouse();
    m_currentSettings.disableParallax = config.globalDisableParallax();
    m_currentSettings.noFullscreenPause = config.globalNoFullscreenPause();
    m_currentSettings.noLoop = config.globalNoLoop();
    m_currentSettings.noHardwareDecode = config.globalNoHardwareDecode();
    m_currentSettings.forceX11 = config.globalForceX11();
    m_currentSettings.forceWayland = config.globalForceWayland();
    m_currentSettings.verbose = config.globalVerbose();
    m_currentSettings.logLevel = config.globalLogLevel();
    m_currentSettings.mpvOptions = config.globalMpvOptions();
    
    m_propertyWidgets.clear();
    m_originalValues.clear();
    m_originalPropertyObjects.clear();
    m_propertiesModified = false;
    m_settingsModified = false;
    m_isWallpaperRunning = false;
    
    m_nameLabel->setText("No wallpaper selected");
    m_nameLabel->setToolTip("No wallpaper selected");
    m_authorLabel->setText("-");
    m_authorLabel->setToolTip("-");
    m_typeLabel->setText("-");
    m_typeLabel->setToolTip("-");
    m_fileSizeLabel->setText("-");
    m_fileSizeLabel->setToolTip("-");
    m_postedLabel->setText("-");
    m_postedLabel->setToolTip("-");
    m_updatedLabel->setText("-");
    m_updatedLabel->setToolTip("-");
    m_viewsLabel->setText("-");
    m_viewsLabel->setToolTip("-");
    m_subscriptionsLabel->setText("-");
    m_subscriptionsLabel->setToolTip("-");
    m_favoritesLabel->setText("-");
    m_favoritesLabel->setToolTip("-");
    m_descriptionEdit->setPlainText("Select a wallpaper to view its properties");
    
    setPlaceholderPreview("No wallpaper selected");
    
    // Clear properties safely
    if (m_propertiesWidget->layout()) {
        QLayout* oldLayout = m_propertiesWidget->layout();
        
        while (QLayoutItem* item = oldLayout->takeAt(0)) {
            if (QWidget* widget = item->widget()) {
                widget->setParent(nullptr);
                widget->deleteLater();
            }
            delete item;
        }
        
        delete oldLayout;
    }
    
    auto* layout = new QVBoxLayout(m_propertiesWidget);
    layout->setContentsMargins(8, 8, 8, 8);
    auto* label = new QLabel("No properties to display");
    label->setStyleSheet("color: #7f8c8d; font-style: italic;");
    layout->addWidget(label);
    
    // Reset settings controls to defaults
    updateSettingsControls();
    
    m_launchButton->setEnabled(false);
    m_savePropertiesButton->setEnabled(false);
    m_resetPropertiesButton->setEnabled(false);
    m_saveSettingsButton->setEnabled(false);
}

QString PropertiesPanel::formatFileSize(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    
    if (bytes >= GB) {
        return QString::number(bytes / (double)GB, 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / (double)MB, 'f', 1) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / (double)KB, 'f', 0) + " KB";
    } else {
        return QString::number(bytes) + " bytes";
    }
}

bool PropertiesPanel::loadCachedProperties(const QString& wallpaperId)
{
    QString cachePath = getCacheFilePath(wallpaperId);
    QFile file(cachePath);
    
    if (!file.exists()) {
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(propertiesPanel) << "Failed to open cache file:" << cachePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qCWarning(propertiesPanel) << "Failed to parse cache JSON:" << error.errorString();
        return false;
    }
    
    // Update properties with cached values
    updateProperties(doc.object());
    return true;
}

bool PropertiesPanel::saveCachedProperties(const QString& wallpaperId, const QJsonObject& properties)
{
    QString cachePath = getCacheFilePath(wallpaperId);
    
    // Create directory if it doesn't exist
    QFileInfo fileInfo(cachePath);
    QDir().mkpath(fileInfo.path());
    
    QFile file(cachePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(propertiesPanel) << "Failed to open cache file for writing:" << cachePath;
        return false;
    }
    
    QJsonDocument doc(properties);
    file.write(doc.toJson());
    return true;
}

void PropertiesPanel::updateSteamApiMetadata(const WallpaperInfo& wallpaper)
{
    // Skip Steam API calls for external wallpapers
    if (wallpaper.type == "External") {
        qCDebug(propertiesPanel) << "Skipping Steam API metadata for external wallpaper:" << wallpaper.id;
        return;
    }
    
    qCDebug(propertiesPanel) << "Fetching Steam API metadata for wallpaper ID:" << wallpaper.id;
    
    // Connect to the SteamApiManager temporarily for this request
    connect(&SteamApiManager::instance(), &SteamApiManager::itemDetailsReceived,
            this, &PropertiesPanel::onApiMetadataReceived, Qt::UniqueConnection);
    
    // Check if we have already cached data
    if (SteamApiManager::instance().hasCachedInfo(wallpaper.id)) {
        WorkshopItemInfo info = SteamApiManager::instance().getCachedItemInfo(wallpaper.id);
        onApiMetadataReceived(wallpaper.id, info);
        
        // If we have a creator ID but no creator name, fetch user profile
        if (!info.creator.isEmpty() && (info.creatorName.isEmpty() || info.creatorName == info.creator)) {
            SteamApiManager::instance().fetchUserProfile(info.creator);
        }
    } else {
        // No cached data, fetch from API
        SteamApiManager::instance().fetchItemDetails(wallpaper.id);
    }
}

QJsonObject PropertiesPanel::saveCurrentProperties()
{
    QJsonObject result;
    QJsonObject generalProps;
    QJsonObject properties;
    
    // Iterate through all property widgets and collect their current values
    for (auto it = m_propertyWidgets.begin(); it != m_propertyWidgets.end(); ++it) {
        QString propName = it.key();
        QWidget* widget = it.value();
        
        if (!widget) continue;
        
        QString type = widget->property("propertyType").toString();
        QJsonValue newValue;
        
        if (type == "bool") {
            QCheckBox* checkBox = qobject_cast<QCheckBox*>(widget);
            if (checkBox) {
                newValue = checkBox->isChecked();
            }
        }
        else if (type == "slider") {
            QSlider* slider = widget->findChild<QSlider*>();
            if (slider) {
                double minVal = slider->property("minValue").toDouble();
                double step = slider->property("step").toDouble();
                newValue = minVal + (slider->value() * step);
            }
        }
        else if (type == "float") {
            QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(widget);
            if (spinBox) {
                newValue = spinBox->value();
            }
        }
        else if (type == "int") {
            QSpinBox* spinBox = qobject_cast<QSpinBox*>(widget);
            if (spinBox) {
                newValue = spinBox->value();
            }
        }
        else if (type == "combo") {
            QComboBox* comboBox = qobject_cast<QComboBox*>(widget);
            if (comboBox) {
                // If the combo box has userData, use that as the value
                if (comboBox->currentData().isValid()) {
                    newValue = comboBox->currentData().toString();
                } else {
                    newValue = comboBox->currentText();
                }
            }
        }
        else if (type == "textinput" || type == "color") {
            QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget);
            if (lineEdit) {
                newValue = lineEdit->text();
            }
        }
        
        // If we got a value, create the property object preserving original metadata
        if (!newValue.isUndefined()) {
            QJsonObject propObj;
            
            // Start with the original property object if available to preserve all metadata
            if (m_originalPropertyObjects.contains(propName)) {
                propObj = m_originalPropertyObjects[propName];
            } else {
                // Fallback to minimal object if no original found
                propObj["type"] = type;
            }
            
            // Update only the value, keeping all other metadata (options, min, max, text, order, etc.)
            propObj["value"] = newValue;
            
            properties[propName] = propObj;
        }
    }
    
    // Build the JSON structure similar to project.json
    if (!properties.isEmpty()) {
        generalProps["properties"] = properties;
        result["general"] = generalProps;
    }
    
    return result;
}

QJsonObject PropertiesPanel::loadPropertiesFromProjectJson(const QString& wallpaperId)
{
    QString projectPath = getProjectJsonPath(wallpaperId);
    if (projectPath.isEmpty()) {
        qCWarning(propertiesPanel) << "Cannot find project.json for wallpaper:" << wallpaperId;
        return QJsonObject();
    }
    
    // Read the project.json file
    QFile file(projectPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(propertiesPanel) << "Failed to open project.json for reading:" << projectPath;
        return QJsonObject();
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qCWarning(propertiesPanel) << "Failed to parse project.json:" << error.errorString();
        return QJsonObject();
    }
    
    QJsonObject projectJson = doc.object();
    
    // Extract properties using the same logic as WallpaperManager::extractProperties
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
    
    // Also check for properties directly in root (fallback)
    if (projectJson.contains("properties")) {
        QJsonObject rootProps = projectJson.value("properties").toObject();
        
        // Merge root properties
        for (auto it = rootProps.begin(); it != rootProps.end(); ++it) {
            properties[it.key()] = it.value();
        }
    }
    
    qCDebug(propertiesPanel) << "Loaded" << properties.size() << "properties from project.json for wallpaper:" << wallpaperId;
    return properties;
}

void PropertiesPanel::onApiMetadataReceived(const QString& itemId, const WorkshopItemInfo& info)
{
    qCDebug(propertiesPanel) << "Received Steam API metadata for wallpaper ID:" << itemId;
    
    // Only update if this is for our current wallpaper
    if (m_currentWallpaper.id != itemId) {
        qCDebug(propertiesPanel) << "Ignoring metadata for different wallpaper";
        return;
    }
    
    // Update fields with data from API and set tooltips
    if (!info.title.isEmpty() && info.title != "Unknown") {
        m_nameLabel->setText(info.title);
        m_nameLabel->setToolTip(info.title);
        m_currentWallpaper.name = info.title;
    }
    
    if (!info.creatorName.isEmpty()) {
        m_authorLabel->setText(info.creatorName);
        m_authorLabel->setToolTip(info.creatorName);
        m_currentWallpaper.author = info.creatorName;
        m_currentWallpaper.authorId = info.creator;
    } else if (!info.creator.isEmpty()) {
        m_authorLabel->setText(info.creator);
        m_authorLabel->setToolTip(info.creator);
        m_currentWallpaper.author = info.creator;
        m_currentWallpaper.authorId = info.creator;
    }
    
    if (!info.description.isEmpty()) {
        m_descriptionEdit->setPlainText(info.description);
        m_descriptionEdit->setEnabled(true);
        m_descriptionEdit->setStyleSheet("QTextEdit { color: #333; }");
        m_currentWallpaper.description = info.description;
    }
    
    if (!info.type.isEmpty()) {
        m_typeLabel->setText(info.type);
        m_currentWallpaper.type = info.type;
    }
    
    if (info.fileSize > 0) {
        m_fileSizeLabel->setText(formatFileSize(info.fileSize));
        m_currentWallpaper.fileSize = info.fileSize;
    }
    
    // Steam workshop statistics
    if (info.created.isValid()) {
        QString dateStr = info.created.toString("MMM d, yyyy");
        m_postedLabel->setText(dateStr);
        m_postedLabel->setToolTip(dateStr);
        m_currentWallpaper.created = info.created;
    }
    
    if (info.updated.isValid()) {
        QString dateStr = info.updated.toString("MMM d, yyyy");
        m_updatedLabel->setText(dateStr);
        m_updatedLabel->setToolTip(dateStr);
        m_currentWallpaper.updated = info.updated;
    }
    
    QString views = QString::number(info.views);
    m_viewsLabel->setText(views);
    m_viewsLabel->setToolTip(views);
    
    QString subs = QString::number(info.subscriptions);
    m_subscriptionsLabel->setText(subs);
    m_subscriptionsLabel->setToolTip(subs);
    
    QString favs = QString::number(info.favorites);
    m_favoritesLabel->setText(favs);
    m_favoritesLabel->setToolTip(favs);
    
    // Update tags
    if (!info.tags.isEmpty()) {
        m_currentWallpaper.tags = info.tags;
    }
    
    // Update UI to reflect changes
    update();
}

void PropertiesPanel::onUserProfileReceived(const QString& steamId, const SteamUserProfile& profile)
{
    qCDebug(propertiesPanel) << "Received user profile for Steam ID:" << steamId 
                            << "Name:" << profile.personaName;
    
    // Only update if this is for our current wallpaper's creator
    if (m_currentWallpaper.authorId == steamId) {
        // Update the author label and wallpaper info with tooltip
        m_authorLabel->setText(profile.personaName);
        m_authorLabel->setToolTip(profile.personaName);
        m_currentWallpaper.author = profile.personaName;
        
        // Update UI to reflect changes
        update();
        
        // Also update the cached WorkshopItemInfo if available
        if (SteamApiManager::instance().hasCachedInfo(m_currentWallpaper.id)) {
            WorkshopItemInfo info = SteamApiManager::instance().getCachedItemInfo(m_currentWallpaper.id);
            info.creatorName = profile.personaName;
            SteamApiManager::instance().saveToCache(info);
        }
    }
}

QString PropertiesPanel::getCacheFilePath(const QString& wallpaperId) const

{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheDir.isEmpty()) {
        cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    }
    
    if (cacheDir.isEmpty()) {
        cacheDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + 
                  "/.cache/wallpaperengine-gui";
    } else {
        cacheDir += "/wallpaperengine-gui";
    }
    
    return QString("%1/properties/%2.json").arg(cacheDir, wallpaperId);
}

bool PropertiesPanel::savePropertiesToProjectJson(const QString& wallpaperId, const QJsonObject& properties)
{
    QString projectPath = getProjectJsonPath(wallpaperId);
    if (projectPath.isEmpty()) {
        qCWarning(propertiesPanel) << "Cannot find project.json for wallpaper:" << wallpaperId;
        return false;
    }
    
    QString backupPath = getBackupProjectJsonPath(wallpaperId);
    
    // Create backup if it doesn't exist
    if (!QFileInfo::exists(backupPath)) {
        if (!QFile::copy(projectPath, backupPath)) {
            qCWarning(propertiesPanel) << "Failed to create backup of project.json:" << backupPath;
            return false;
        }
        qCDebug(propertiesPanel) << "Created backup of project.json:" << backupPath;
    }
    
    // Read the current project.json
    QFile file(projectPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(propertiesPanel) << "Failed to open project.json for reading:" << projectPath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qCWarning(propertiesPanel) << "Failed to parse project.json:" << error.errorString();
        return false;
    }
    
    QJsonObject projectJson = doc.object();
    
    // Update properties in the project.json structure
    if (properties.contains("general")) {
        QJsonObject generalProps = properties.value("general").toObject();
        if (generalProps.contains("properties")) {
            QJsonObject newProperties = generalProps.value("properties").toObject();
            
            // Get or create the general section
            QJsonObject general = projectJson.value("general").toObject();
            QJsonObject existingProperties = general.value("properties").toObject();
            
            // Merge the new properties with existing ones
            for (auto it = newProperties.begin(); it != newProperties.end(); ++it) {
                existingProperties[it.key()] = it.value();
            }
            
            general["properties"] = existingProperties;
            projectJson["general"] = general;
        }
    }
    
    // Write the modified project.json
    QFile writeFile(projectPath);
    if (!writeFile.open(QIODevice::WriteOnly)) {
        qCWarning(propertiesPanel) << "Failed to open project.json for writing:" << projectPath;
        return false;
    }
    
    QJsonDocument newDoc(projectJson);
    writeFile.write(newDoc.toJson());
    writeFile.close();
    
    qCDebug(propertiesPanel) << "Successfully saved properties to project.json:" << projectPath;
    return true;
}

bool PropertiesPanel::resetPropertiesFromBackup(const QString& wallpaperId)
{
    QString projectPath = getProjectJsonPath(wallpaperId);
    QString backupPath = getBackupProjectJsonPath(wallpaperId);
    
    if (projectPath.isEmpty()) {
        qCWarning(propertiesPanel) << "Cannot find project.json for wallpaper:" << wallpaperId;
        return false;
    }
    
    if (!QFileInfo::exists(backupPath)) {
        QMessageBox::information(this, "Reset Changes", 
            "No backup file found. Either no changes have been made, or the backup file is missing.");
        return false;
    }
    
    // Remove the modified project.json
    if (QFileInfo::exists(projectPath)) {
        if (!QFile::remove(projectPath)) {
            qCWarning(propertiesPanel) << "Failed to remove modified project.json:" << projectPath;
            return false;
        }
    }
    
    // Rename backup back to project.json
    if (!QFile::rename(backupPath, projectPath)) {
        qCWarning(propertiesPanel) << "Failed to restore backup project.json:" << backupPath;
        return false;
    }
    
    qCDebug(propertiesPanel) << "Successfully reset properties from backup:" << projectPath;
    return true;
}

QString PropertiesPanel::getProjectJsonPath(const QString& wallpaperId) const
{
    if (m_currentWallpaper.id == wallpaperId && !m_currentWallpaper.projectPath.isEmpty()) {
        return m_currentWallpaper.projectPath;
    }
    
    // If we don't have the current wallpaper info, we can't find the project path
    return QString();
}

QString PropertiesPanel::getBackupProjectJsonPath(const QString& wallpaperId) const
{
    QString projectPath = getProjectJsonPath(wallpaperId);
    if (projectPath.isEmpty()) {
        return QString();
    }
    
    return projectPath + ".backup";
}

void PropertiesPanel::loadAnimatedPreview(const QString& previewPath)
{
    // Clean up existing movie if any
    stopPreviewAnimation();
    
    // Create and configure QMovie for animation
    m_previewMovie = new QMovie(previewPath, QByteArray(), this);
    
    if (!m_previewMovie->isValid()) {
        qCWarning(propertiesPanel) << "Invalid animated preview file:" << previewPath;
        m_previewMovie->deleteLater();
        m_previewMovie = nullptr;
        setPlaceholderPreview("Invalid animated preview");
        return;
    }
    
    // Connect to movie signals for frame updates
    connect(m_previewMovie, &QMovie::frameChanged, this, [this](int frameNumber) {
        Q_UNUSED(frameNumber)
        if (m_previewMovie) {
            QPixmap currentFrame = m_previewMovie->currentPixmap();
            if (!currentFrame.isNull()) {
                // Scale the frame to fit the preview label
                QSize labelSize = m_previewLabel->size();
                if (labelSize.width() < 50 || labelSize.height() < 50) {
                    labelSize = QSize(256, 144); // Use default size if label isn't sized yet
                }
                
                QPixmap scaledFrame = scalePixmapKeepAspectRatio(currentFrame, labelSize);
                m_previewLabel->setPixmap(scaledFrame);
                       }
        }
    });
    
    connect(m_previewMovie, &QMovie::error, this, [this, previewPath](QImageReader::ImageReaderError error) {
        qCWarning(propertiesPanel) << "Movie error:" << error << "for file:" << previewPath;
        setPlaceholderPreview("Animation error");
    });
    
    // Set the movie to the label and start playing
    m_previewLabel->setMovie(m_previewMovie);
    startPreviewAnimation();
    
    qCDebug(propertiesPanel) << "Loaded animated preview for:" << previewPath;
}

bool PropertiesPanel::hasAnimatedPreview(const QString& previewPath) const
{
    if (previewPath.isEmpty() || !QFileInfo::exists(previewPath)) {
        return false;
    }
    
    // Check if the preview is an animated format
    QString lowerPath = previewPath.toLower();
    return lowerPath.endsWith(".gif") || lowerPath.endsWith(".webp");
}

void PropertiesPanel::stopPreviewAnimation()
{
    if (m_previewMovie) {
        m_previewMovie->stop();
        m_previewMovie->deleteLater();
        m_previewMovie = nullptr;
    }
    
    // Clear any movie from the label
    m_previewLabel->setMovie(nullptr);
}

void PropertiesPanel::startPreviewAnimation()
{
    if (m_previewMovie && m_previewMovie->isValid()) {
        qCDebug(propertiesPanel) << "Starting preview animation";
        m_previewMovie->start();
    }
}

// Event filter to intercept tab clicks before they change tabs
bool PropertiesPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_innerTabWidget->tabBar()) {
        if (event->type() == QEvent::MouseButtonPress) {
            // User is starting to interact with tabs
            m_userInteractingWithTabs = true;
            
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                // Get the tab index at the click position
                QTabBar *tabBar = m_innerTabWidget->tabBar();
                int clickedIndex = tabBar->tabAt(mouseEvent->pos());
                
                if (clickedIndex >= 0) {
                    return handleTabClickWithUnsavedCheck(clickedIndex);
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            // Use a timer to clear the interaction flag after a short delay
            // This prevents external tab switches during the tab change process
            QTimer::singleShot(1000, this, [this]() {
                m_userInteractingWithTabs = false;
            });
        }
    }
    
    return QWidget::eventFilter(watched, event);
}

// Handle tab click with unsaved changes check
bool PropertiesPanel::handleTabClickWithUnsavedCheck(int index)
{
    if (m_ignoreTabChange) {
        // Allow the tab change to proceed without checking
        return false; // Don't consume the event
    }
    
    int currentIndex = m_innerTabWidget->currentIndex();
    
    // If clicking the same tab, no need to check
    if (currentIndex == index) {
        return false; // Don't consume the event
    }
    
    // Check if we're leaving a tab with unsaved changes
    bool hasUnsaved = false;
    
    // Skip unsaved changes check for external wallpapers to avoid tab switching issues
    bool isExternalWallpaper = (m_currentWallpaper.type == "External");
    
    if (!isExternalWallpaper) {
        if (currentIndex == 1) { // Wallpaper Settings tab
            hasUnsaved = m_propertiesModified;
        } else if (currentIndex == 2) { // Engine Settings tab
            hasUnsaved = m_settingsModified;
        }
    }
    
    if (hasUnsaved) {
        // Show the confirmation dialog
        if (showUnsavedChangesDialog()) {
            // User chose to discard changes
            resetUnsavedChanges();
            
            // Allow the tab change by programmatically setting it
            m_ignoreTabChange = true;
            m_innerTabWidget->setCurrentIndex(index);
            m_ignoreTabChange = false;
        }
        // If user chose to stay, consume the event to prevent the tab change
        return true; // Consume the event to prevent default behavior
    } else {
        // No unsaved changes, allow the click to proceed normally
        return false; // Don't consume the event
    }
}

// Legacy method - no longer used but kept for compatibility
void PropertiesPanel::onTabBarClicked(int index)
{
    // This method is no longer used since we're using event filtering
    Q_UNUSED(index)
}

bool PropertiesPanel::hasUnsavedChanges() const
{
    return m_propertiesModified || m_settingsModified;
}

bool PropertiesPanel::showUnsavedChangesDialog()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Unsaved Changes");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText("You have unsaved changes that will be lost.");
    msgBox.setInformativeText("Do you want to discard your changes and continue?");
    
    QPushButton* discardButton = msgBox.addButton("Discard Changes", QMessageBox::DestructiveRole);
    QPushButton* stayButton = msgBox.addButton("Stay Here", QMessageBox::RejectRole);
    msgBox.setDefaultButton(stayButton);
    
    msgBox.exec();
    
    return msgBox.clickedButton() == discardButton;
}

void PropertiesPanel::resetUnsavedChanges()
{
    if (m_propertiesModified) {
        // Reset properties to original values
        onResetPropertiesClicked();
    }
    
    if (m_settingsModified) {
        // Reset settings to last saved values
        loadWallpaperSettings(m_currentWallpaper.id);
        m_settingsModified = false;
        m_saveSettingsButton->setEnabled(false);
    }
}

bool PropertiesPanel::checkUnsavedChangesBeforeAction()
{
    if (hasUnsavedChanges()) {
        return showUnsavedChangesDialog();
    }
    return true; // No unsaved changes, proceed
}

// Slot implementation for copying wallpaper ID
void PropertiesPanel::copyWallpaperIdToClipboard()
{
    if (m_currentWallpaper.id.isEmpty()) {
        return;
    }
    
    QClipboard* clipboard = QApplication::clipboard();
    QString textToCopy;
    QString messageTitle;
    QString messageText;
    
    if (m_currentWallpaper.type == "External") {
        // For external wallpapers, copy the file path
        textToCopy = getExternalWallpaperFilePath(m_currentWallpaper.id);
        if (textToCopy.isEmpty()) {
            textToCopy = m_currentWallpaper.id; // Fallback to ID
            messageTitle = "Copy File Path";
            messageText = "Could not find file path, wallpaper ID copied instead.";
        } else {
            messageTitle = "Copy File Path";
            messageText = "File path copied to clipboard.";
        }
    } else {
        // For regular wallpapers, copy the ID
        textToCopy = m_currentWallpaper.id;
        messageTitle = "Copy Wallpaper ID";
        messageText = "Wallpaper ID copied to clipboard.";
    }
    
    clipboard->setText(textToCopy);
    QMessageBox::information(this, messageTitle, messageText);
}

void PropertiesPanel::setupExternalWallpaperUI()
{
    qCDebug(propertiesPanel) << "setupExternalWallpaperUI called - using simplified version";
    
    // Safety check: ensure m_propertiesWidget exists
    if (!m_propertiesWidget) {
        qCWarning(propertiesPanel) << "setupExternalWallpaperUI: m_propertiesWidget is null";
        return;
    }
    
    // Use a very simple layout to avoid widget lifecycle issues
    auto* layout = new QVBoxLayout(m_propertiesWidget);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(16);
    
    // Simple external wallpaper information section
    auto* infoGroup = new QGroupBox("External Wallpaper");
    auto* infoLayout = new QVBoxLayout(infoGroup);
    infoLayout->setContentsMargins(12, 16, 12, 12);
    infoLayout->setSpacing(12);
    
    QString infoText = QString(
        "External wallpaper: %1\n\n"
        "This is a custom media file launched using WNEL (wallpaper_not-engine_linux).\n"
        "You can configure engine settings in the Engine Settings tab.\n\n"
        "File path: %2"
    ).arg(m_currentWallpaper.name).arg(m_currentWallpaper.path.isEmpty() ? m_currentWallpaper.id : m_currentWallpaper.path);
    
    auto* infoLabel = new QLabel(infoText);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("background: #f0f0f0; padding: 12px; border: 1px solid #ccc; border-radius: 4px;");
    
    infoLayout->addWidget(infoLabel);
    layout->addWidget(infoGroup);
    layout->addStretch();
    
    qCDebug(propertiesPanel) << "External wallpaper UI setup completed successfully";
}

void PropertiesPanel::updateWNELSettingsVisibility(bool isExternalWallpaper)
{
    // Find the WNEL group widget in the engine settings tab
    QWidget* engineTab = engineSettingsTab();
    if (!engineTab) return;
    
    QGroupBox* wnelGroup = engineTab->findChild<QGroupBox*>("wnelGroup");
    if (wnelGroup) {
        wnelGroup->setVisible(isExternalWallpaper);
    }
    
    // Also hide certain regular wallpaper settings when external wallpaper is selected
    // These settings don't apply to WNEL
    if (isExternalWallpaper) {
        // Hide settings that don't apply to WNEL
        m_disableMouseCheckBox->setVisible(false);
        m_disableParallaxCheckBox->setVisible(false);
        m_noFullscreenPauseCheckBox->setVisible(false);
        m_windowGeometryEdit->setVisible(false);
        m_backgroundIdEdit->setVisible(false);
        m_clampingCombo->setVisible(false);
        
        // Find and hide their labels too
        QWidget* parent = m_disableMouseCheckBox->parentWidget();
        if (parent) {
            QFormLayout* layout = qobject_cast<QFormLayout*>(parent->layout());
            if (layout) {
                // Hide the entire behavior group for external wallpapers
                QGroupBox* behaviorGroup = parent->parentWidget() ? 
                    qobject_cast<QGroupBox*>(parent->parentWidget()) : nullptr;
                if (behaviorGroup && behaviorGroup->title() == "Behavior Settings") {
                    behaviorGroup->setVisible(false);
                }
            }
        }
    } else {
        // Show regular wallpaper settings
        m_disableMouseCheckBox->setVisible(true);
        m_disableParallaxCheckBox->setVisible(true);
        m_noFullscreenPauseCheckBox->setVisible(true);
        m_windowGeometryEdit->setVisible(true);
        m_backgroundIdEdit->setVisible(true);
        m_clampingCombo->setVisible(true);
        
        // Show behavior group
        QWidget* parent = m_disableMouseCheckBox->parentWidget();
        if (parent) {
            QGroupBox* behaviorGroup = parent->parentWidget() ? 
                qobject_cast<QGroupBox*>(parent->parentWidget()) : nullptr;
            if (behaviorGroup && behaviorGroup->title() == "Behavior Settings") {
                behaviorGroup->setVisible(true);
            }
        }
    }
}

void PropertiesPanel::updateUIVisibilityForWallpaperType(bool isExternal)
{
    qCDebug(propertiesPanel) << "updateUIVisibilityForWallpaperType called with isExternal:" << isExternal;
    
    // Update ID section - show file path for external, ID for regular
    if (m_idSection) {
        // Change the label text for external wallpapers
        QLabel* idTitleLabel = m_idSection->findChild<QLabel*>();
        if (idTitleLabel && idTitleLabel->text() == "ID:") {
            idTitleLabel->setText(isExternal ? "File:" : "ID:");
        }
        
        // For external wallpapers, make the copy button copy the file path
        if (m_copyIdButton) {
            m_copyIdButton->setToolTip(isExternal ? "Copy file path" : "Copy wallpaper ID");
        }
    }
    
    // Show/hide Steam-specific metadata
    if (m_steamSection) {
        m_steamSection->setVisible(!isExternal);
        qCDebug(propertiesPanel) << "Steam section visibility set to:" << !isExternal;
    }
    
    // Hide unsupported engine settings for external wallpapers
    QWidget* engineTab = engineSettingsTab();
    if (engineTab) {
        qCDebug(propertiesPanel) << "Found engine tab, looking for widgets to hide";
        
        // Hide audio processing (not supported by WNEL)
        if (m_noAudioProcessingWidget) {
            m_noAudioProcessingWidget->setVisible(!isExternal);
            qCDebug(propertiesPanel) << "Audio processing widget visibility set to:" << !isExternal;
        } else {
            qCDebug(propertiesPanel) << "Audio processing widget not found";
        }
        
        // Hide window geometry (not supported by WNEL)
        if (m_windowGeometryWidget && m_windowGeometryLabel) {
            m_windowGeometryWidget->setVisible(!isExternal);
            m_windowGeometryLabel->setVisible(!isExternal);
            qCDebug(propertiesPanel) << "Window geometry widget and label visibility set to:" << !isExternal;
        } else {
            qCDebug(propertiesPanel) << "Window geometry widget or label not found";
        }
        
        // Hide background ID (not supported by WNEL)
        if (m_backgroundIdWidget && m_backgroundIdLabel) {
            m_backgroundIdWidget->setVisible(!isExternal);
            m_backgroundIdLabel->setVisible(!isExternal);
            qCDebug(propertiesPanel) << "Background ID widget and label visibility set to:" << !isExternal;
        } else {
            qCDebug(propertiesPanel) << "Background ID widget or label not found";
        }
        
        // Hide clamping (not supported by WNEL)
        if (m_clampingWidget && m_clampingLabel) {
            m_clampingWidget->setVisible(!isExternal);
            m_clampingLabel->setVisible(!isExternal);
            qCDebug(propertiesPanel) << "Clamping widget and label visibility set to:" << !isExternal;
        } else {
            qCDebug(propertiesPanel) << "Clamping widget or label not found";
        }
        
        // Hide behavior settings group (not applicable to WNEL)
        QList<QGroupBox*> groups = engineTab->findChildren<QGroupBox*>();
        for (QGroupBox* group : groups) {
            if (group && group->title() == "Behavior Settings") {
                group->setVisible(!isExternal);
                qCDebug(propertiesPanel) << "Behavior Settings group visibility set to:" << !isExternal;
                break;
            }
        }
    } else {
        qCDebug(propertiesPanel) << "Engine tab not found";
    }
}

QString PropertiesPanel::getExternalWallpaperFilePath(const QString& wallpaperId)
{
    // Validate input
    if (wallpaperId.isEmpty()) {
        qCWarning(propertiesPanel) << "getExternalWallpaperFilePath: Empty wallpaper ID";
        return QString();
    }
    
    // Read the project.json file for the external wallpaper to get the file path
    // Use the configured external wallpapers path instead of hardcoded config location
    ConfigManager& config = ConfigManager::instance();
    QString externalWallpapersDir = config.externalWallpapersPath();
    QString externalDir = externalWallpapersDir + "/" + wallpaperId;
    QString projectFile = externalDir + "/project.json";
    
    qCDebug(propertiesPanel) << "Looking for external wallpaper in:" << externalDir;
    
    // Check if the directory and project file exist before trying to read
    QDir dir(externalDir);
    if (!dir.exists()) {
        qCWarning(propertiesPanel) << "External wallpaper directory does not exist:" << externalDir;
        return QString();
    }
    
    QFile file(projectFile);
    if (!file.exists()) {
        qCWarning(propertiesPanel) << "External wallpaper project.json does not exist:" << projectFile;
        return QString();
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(propertiesPanel) << "Failed to open external wallpaper project.json:" << projectFile;
        return QString();
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qCWarning(propertiesPanel) << "Failed to parse external wallpaper project.json:" << error.errorString();
        return QString();
    }
    
    QJsonObject project = doc.object();
    QString filePath = project.value("originalPath").toString();
    
    // Fallback to relative file path if originalPath is not available
    if (filePath.isEmpty()) {
        QString relativeFile = project.value("file").toString();
        if (!relativeFile.isEmpty()) {
            filePath = externalDir + "/" + relativeFile;
        }
    }
    
    // Validate the file path exists
    if (!filePath.isEmpty() && !QFile::exists(filePath)) {
        qCWarning(propertiesPanel) << "External wallpaper original file does not exist:" << filePath;
        // Don't return empty string, return the path anyway for display purposes
    }
    
    // Return the original file path if available
    return filePath;
}
