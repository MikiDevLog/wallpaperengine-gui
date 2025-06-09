#include "PropertiesPanel.h"
#include "ConfigManager.h"
#include "SteamApiManager.h"
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

Q_LOGGING_CATEGORY(propertiesPanel, "app.propertiespanel")

// Implementation of WallpaperSettings::toCommandLineArgs()
QStringList WallpaperSettings::toCommandLineArgs() const
{
    QStringList args;
    
    // Audio settings
    if (silent) {
        args << "--silent";
    }
    
    if (volume != 15) { // Only add if different from default
        args << "--volume" << QString::number(volume);
    }
    
    if (noAutoMute) {
        args << "--noautomute";
    }
    
    if (noAudioProcessing) {
        args << "--no-audio-processing";
    }
    
    // Performance settings
    if (fps != 30) { // Only add if different from default
        args << "--fps" << QString::number(fps);
    }
    
    // Display settings
    if (!windowGeometry.isEmpty()) {
        args << "--window" << windowGeometry;
    }
    
    if (!screenRoot.isEmpty()) {
        args << "--screen-root" << screenRoot;
        
        if (!backgroundId.isEmpty()) {
            args << "--bg" << backgroundId;
        }
    }
    
    if (scaling != "default") {
        args << "--scaling" << scaling;
    }
    
    if (clamping != "clamp") {
        args << "--clamping" << clamping;
    }
    
    // Behavior settings
    if (disableMouse) {
        args << "--disable-mouse";
    }
    
    if (disableParallax) {
        args << "--disable-parallax";
    }
    
    if (noFullscreenPause) {
        args << "--no-fullscreen-pause";
    }
    
    return args;
}

PropertiesPanel::PropertiesPanel(QWidget* parent)
    : QWidget(parent)
    , m_nameLabel(new QLabel("No wallpaper selected"))
    , m_authorLabel(new QLabel)
    , m_typeLabel(new QLabel)
    , m_fileSizeLabel(new QLabel)
    , m_postedLabel(new QLabel)
    , m_updatedLabel(new QLabel)
    , m_viewsLabel(new QLabel)
    , m_subscriptionsLabel(new QLabel)
    , m_favoritesLabel(new QLabel)
    , m_previewLabel(new QLabel)
    , m_descriptionEdit(new QTextEdit)
    , m_launchButton(new QPushButton("Launch Wallpaper"))
    , m_savePropertiesButton(new QPushButton("Save Properties"))
    , m_propertiesWidget(nullptr)
    , m_scrollArea(new QScrollArea)
    , m_settingsWidget(nullptr)
    , m_saveSettingsButton(new QPushButton("Save Settings"))
    , m_silentCheckBox(new QCheckBox("Silent mode"))
    , m_volumeSlider(new QSlider(Qt::Horizontal))
    , m_volumeLabel(new QLabel("15%"))
    , m_noAutoMuteCheckBox(new QCheckBox("Don't mute when other apps play audio"))
    , m_noAudioProcessingCheckBox(new QCheckBox("Disable audio processing"))
    , m_fpsSpinBox(new QSpinBox)
    , m_windowGeometryEdit(new QLineEdit)
    , m_screenRootCombo(new QComboBox)
    , m_backgroundIdEdit(new QLineEdit)
    , m_scalingCombo(new QComboBox)
    , m_clampingCombo(new QComboBox)
    , m_disableMouseCheckBox(new QCheckBox("Disable mouse interaction"))
    , m_disableParallaxCheckBox(new QCheckBox("Disable parallax effects"))
    , m_noFullscreenPauseCheckBox(new QCheckBox("Don't pause when apps go fullscreen"))
    , m_currentWallpaper()
    , m_currentSettings()
    , m_previewMovie(nullptr)
    , m_propertyWidgets()
    , m_originalValues()
    , m_propertiesModified(false)
    , m_settingsModified(false)
    , m_isWallpaperRunning(false)
{
    setupUI();
    
    // Connect signals for property changes
    connect(m_savePropertiesButton, &QPushButton::clicked, this, &PropertiesPanel::onSavePropertiesClicked);
    connect(m_saveSettingsButton, &QPushButton::clicked, this, &PropertiesPanel::onSaveSettingsClicked);
    connect(m_launchButton, &QPushButton::clicked, this, [this]() {
        if (!m_currentWallpaper.id.isEmpty()) {
            emit launchWallpaper(m_currentWallpaper);
        }
    });
    
    // Connect settings change signals
    connect(m_silentCheckBox, &QCheckBox::toggled, this, &PropertiesPanel::onSettingChanged);
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_volumeLabel->setText(QString("%1%").arg(value));
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
       infoLayout->addRow(createFormLabel("Posted:"), m_postedLabel);
       infoLayout->addRow(createFormLabel("Updated:"), m_updatedLabel);
       infoLayout->addRow(createFormLabel("Views:"), m_viewsLabel);
       infoLayout->addRow(createFormLabel("Subscriptions:"), m_subscriptionsLabel);
       infoLayout->addRow(createFormLabel("Favorites:"), m_favoritesLabel);
       
       l->addWidget(infoSection);
       
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

       // Create placeholder widget for properties
       m_propertiesWidget = new QWidget;
       auto* propsFormLayout = new QVBoxLayout(m_propertiesWidget);
       propsFormLayout->setContentsMargins(8, 8, 8, 8);
       propsFormLayout->addWidget(new QLabel("Select a wallpaper to view properties"));

       m_scrollArea->setWidget(m_propertiesWidget);
       propsLayout->addWidget(m_scrollArea);
       // Properties buttons
       auto* propsButtonLayout = new QHBoxLayout;
       propsButtonLayout->setContentsMargins(0, 8, 0, 0);
       propsButtonLayout->addStretch();
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
    
    m_volumeLabel->setMinimumWidth(50);
    m_volumeLabel->setMinimumHeight(28);
    m_volumeLabel->setAlignment(Qt::AlignCenter);
    m_volumeLabel->setStyleSheet("border: 1px solid #c0c0c0; padding: 4px; background: white;");
    
    volumeLayout->addWidget(m_volumeSlider);
    volumeLayout->addWidget(m_volumeLabel);
    
    auto* volumeLabel = new QLabel("Volume:");
    volumeLabel->setMinimumWidth(80);
    volumeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    volumeLabel->setStyleSheet("font-weight: bold;");
    audioLayout->addRow(volumeLabel, volumeWidget);
    
    // Other audio checkboxes
    m_noAutoMuteCheckBox->setMinimumHeight(28);
    audioLayout->addRow("", m_noAutoMuteCheckBox);
    
    m_noAudioProcessingCheckBox->setMinimumHeight(28);
    audioLayout->addRow("", m_noAudioProcessingCheckBox);
    
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
    
    m_windowGeometryEdit->setPlaceholderText("e.g., 1920x1080+0+0");
    m_windowGeometryEdit->setMinimumWidth(200);
    m_windowGeometryEdit->setMinimumHeight(28);
    displayLayout->addRow(createDisplayLabel("Window Geometry:"), m_windowGeometryEdit);
    
    // Screen root with proper sizing
    m_screenRootCombo->setMinimumWidth(200);
    m_screenRootCombo->setMinimumHeight(28);
    QStringList screens = getAvailableScreens();
    m_screenRootCombo->addItems(screens);
    displayLayout->addRow(createDisplayLabel("Screen Root:"), m_screenRootCombo);
    
    m_backgroundIdEdit->setPlaceholderText("Background ID");
    m_backgroundIdEdit->setMinimumWidth(200);
    m_backgroundIdEdit->setMinimumHeight(28);
    displayLayout->addRow(createDisplayLabel("Background ID:"), m_backgroundIdEdit);
    
    // Scaling combo
    m_scalingCombo->addItems({"default", "stretch", "fit", "fill", "center"});
    m_scalingCombo->setMinimumWidth(150);
    m_scalingCombo->setMinimumHeight(28);
    displayLayout->addRow(createDisplayLabel("Scaling:"), m_scalingCombo);
    
    // Clamping combo
    m_clampingCombo->addItems({"clamp", "border", "repeat"});
    m_clampingCombo->setMinimumWidth(150);
    m_clampingCombo->setMinimumHeight(28);
    displayLayout->addRow(createDisplayLabel("Clamping:"), m_clampingCombo);
    
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
    
    m_currentWallpaper = wallpaper;
    
    // Update basic info
    m_nameLabel->setText(wallpaper.name.isEmpty() ? "Unknown" : wallpaper.name);
    m_authorLabel->setText(wallpaper.author.isEmpty() ? "Unknown" : wallpaper.author);
    m_typeLabel->setText(wallpaper.type.isEmpty() ? "Unknown" : wallpaper.type);
    m_fileSizeLabel->setText(formatFileSize(wallpaper.fileSize));
    
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
    
    // Update description
    if (!wallpaper.description.isEmpty()) {
        m_descriptionEdit->setText(wallpaper.description);
    } else {
        m_descriptionEdit->setText("No description available.");
    }
    
    // Update preview
    updatePreview(wallpaper);
    
    // Update properties
    updateProperties(wallpaper.properties);
    
    // Load and update settings for this wallpaper
    loadWallpaperSettings(wallpaper.id);
    
    // Enable launch button
    m_launchButton->setEnabled(!wallpaper.id.isEmpty());
    
    // Fetch Steam API metadata if available
    updateSteamApiMetadata(wallpaper);
    
    qCDebug(propertiesPanel) << "setWallpaper completed for:" << wallpaper.name;
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
    
    m_propertiesWidget = new QWidget;
    auto* layout = new QFormLayout(m_propertiesWidget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setVerticalSpacing(12);
    layout->setHorizontalSpacing(16);
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->setLabelAlignment(Qt::AlignLeft);
    
    m_propertyWidgets.clear();
    m_originalValues.clear();
    
    if (properties.isEmpty()) {
        auto* noPropsLabel = new QLabel("No properties available");
        noPropsLabel->setStyleSheet("color: #666; font-style: italic; padding: 20px;");
        noPropsLabel->setAlignment(Qt::AlignCenter);
        layout->addRow(noPropsLabel);
    } else {
        addPropertiesFromObject(layout, properties, "");
    }
    
    // Ensure proper sizing
    m_propertiesWidget->setMinimumSize(m_propertiesWidget->sizeHint());
    m_scrollArea->setWidget(m_propertiesWidget);
    
    m_propertiesModified = false;
    m_savePropertiesButton->setEnabled(false);
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
                
                // Store original value for comparison
                m_originalValues[propName] = value;
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
    }
    
    return widget;
}

void PropertiesPanel::onPropertyChanged()
{
    // Mark properties as modified to enable the save button
    m_propertiesModified = true;
    m_savePropertiesButton->setEnabled(true);
    
    // Provide visual indication the properties have changed
    m_savePropertiesButton->setStyleSheet("QPushButton { background-color: #4CAF50; font-weight: bold; }");
}

void PropertiesPanel::onSavePropertiesClicked()
{
    QJsonObject modifiedProperties = saveCurrentProperties();
    
    // Save to cache file
    bool saved = saveCachedProperties(m_currentWallpaper.id, modifiedProperties);
    if (saved) {
        qCDebug(propertiesPanel) << "Properties saved successfully for wallpaper:" << m_currentWallpaper.id;
        
        // Reset the modified state
        m_propertiesModified = false;
        m_savePropertiesButton->setEnabled(false);
        m_savePropertiesButton->setStyleSheet("QPushButton { background-color: #4CAF50; }");
        
        // If wallpaper is running, ask to restart it
        if (m_isWallpaperRunning) {
            restartWallpaperWithChanges();
        }
    } else {
        qCWarning(propertiesPanel) << "Failed to save properties for wallpaper:" << m_currentWallpaper.id;
    }
}

void PropertiesPanel::onSettingChanged()
{
    // Update current settings from UI controls
    m_currentSettings.silent = m_silentCheckBox->isChecked();
    m_currentSettings.volume = m_volumeSlider->value();
    m_currentSettings.noAutoMute = m_noAutoMuteCheckBox->isChecked();
    m_currentSettings.noAudioProcessing = m_noAudioProcessingCheckBox->isChecked();
    m_currentSettings.fps = m_fpsSpinBox->value();
    m_currentSettings.windowGeometry = m_windowGeometryEdit->text();
    m_currentSettings.screenRoot = m_screenRootCombo->currentText() == "Default" ? "" : m_screenRootCombo->currentText();
    m_currentSettings.backgroundId = m_backgroundIdEdit->text();
    m_currentSettings.scaling = m_scalingCombo->currentText();
    m_currentSettings.clamping = m_clampingCombo->currentText();
    m_currentSettings.disableMouse = m_disableMouseCheckBox->isChecked();
    m_currentSettings.disableParallax = m_disableParallaxCheckBox->isChecked();
    m_currentSettings.noFullscreenPause = m_noFullscreenPauseCheckBox->isChecked();
    
    m_settingsModified = true;
    m_saveSettingsButton->setEnabled(true);
    
    // Emit signal to notify that settings have changed
    emit settingsChanged(m_currentWallpaper.id, m_currentSettings);
}

void PropertiesPanel::onSaveSettingsClicked()
{
    if (!m_currentWallpaper.id.isEmpty()) {
        if (saveWallpaperSettings(m_currentWallpaper.id)) {
            m_settingsModified = false;
            m_saveSettingsButton->setEnabled(false);
            qCDebug(propertiesPanel) << "Settings saved successfully for wallpaper:" << m_currentWallpaper.id;
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

void PropertiesPanel::restartWallpaperWithChanges()
{
    if (m_currentWallpaper.id.isEmpty()) {
        qCWarning(propertiesPanel) << "Cannot restart wallpaper: no current wallpaper";
        return;
    }
    
    qCDebug(propertiesPanel) << "Restarting wallpaper with new changes:" << m_currentWallpaper.name;
    
    // Emit the launch signal to restart the wallpaper with new settings
    emit launchWallpaper(m_currentWallpaper);
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
    settingsObj["backgroundId"] = m_currentSettings.backgroundId;
    settingsObj["scaling"] = m_currentSettings.scaling;
    settingsObj["clamping"] = m_currentSettings.clamping;
    settingsObj["disableMouse"] = m_currentSettings.disableMouse;
    settingsObj["disableParallax"] = m_currentSettings.disableParallax;
    settingsObj["noFullscreenPause"] = m_currentSettings.noFullscreenPause;
    
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
        // Use default settings
        m_currentSettings = WallpaperSettings();
        updateSettingsControls();
        return false;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        m_currentSettings = WallpaperSettings();
        updateSettingsControls();
        return false;
    }
    
    QJsonObject obj = doc.object();
    m_currentSettings.silent = obj["silent"].toBool();
    m_currentSettings.volume = obj["volume"].toInt(15);
    m_currentSettings.noAutoMute = obj["noAutoMute"].toBool();
    m_currentSettings.noAudioProcessing = obj["noAudioProcessing"].toBool();
    m_currentSettings.fps = obj["fps"].toInt(30);
    m_currentSettings.windowGeometry = obj["windowGeometry"].toString();
    m_currentSettings.screenRoot = obj["screenRoot"].toString();
    m_currentSettings.backgroundId = obj["backgroundId"].toString();
    m_currentSettings.scaling = obj["scaling"].toString("default");
    m_currentSettings.clamping = obj["clamping"].toString("clamp");
    m_currentSettings.disableMouse = obj["disableMouse"].toBool();
    m_currentSettings.disableParallax = obj["disableParallax"].toBool();
    m_currentSettings.noFullscreenPause = obj["noFullscreenPause"].toBool();
    
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
    // Add common screen identifiers
    screens << "HDMI-A-1" << "HDMI-A-2" << "DP-1" << "DP-2" << "eDP-1" << "VGA-1";
    return screens;
}

void PropertiesPanel::updateSettingsControls()
{
    // Block signals to prevent triggering onSettingChanged
    m_silentCheckBox->blockSignals(true);
    m_volumeSlider->blockSignals(true);
    m_noAutoMuteCheckBox->blockSignals(true);
    m_noAudioProcessingCheckBox->blockSignals(true);
    m_fpsSpinBox->blockSignals(true);
    m_windowGeometryEdit->blockSignals(true);
    m_screenRootCombo->blockSignals(true);
    m_backgroundIdEdit->blockSignals(true);
    m_scalingCombo->blockSignals(true);
    m_clampingCombo->blockSignals(true);
    m_disableMouseCheckBox->blockSignals(true);
    m_disableParallaxCheckBox->blockSignals(true);
    m_noFullscreenPauseCheckBox->blockSignals(true);
    
    // Update controls with current settings
    m_silentCheckBox->setChecked(m_currentSettings.silent);
    m_volumeSlider->setValue(m_currentSettings.volume);
    m_volumeLabel->setText(QString("%1%").arg(m_currentSettings.volume));
    m_noAutoMuteCheckBox->setChecked(m_currentSettings.noAutoMute);
    m_noAudioProcessingCheckBox->setChecked(m_currentSettings.noAudioProcessing);
    m_fpsSpinBox->setValue(m_currentSettings.fps);
    m_windowGeometryEdit->setText(m_currentSettings.windowGeometry);
    m_screenRootCombo->setCurrentText(m_currentSettings.screenRoot.isEmpty() ? "Default" : m_currentSettings.screenRoot);
    m_backgroundIdEdit->setText(m_currentSettings.backgroundId);
    m_scalingCombo->setCurrentText(m_currentSettings.scaling);
    m_clampingCombo->setCurrentText(m_currentSettings.clamping);
    m_disableMouseCheckBox->setChecked(m_currentSettings.disableMouse);
    m_disableParallaxCheckBox->setChecked(m_currentSettings.disableParallax);
    m_noFullscreenPauseCheckBox->setChecked(m_currentSettings.noFullscreenPause);
    
    // Re-enable signals
    m_silentCheckBox->blockSignals(false);
    m_volumeSlider->blockSignals(false);
    m_noAutoMuteCheckBox->blockSignals(false);
    m_noAudioProcessingCheckBox->blockSignals(false);
    m_fpsSpinBox->blockSignals(false);
    m_windowGeometryEdit->blockSignals(false);
    m_screenRootCombo->blockSignals(false);
    m_backgroundIdEdit->blockSignals(false);
    m_scalingCombo->blockSignals(false);
    m_clampingCombo->blockSignals(false);
    m_disableMouseCheckBox->blockSignals(false);
    m_disableParallaxCheckBox->blockSignals(false);
    m_noFullscreenPauseCheckBox->blockSignals(false);
    
    m_settingsModified = false;
    m_saveSettingsButton->setEnabled(false);
}

void PropertiesPanel::clear()
{
    qCDebug(propertiesPanel) << "Clearing properties panel";
    
    m_currentWallpaper = WallpaperInfo();
    m_currentSettings = WallpaperSettings(); // Reset settings to defaults
    
    m_propertyWidgets.clear();
    m_originalValues.clear();
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
        
        // If we got a value, create the property object
        if (!newValue.isUndefined()) {
            QJsonObject propObj;
            propObj["type"] = type;
            propObj["value"] = newValue;
            
            // Add other metadata if needed
            if (m_originalValues.contains(propName)) {
                QJsonValue originalValue = m_originalValues[propName];
                // You could add "text", "options", etc. here if needed
            }
            
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
