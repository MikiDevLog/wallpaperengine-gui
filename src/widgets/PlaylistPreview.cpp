#include "PlaylistPreview.h"
#include "../addons/WNELAddon.h"  // Add WNELAddon include
#include <QApplication>
#include <QStyle>
#include <QPixmap>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDrag>
#include <QPainter>
#include <QStyleOption>
#include <QDebug>
#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(playlistPreview, "app.playlistpreview")

PlaylistPreview::PlaylistPreview(WallpaperPlaylist* playlist, WallpaperManager* wallpaperManager, QWidget* parent)
    : QWidget(parent)
    , m_playlist(playlist)
    , m_wallpaperManager(wallpaperManager)
    , m_wnelAddon(nullptr)
    , m_selectedItem(nullptr)
    , m_currentItemsPerRow(PREFERRED_ITEMS_PER_ROW)
    , m_lastContainerWidth(0)
    , m_layoutUpdatePending(false)
    , m_dragStartPosition(-1)
    , m_isDragging(false)
{
    qCDebug(playlistPreview) << "PlaylistPreview::PlaylistPreview() - Constructor START";
    qCDebug(playlistPreview) << "PlaylistPreview::PlaylistPreview() - Playlist pointer:" << m_playlist;
    qCDebug(playlistPreview) << "PlaylistPreview::PlaylistPreview() - WallpaperManager pointer:" << m_wallpaperManager;
    
    setupUI();
    
    // Connect playlist signals
    if (m_playlist) {
        connect(m_playlist, &WallpaperPlaylist::wallpaperAdded, this, &PlaylistPreview::onWallpaperAdded);
        connect(m_playlist, &WallpaperPlaylist::wallpaperRemoved, this, &PlaylistPreview::onWallpaperRemoved);
        connect(m_playlist, &WallpaperPlaylist::wallpaperMoved, this, &PlaylistPreview::onWallpaperMoved);
        connect(m_playlist, &WallpaperPlaylist::playlistCleared, this, &PlaylistPreview::onPlaylistCleared);
        connect(m_playlist, &WallpaperPlaylist::currentWallpaperChanged, this, &PlaylistPreview::onCurrentWallpaperChanged);
        connect(m_playlist, &WallpaperPlaylist::playbackStarted, this, &PlaylistPreview::onPlaybackStarted);
        connect(m_playlist, &WallpaperPlaylist::playbackStopped, this, &PlaylistPreview::onPlaybackStopped);
        connect(m_playlist, &WallpaperPlaylist::settingsChanged, this, &PlaylistPreview::onSettingsChanged);
    }
    
    // Connect wallpaper manager signals
    if (m_wallpaperManager) {
        connect(m_wallpaperManager, &WallpaperManager::refreshFinished, this, &PlaylistPreview::onWallpaperManagerRefreshFinished);
        qCDebug(playlistPreview) << "PlaylistPreview::PlaylistPreview() - Connected to WallpaperManager refreshFinished signal";
    }
    
    // Enable drag and drop
    setAcceptDrops(true);
    
    // Initial refresh    
    qCDebug(playlistPreview) << "PlaylistPreview::PlaylistPreview() - About to call refreshPlaylist()";
    refreshPlaylist();
    qCDebug(playlistPreview) << "PlaylistPreview::PlaylistPreview() - Constructor END";
}

void PlaylistPreview::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);

    // Setup playback controls
    setupPlaylistControls();
    
    // Setup settings panel
    setupPlaylistSettings();
    
    // Setup scroll area for playlist items
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    m_scrollContent = new QWidget;
    m_gridLayout = new QGridLayout(m_scrollContent);
    m_gridLayout->setContentsMargins(5, 5, 5, 5);
    m_gridLayout->setSpacing(ITEM_SPACING);
    
    m_scrollArea->setWidget(m_scrollContent);
    m_mainLayout->addWidget(m_scrollArea, 1); // Give it most of the space
}

void PlaylistPreview::setupPlaylistControls()
{
    // Playback controls
    QGroupBox* controlsGroup = new QGroupBox("Playback Controls");
    QVBoxLayout* controlsMainLayout = new QVBoxLayout(controlsGroup);
    
    // Current wallpaper display
    m_currentWallpaperLabel = new QLabel("No wallpaper selected");
    m_currentWallpaperLabel->setAlignment(Qt::AlignCenter);
    m_currentWallpaperLabel->setStyleSheet("font-weight: bold; padding: 5px; border: 1px solid gray; border-radius: 3px;");
    controlsMainLayout->addWidget(m_currentWallpaperLabel);
    
    // Button layout
    m_controlsLayout = new QHBoxLayout;
    m_controlsLayout->setSpacing(5);
    
    m_previousButton = new QPushButton("Previous");
    m_previousButton->setEnabled(false);
    m_playPauseButton = new QPushButton("Start Playlist");
    m_nextButton = new QPushButton("Next");
    m_nextButton->setEnabled(false);
    m_clearButton = new QPushButton("Clear Playlist");
    
    m_controlsLayout->addWidget(m_previousButton);
    m_controlsLayout->addWidget(m_playPauseButton);
    m_controlsLayout->addWidget(m_nextButton);
    m_controlsLayout->addStretch();
    m_controlsLayout->addWidget(m_clearButton);
    
    controlsMainLayout->addLayout(m_controlsLayout);
    m_mainLayout->addWidget(controlsGroup);
    
    // Connect signals
    connect(m_playPauseButton, &QPushButton::clicked, this, &PlaylistPreview::onPlaybackControlClicked);
    connect(m_previousButton, &QPushButton::clicked, this, &PlaylistPreview::onPreviousButtonClicked);
    connect(m_nextButton, &QPushButton::clicked, this, &PlaylistPreview::onNextButtonClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &PlaylistPreview::onClearPlaylistClicked);
}

void PlaylistPreview::setupPlaylistSettings()
{
    m_settingsGroup = new QGroupBox("Playlist Settings");
    QGridLayout* settingsLayout = new QGridLayout(m_settingsGroup);
    
    // Enable checkbox
    m_enabledCheck = new QCheckBox("Enable automatic playback");
    settingsLayout->addWidget(m_enabledCheck, 0, 0, 1, 2);
    
    // Playback order
    settingsLayout->addWidget(new QLabel("Playback Order:"), 1, 0);
    m_orderCombo = new QComboBox;
    m_orderCombo->addItem("Cycle", static_cast<int>(PlaybackOrder::Cycle));
    m_orderCombo->addItem("Random", static_cast<int>(PlaybackOrder::Random));
    settingsLayout->addWidget(m_orderCombo, 1, 1);
    
    // Delay setting
    settingsLayout->addWidget(new QLabel("Delay (seconds):"), 2, 0);
    m_delaySpin = new QSpinBox;
    m_delaySpin->setRange(1, 3600); // 1 second to 1 hour
    m_delaySpin->setValue(300); // 5 minutes default
    m_delaySpin->setSuffix(" sec");
    settingsLayout->addWidget(m_delaySpin, 2, 1);
    
    m_mainLayout->addWidget(m_settingsGroup);
    
    // Connect settings signals
    connect(m_enabledCheck, &QCheckBox::toggled, this, &PlaylistPreview::onPlaylistSettingsChanged);
    connect(m_orderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlaylistPreview::onPlaylistSettingsChanged);
    connect(m_delaySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PlaylistPreview::onPlaylistSettingsChanged);
}

void PlaylistPreview::refreshPlaylist()
{
    qCDebug(playlistPreview) << "PlaylistPreview::refreshPlaylist() - START";
    updatePlaylistItems();
    updatePlaybackControls();
    updateSettingsUI();
    qCDebug(playlistPreview) << "PlaylistPreview::refreshPlaylist() - END";
}

void PlaylistPreview::updatePlaylistItems()
{
    qCDebug(playlistPreview) << "PlaylistPreview::updatePlaylistItems() - START";
    
    // Clear existing widgets
    clearCurrentItems();
    
    if (!m_playlist) {
        qCDebug(playlistPreview) << "PlaylistPreview::updatePlaylistItems() - No playlist available!";
        return;
    }
    
    // Add new widgets using responsive grid layout
    auto items = m_playlist->getPlaylistItems();
    qCDebug(playlistPreview) << "PlaylistPreview::updatePlaylistItems() - Found" << items.size() << "items in playlist";
    int itemsPerRow = calculateItemsPerRow();
    
    for (int i = 0; i < items.size(); ++i) {
        qCDebug(playlistPreview) << "PlaylistPreview::updatePlaylistItems() - Creating item" << i << "for wallpaper ID:" << items[i].wallpaperId;
        PlaylistPreviewItem* widget = createPlaylistPreviewItem(items[i], i);
        m_itemWidgets.append(widget);
        
        int row = i / itemsPerRow;
        int col = i % itemsPerRow;
        m_gridLayout->addWidget(widget, row, col);
        qCDebug(playlistPreview) << "PlaylistPreview::updatePlaylistItems() - Added item" << i << "to grid at row" << row << "col" << col;
    }
    
    // Add empty label if playlist is empty
    if (items.isEmpty()) {
        qCDebug(playlistPreview) << "PlaylistPreview::updatePlaylistItems() - Playlist is empty, showing empty label";
        QLabel* emptyLabel = new QLabel("Playlist is empty\nDrag wallpapers here or use 'Add to Playlist' button");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: gray; font-style: italic; padding: 20px;");
        m_gridLayout->addWidget(emptyLabel, 0, 0, 1, itemsPerRow);
    }
    
    qCDebug(playlistPreview) << "PlaylistPreview::updatePlaylistItems() - END";
}

PlaylistPreviewItem* PlaylistPreview::createPlaylistPreviewItem(const PlaylistItem& item, int index)
{
    PlaylistPreviewItem* widget = new PlaylistPreviewItem(item, index, this);
    
    // Set current state
    if (m_playlist && m_playlist->getCurrentWallpaperId() == item.wallpaperId) {
        widget->setCurrent(true);
    }
    
    // Connect signals
    connect(widget, &PlaylistPreviewItem::clicked, this, &PlaylistPreview::wallpaperSelected);
    connect(widget, &PlaylistPreviewItem::removeRequested, this, &PlaylistPreview::removeFromPlaylistRequested);
    connect(widget, &PlaylistPreviewItem::moveUpRequested, this, &PlaylistPreview::onMoveUpButtonClicked);
    connect(widget, &PlaylistPreviewItem::moveDownRequested, this, &PlaylistPreview::onMoveDownButtonClicked);
    
    return widget;
}

void PlaylistPreview::updatePlaybackControls()
{
    if (!m_playlist) {
        return;
    }
    
    bool hasItems = !m_playlist->isEmpty();
    bool isPlaying = m_playlist->getSettings().enabled;
    
    m_previousButton->setEnabled(hasItems);
    m_nextButton->setEnabled(hasItems);
    m_playPauseButton->setEnabled(hasItems);
    
    if (isPlaying) {
        m_playPauseButton->setText("Stop Playlist");
    } else {
        m_playPauseButton->setText("Start Playlist");
    }
    
    // Update current wallpaper label
    QString currentId = m_playlist->getCurrentWallpaperId();
    if (!currentId.isEmpty() && m_wallpaperManager) {
        auto wallpaperInfo = m_wallpaperManager->getWallpaperInfo(currentId);
        if (wallpaperInfo.has_value()) {
            m_currentWallpaperLabel->setText(QString("Current: %1").arg(wallpaperInfo->name));
        } else {
            m_currentWallpaperLabel->setText(QString("Current: %1").arg(currentId));
        }
    } else {
        m_currentWallpaperLabel->setText("No wallpaper selected");
    }
}

void PlaylistPreview::updateSettingsUI()
{
    if (!m_playlist) {
        return;
    }
    
    auto settings = m_playlist->getSettings();
    
    // Block signals to prevent recursive updates
    m_enabledCheck->blockSignals(true);
    m_orderCombo->blockSignals(true);
    m_delaySpin->blockSignals(true);
    
    m_enabledCheck->setChecked(settings.enabled);
    m_orderCombo->setCurrentIndex(static_cast<int>(settings.order));
    m_delaySpin->setValue(settings.delaySeconds);
    
    m_enabledCheck->blockSignals(false);
    m_orderCombo->blockSignals(false);
    m_delaySpin->blockSignals(false);
}

void PlaylistPreview::updateCurrentWallpaper(const QString& wallpaperId)
{
    m_currentWallpaperId = wallpaperId;
    
    // If item widgets are empty (e.g., during startup), refresh the playlist first
    if (m_itemWidgets.isEmpty() && m_playlist && !m_playlist->isEmpty()) {
        updatePlaylistItems();
    }
    
    // Update all item widgets
    for (auto* widget : m_itemWidgets) {
        widget->setCurrent(widget->playlistItem().wallpaperId == wallpaperId);
    }
    
    updatePlaybackControls();
}

// Slot implementations
void PlaylistPreview::onWallpaperAdded(const QString& wallpaperId)
{
    Q_UNUSED(wallpaperId)
    updatePlaylistItems();
    updatePlaybackControls();
}

void PlaylistPreview::onWallpaperRemoved(const QString& wallpaperId)
{
    Q_UNUSED(wallpaperId)
    updatePlaylistItems();
    updatePlaybackControls();
}

void PlaylistPreview::onWallpaperMoved(int fromIndex, int toIndex)
{
    Q_UNUSED(fromIndex)
    Q_UNUSED(toIndex)
    updatePlaylistItems();
}

void PlaylistPreview::onPlaylistCleared()
{
    updatePlaylistItems();
    updatePlaybackControls();
}

void PlaylistPreview::onCurrentWallpaperChanged(const QString& wallpaperId)
{
    updateCurrentWallpaper(wallpaperId);
}

void PlaylistPreview::onPlaybackStarted()
{
    updatePlaybackControls();
}

void PlaylistPreview::onPlaybackStopped()
{
    updatePlaybackControls();
}

void PlaylistPreview::onSettingsChanged()
{
    updateSettingsUI();
    updatePlaybackControls();
}

void PlaylistPreview::onRemoveButtonClicked()
{
    // This will be handled by individual item widgets
}

void PlaylistPreview::onMoveUpButtonClicked()
{
    auto* sender = qobject_cast<PlaylistPreviewItem*>(QObject::sender());
    if (sender && m_playlist) {
        int index = sender->getIndex();
        if (index > 0) {
            m_playlist->moveWallpaper(index, index - 1);
        }
    }
}

void PlaylistPreview::onMoveDownButtonClicked()
{
    auto* sender = qobject_cast<PlaylistPreviewItem*>(QObject::sender());
    if (sender && m_playlist) {
        int index = sender->getIndex();
        if (index < m_playlist->size() - 1) {
            m_playlist->moveWallpaper(index, index + 1);
        }
    }
}

void PlaylistPreview::onClearPlaylistClicked()
{
    if (m_playlist) {
        m_playlist->clearPlaylist();
    }
}

void PlaylistPreview::onPlaylistSettingsChanged()
{
    if (!m_playlist) {
        return;
    }
    
    PlaylistSettings settings;
    settings.enabled = m_enabledCheck->isChecked();
    settings.order = static_cast<PlaybackOrder>(m_orderCombo->currentData().toInt());
    settings.delaySeconds = m_delaySpin->value();
    
    m_playlist->setSettings(settings);
}

void PlaylistPreview::onPlaybackControlClicked()
{
    if (!m_playlist) {
        return;
    }
    
    if (m_playlist->getSettings().enabled) {
        m_playlist->setEnabled(false);
    } else {
        m_playlist->setEnabled(true);
    }
}

void PlaylistPreview::onNextButtonClicked()
{
    if (m_playlist) {
        m_playlist->nextWallpaper();
    }
}

void PlaylistPreview::onPreviousButtonClicked()
{
    if (m_playlist) {
        m_playlist->previousWallpaper();
    }
}

void PlaylistPreview::onWallpaperManagerRefreshFinished()
{
    qCDebug(playlistPreview) << "PlaylistPreview::onWallpaperManagerRefreshFinished() - WallpaperManager refresh completed, updating playlist items";
    
    // Now that wallpapers are loaded, refresh the playlist items to get proper names and previews
    updatePlaylistItems();
    updatePlaybackControls();
}

// Drag and drop implementation
void PlaylistPreview::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-wallpaper-id")) {
        if (event->possibleActions() & Qt::CopyAction) {
            event->setDropAction(Qt::CopyAction);
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->ignore();
    }
}

void PlaylistPreview::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-wallpaper-id")) {
        if (event->possibleActions() & Qt::CopyAction) {
            event->setDropAction(Qt::CopyAction);
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->ignore();
    }
}

void PlaylistPreview::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-wallpaper-id")) {
        QByteArray data = event->mimeData()->data("application/x-wallpaper-id");
        QString wallpaperId = QString::fromUtf8(data);
        
        if (m_playlist && !wallpaperId.isEmpty()) {
            m_playlist->addWallpaper(wallpaperId);
        }
        
        event->setDropAction(Qt::CopyAction);
        event->accept();
    } else {
        event->ignore();
    }
}

void PlaylistPreview::resizeEvent(QResizeEvent* event)
{
    if (!event) {
        return;
    }
    
    QWidget::resizeEvent(event);
    
    if (m_gridLayout && isVisible()) {
        if (!m_layoutUpdatePending) {
            m_layoutUpdatePending = true;
            QTimer* layoutTimer = new QTimer(this);
            layoutTimer->setSingleShot(true);
            connect(layoutTimer, &QTimer::timeout, this, [this, layoutTimer]() {
                m_layoutUpdatePending = false;
                if (isVisible()) {
                    recalculateLayout();
                }
                layoutTimer->deleteLater();
            });
            layoutTimer->start(50);
        }
    }
}

int PlaylistPreview::calculateItemsPerRow() const
{
    if (!m_scrollArea) {
        return PREFERRED_ITEMS_PER_ROW;
    }
    
    int availableWidth = m_scrollArea->viewport()->width() - 2 * ITEM_SPACING;
    int itemWidthWithSpacing = PlaylistPreviewItem::ITEM_WIDTH + ITEM_SPACING;
    
    int itemsPerRow = qMax(MIN_ITEMS_PER_ROW, availableWidth / itemWidthWithSpacing);
    return qMin(itemsPerRow, MAX_ITEMS_PER_ROW);
}

void PlaylistPreview::recalculateLayout()
{
    if (!m_gridLayout || !isVisible()) {
        return;
    }
    
    QSize currentSize = size();
    if (currentSize.isEmpty() || currentSize.width() <= 0) {
        return;
    }
    
    int newItemsPerRow = calculateItemsPerRow();
    
    if (newItemsPerRow != m_currentItemsPerRow) {
        adjustGridForNewColumnCount(newItemsPerRow);
        m_currentItemsPerRow = newItemsPerRow;
    }
    
    m_lastContainerWidth = currentSize.width();
}

void PlaylistPreview::adjustGridForNewColumnCount(int newColumnCount)
{
    if (!m_gridLayout || newColumnCount <= 0) {
        return;
    }
    
    QList<PlaylistPreviewItem*> allItems;
    
    for (int i = 0; i < m_gridLayout->count(); ++i) {
        QLayoutItem* item = m_gridLayout->itemAt(i);
        if (item && item->widget()) {
            PlaylistPreviewItem* previewItem = qobject_cast<PlaylistPreviewItem*>(item->widget());
            if (previewItem) {
                allItems.append(previewItem);
            }
        }
    }
    
    while (QLayoutItem* item = m_gridLayout->takeAt(0)) {
        delete item;
    }
    
    int row = 0, col = 0;
    for (PlaylistPreviewItem* item : allItems) {
        if (item) {
            m_gridLayout->addWidget(item, row, col);
            
            col++;
            if (col >= newColumnCount) {
                col = 0;
                row++;
            }
        }
    }
    
    m_gridLayout->setHorizontalSpacing(ITEM_SPACING);
    m_gridLayout->setVerticalSpacing(ITEM_SPACING);
}

void PlaylistPreview::clearCurrentItems()
{
    for (auto* widget : m_itemWidgets) {
        widget->deleteLater();
    }
    m_itemWidgets.clear();
    m_selectedItem = nullptr;
}

// PlaylistPreviewItem implementation (replaces PlaylistItemWidget)
PlaylistPreviewItem::PlaylistPreviewItem(const PlaylistItem& item, int index, PlaylistPreview* parent)
    : QWidget(parent)
    , m_item(item)
    , m_index(index)
    , m_isCurrent(false)
    , m_selected(false)
    , m_playlistPreview(parent)
    , m_previewMovie(nullptr)
    , m_useCustomPainting(true)
{
    setFixedSize(ITEM_WIDTH, ITEM_HEIGHT + 20);
    
    // Load wallpaper info using parent's method that checks both regular and external wallpapers
    if (m_playlistPreview) {
        auto wallpaperInfoOpt = m_playlistPreview->getWallpaperInfo(m_item.wallpaperId);
        if (wallpaperInfoOpt.has_value()) {
            m_wallpaperInfo = wallpaperInfoOpt.value();
            qCDebug(playlistPreview) << "PlaylistPreviewItem: Found wallpaper info for ID:" << m_item.wallpaperId << "Name:" << m_wallpaperInfo.name << "Preview:" << m_wallpaperInfo.previewPath;
        } else {
            // Fallback wallpaper info
            m_wallpaperInfo.id = m_item.wallpaperId;
            m_wallpaperInfo.name = QString("Wallpaper %1").arg(m_item.wallpaperId);
            m_wallpaperInfo.author = "Unknown";
            m_wallpaperInfo.type = "Unknown";
            m_wallpaperInfo.previewPath = ""; // Will try to find it
            qCDebug(playlistPreview) << "PlaylistPreviewItem: No wallpaper info found for ID:" << m_item.wallpaperId << "Using fallback";
        }
    } else {
        // No playlist preview parent - create minimal info
        m_wallpaperInfo.id = m_item.wallpaperId;
        m_wallpaperInfo.name = QString("Wallpaper %1").arg(m_item.wallpaperId);
        m_wallpaperInfo.author = "Unknown";
        m_wallpaperInfo.type = "Unknown";
        m_wallpaperInfo.previewPath = "";
        qCDebug(playlistPreview) << "PlaylistPreviewItem: No PlaylistPreview parent available for ID:" << m_item.wallpaperId;
    }
    
    setupUI();
    loadPreviewImage();
}

void PlaylistPreviewItem::setupUI()
{
    setContentsMargins(0, 0, 0, 0);
    
    // Create control buttons (positioned over the preview)
    m_removeButton = new QPushButton("×", this);
    m_removeButton->setFixedSize(24, 24);
    m_removeButton->setStyleSheet(
        "QPushButton { background-color: rgba(231, 76, 60, 200); color: white; border: none; border-radius: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: rgba(231, 76, 60, 255); }"
    );
    m_removeButton->setToolTip("Remove from playlist");
    
    m_moveUpButton = new QPushButton("↑", this);
    m_moveUpButton->setFixedSize(24, 24);
    m_moveUpButton->setStyleSheet(
        "QPushButton { background-color: rgba(52, 152, 219, 200); color: white; border: none; border-radius: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: rgba(52, 152, 219, 255); }"
    );
    m_moveUpButton->setToolTip("Move up");
    
    m_moveDownButton = new QPushButton("↓", this);
    m_moveDownButton->setFixedSize(24, 24);
    m_moveDownButton->setStyleSheet(
        "QPushButton { background-color: rgba(52, 152, 219, 200); color: white; border: none; border-radius: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: rgba(52, 152, 219, 255); }"
    );
    m_moveDownButton->setToolTip("Move down");
    
    m_positionLabel = new QLabel(QString::number(m_index + 1), this);
    m_positionLabel->setFixedSize(30, 30);
    m_positionLabel->setAlignment(Qt::AlignCenter);
    m_positionLabel->setStyleSheet(
        "QLabel { background-color: rgba(52, 152, 219, 200); color: white; border: none; border-radius: 15px; font-weight: bold; }"
    );
    
    // Connect signals
    connect(m_removeButton, &QPushButton::clicked, this, &PlaylistPreviewItem::onRemoveClicked);
    connect(m_moveUpButton, &QPushButton::clicked, this, &PlaylistPreviewItem::onMoveUpClicked);
    connect(m_moveDownButton, &QPushButton::clicked, this, &PlaylistPreviewItem::onMoveDownClicked);
}

void PlaylistPreviewItem::loadPreviewImage()
{
    qCDebug(playlistPreview) << "PlaylistPreviewItem::loadPreviewImage() for ID:" << m_wallpaperInfo.id << "Path:" << m_wallpaperInfo.previewPath;
    
    if (!m_wallpaperInfo.previewPath.isEmpty() && QFileInfo::exists(m_wallpaperInfo.previewPath)) {
        qCDebug(playlistPreview) << "Loading preview image from:" << m_wallpaperInfo.previewPath;
        // Check if it's an animated preview first
        if (hasAnimatedPreview()) {
            loadAnimatedPreview();
            return;
        }
        
        // Load static image
        QPixmap pixmap(m_wallpaperInfo.previewPath);
        if (!pixmap.isNull()) {
            qCDebug(playlistPreview) << "Successfully loaded static preview, size:" << pixmap.size();
            setPreviewPixmap(pixmap);
        } else {
            qCDebug(playlistPreview) << "Failed to load pixmap from:" << m_wallpaperInfo.previewPath;
        }
    } else {
        qCDebug(playlistPreview) << "Preview path is empty or doesn't exist:" << m_wallpaperInfo.previewPath;
    }
}

void PlaylistPreviewItem::setPreviewPixmap(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        return;
    }
    
    m_scaledPreview = scalePreviewKeepAspectRatio(pixmap);
    update();
}

QPixmap PlaylistPreviewItem::scalePreviewKeepAspectRatio(const QPixmap& original)
{
    QSize containerSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
    QSize scaledSize = calculateFitSize(original.size(), containerSize);
    
    return original.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QSize PlaylistPreviewItem::calculateFitSize(const QSize& imageSize, const QSize& containerSize)
{
    if (imageSize.isEmpty() || containerSize.isEmpty()) {
        return containerSize;
    }
    
    QSize result = imageSize.scaled(containerSize, Qt::KeepAspectRatio);
    return result;
}

bool PlaylistPreviewItem::hasAnimatedPreview() const
{
    if (m_wallpaperInfo.previewPath.isEmpty() || !QFileInfo::exists(m_wallpaperInfo.previewPath)) {
        return false;
    }
    
    QString lowerPath = m_wallpaperInfo.previewPath.toLower();
    return lowerPath.endsWith(".gif") || lowerPath.endsWith(".webp");
}

void PlaylistPreviewItem::loadAnimatedPreview()
{
    if (!hasAnimatedPreview()) {
        return;
    }
    
    // Clean up existing movie if any
    if (m_previewMovie) {
        m_previewMovie->stop();
        m_previewMovie->deleteLater();
        m_previewMovie = nullptr;
    }
    
    // Create and configure QMovie for animation
    m_previewMovie = new QMovie(m_wallpaperInfo.previewPath, QByteArray(), this);
    
    if (!m_previewMovie->isValid()) {
        m_previewMovie->deleteLater();
        m_previewMovie = nullptr;
        // Fall back to static image
        QPixmap staticPixmap(m_wallpaperInfo.previewPath);
        if (!staticPixmap.isNull()) {
            setPreviewPixmap(staticPixmap);
        }
        return;
    }
    
    // Connect to movie signals for frame updates
    connect(m_previewMovie, &QMovie::frameChanged, this, [this](int frameNumber) {
        Q_UNUSED(frameNumber)
        if (m_previewMovie) {
            QPixmap currentFrame = m_previewMovie->currentPixmap();
            if (!currentFrame.isNull()) {
                m_scaledPreview = scalePreviewKeepAspectRatio(currentFrame);
                update();
            }
        }
    });
    
    // Get the first frame
    m_previewMovie->jumpToFrame(0);
    QPixmap firstFrame = m_previewMovie->currentPixmap();
    if (!firstFrame.isNull()) {
        m_scaledPreview = scalePreviewKeepAspectRatio(firstFrame);
        update();
    }
}

void PlaylistPreviewItem::startAnimation()
{
    if (m_previewMovie && m_previewMovie->isValid()) {
        m_previewMovie->start();
    }
}

void PlaylistPreviewItem::stopAnimation()
{
    if (m_previewMovie && m_previewMovie->isValid()) {
        m_previewMovie->stop();
    }
}

void PlaylistPreviewItem::setIndex(int index)
{
    m_index = index;
    m_positionLabel->setText(QString::number(index + 1));
}

void PlaylistPreviewItem::setCurrent(bool current)
{
    m_isCurrent = current;
    update(); // Repaint to show current state
}

void PlaylistPreviewItem::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        update();
    }
}

void PlaylistPreviewItem::paintEvent(QPaintEvent* event)
{
    if (!event || !isVisible()) {
        return;
    }
    
    QSize currentSize = size();
    if (currentSize.isEmpty() || currentSize.width() <= 0 || currentSize.height() <= 0) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    // Background with selection/current state
    QColor bgColor;
    if (m_isCurrent) {
        bgColor = QColor(52, 152, 219, 100); // Blue for current
    } else if (m_selected) {
        bgColor = palette().color(QPalette::Highlight);
    } else {
        bgColor = palette().color(QPalette::Base);
    }
    painter.fillRect(rect(), bgColor);
    
    // Border for current or selected
    if (m_isCurrent || m_selected) {
        QPen borderPen(m_isCurrent ? QColor(52, 152, 219) : palette().color(QPalette::Highlight), 3);
        painter.setPen(borderPen);
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
    }
    
    int availableWidth = currentSize.width() - 2 * PREVIEW_CONTAINER_MARGIN;
    int availableHeight = currentSize.height() - 2 * PREVIEW_CONTAINER_MARGIN;
    int previewHeight = qMin(PREVIEW_HEIGHT, availableHeight - TEXT_AREA_HEIGHT);
    int previewWidth = qMin(PREVIEW_WIDTH, availableWidth);
    
    // Draw preview image
    if (!m_scaledPreview.isNull()) {
        QRect previewRect = QRect(PREVIEW_CONTAINER_MARGIN, PREVIEW_CONTAINER_MARGIN, 
                                 previewWidth, previewHeight);
        
        QRect imageRect = previewRect;
        if (m_scaledPreview.size() != previewRect.size()) {
            QSize scaledSize = m_scaledPreview.size().scaled(previewRect.size(), Qt::KeepAspectRatio);
            imageRect = QRect(
                previewRect.x() + (previewRect.width() - scaledSize.width()) / 2,
                previewRect.y() + (previewRect.height() - scaledSize.height()) / 2,
                scaledSize.width(),
                scaledSize.height()
            );
        }
        
        painter.drawPixmap(imageRect, m_scaledPreview);
    } else {
        QRect previewRect = QRect(PREVIEW_CONTAINER_MARGIN, PREVIEW_CONTAINER_MARGIN, 
                                 previewWidth, previewHeight);
        
        if (previewRect.width() > 0 && previewRect.height() > 0) {
            painter.fillRect(previewRect, QColor(60, 60, 60));
            painter.setPen(QColor(120, 120, 120));
            painter.drawText(previewRect, Qt::AlignCenter, "Loading...");
        }
    }
    
    // Draw text information
    int textY = PREVIEW_CONTAINER_MARGIN + previewHeight + TEXT_MARGIN;
    int maxTextWidth = availableWidth - TEXT_MARGIN;
    int maxTextHeight = currentSize.height() - textY - TEXT_MARGIN;
    
    if (maxTextWidth > 0 && maxTextHeight > 0) {
        QRect textRect = QRect(PREVIEW_CONTAINER_MARGIN + TEXT_MARGIN/2, textY, maxTextWidth, maxTextHeight);
        
        painter.setPen(palette().color(QPalette::Text));
        
        QFont nameFont = font();
        nameFont.setBold(true);
        nameFont.setPointSize(qMax(8, font().pointSize()));
        
        QString displayName = m_wallpaperInfo.name.isEmpty() ? "Unknown" : m_wallpaperInfo.name;
        
        QFontMetrics nameFm(nameFont);
        int nameLineHeight = nameFm.height();
        int maxNameLines = qMax(1, qMin(3, maxTextHeight / nameLineHeight - 2));
        
        QRect nameRect = QRect(textRect.x(), textRect.y(), textRect.width(), nameLineHeight * maxNameLines);
        drawTextWithWordWrap(painter, displayName, nameRect, nameFont, palette().color(QPalette::Text));
        
        QFont infoFont = font();
        infoFont.setPointSize(qMax(7, font().pointSize() - 1));
        
        int infoY = nameRect.bottom() + TEXT_MARGIN/2;
        QRect infoRect = QRect(textRect.x(), infoY, textRect.width(), textRect.bottom() - infoY);
        
        if (infoRect.height() > 0) {
            QStringList infoLines;
            if (!m_wallpaperInfo.type.isEmpty()) {
                infoLines << QString("Type: %1").arg(m_wallpaperInfo.type);
            }
            
            QString infoText = infoLines.join(" • ");
            if (!infoText.isEmpty()) {
                painter.setPen(palette().color(QPalette::Mid));
                drawTextWithWordWrap(painter, infoText, infoRect, infoFont, palette().color(QPalette::Mid));
            }
        }
    }
}

void PlaylistPreviewItem::drawTextWithWordWrap(QPainter& painter, const QString& text, const QRect& rect, 
                                             const QFont& font, const QColor& color, Qt::Alignment alignment)
{
    if (text.isEmpty() || rect.isEmpty()) {
        return;
    }
    
    painter.setFont(font);
    painter.setPen(color);
    
    QFontMetrics fm(font);
    QStringList words = text.split(' ', Qt::SkipEmptyParts);
    
    if (words.isEmpty()) {
        return;
    }
    
    QStringList lines;
    QString currentLine;
    
    for (const QString& word : words) {
        QString testLine = currentLine.isEmpty() ? word : currentLine + " " + word;
        
        if (fm.horizontalAdvance(testLine) <= rect.width()) {
            currentLine = testLine;
        } else {
            if (!currentLine.isEmpty()) {
                lines.append(currentLine);
                currentLine = word;
            } else {
                currentLine = fm.elidedText(word, Qt::ElideRight, rect.width());
                lines.append(currentLine);
                currentLine = "";
            }
        }
    }
    
    if (!currentLine.isEmpty()) {
        lines.append(currentLine);
    }
    
    int lineHeight = fm.height();
    int startY = rect.y();
    
    Qt::Alignment vAlign = alignment & Qt::AlignVertical_Mask;
    if (vAlign == Qt::AlignVCenter) {
        int usedHeight = lines.size() * lineHeight;
        startY = rect.y() + (rect.height() - usedHeight) / 2;
    } else if (vAlign == Qt::AlignBottom) {
        int usedHeight = lines.size() * lineHeight;
        startY = rect.bottom() - usedHeight;
    }
    
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QRect lineRect = QRect(rect.x(), startY + i * lineHeight, rect.width(), lineHeight);
        
        Qt::Alignment hAlign = alignment & Qt::AlignHorizontal_Mask;
        if (hAlign == 0) hAlign = Qt::AlignLeft;
        
        painter.drawText(lineRect, hAlign | Qt::AlignVCenter, line);
    }
}

void PlaylistPreviewItem::resizeEvent(QResizeEvent* event)
{
    if (!event) {
        return;
    }
    
    QWidget::resizeEvent(event);
    
    // Position control buttons over the preview area
    int buttonSize = 24;
    int margin = 4;
    
    // Position buttons in top-right corner of preview area
    int buttonX = PREVIEW_CONTAINER_MARGIN + PREVIEW_WIDTH - buttonSize - margin;
    int buttonY = PREVIEW_CONTAINER_MARGIN + margin;
    
    m_removeButton->move(buttonX, buttonY);
    m_moveUpButton->move(buttonX, buttonY + buttonSize + 2);
    m_moveDownButton->move(buttonX, buttonY + 2 * (buttonSize + 2));
    
    // Position label in top-left corner
    m_positionLabel->move(PREVIEW_CONTAINER_MARGIN + margin, PREVIEW_CONTAINER_MARGIN + margin);
    
    if (isVisible() && !m_wallpaperInfo.id.isEmpty()) {
        updateTextLayout();
    }
}

void PlaylistPreviewItem::updateTextLayout()
{
    if (!isVisible() || size().isEmpty() || size().width() <= 0 || size().height() <= 0) {
        return;
    }
    
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, timer]() {
        if (isVisible()) {
            update();
        }
        timer->deleteLater();
    });
    timer->start(0);
}

void PlaylistPreviewItem::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = event->pos();
        emit clicked(m_item.wallpaperId);
    }
    QWidget::mousePressEvent(event);
}

void PlaylistPreviewItem::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_item.wallpaperId);
    }
    QWidget::mouseDoubleClickEvent(event);
}

void PlaylistPreviewItem::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    int distance = (event->pos() - m_dragStartPosition).manhattanLength();
    if (distance < QApplication::startDragDistance()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // Start drag operation
    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = new QMimeData;
    
    // Set the wallpaper ID as MIME data
    mimeData->setText(m_item.wallpaperId);
    mimeData->setData("application/x-wallpaper-id", m_item.wallpaperId.toUtf8());
    
    drag->setMimeData(mimeData);
    
    // Create a drag pixmap from the preview
    QPixmap dragPixmap;
    if (!m_scaledPreview.isNull()) {
        dragPixmap = m_scaledPreview.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        dragPixmap = QPixmap(64, 64);
        dragPixmap.fill(Qt::gray);
    }
    
    drag->setPixmap(dragPixmap);
    drag->setHotSpot(QPoint(dragPixmap.width() / 2, dragPixmap.height() / 2));
    
    // Execute the drag
    drag->exec(Qt::CopyAction);
    
    QWidget::mouseMoveEvent(event);
}

void PlaylistPreviewItem::onRemoveClicked()
{
    emit removeRequested(m_item.wallpaperId);
}

void PlaylistPreviewItem::onMoveUpClicked()
{
    emit moveUpRequested(m_index);
}

void PlaylistPreviewItem::onMoveDownClicked()
{
    emit moveDownRequested(m_index);
}

void PlaylistPreview::setWNELAddon(WNELAddon* addon)
{
    m_wnelAddon = addon;
    // Refresh playlist to get proper external wallpaper info
    refreshPlaylist();
}

std::optional<WallpaperInfo> PlaylistPreview::getWallpaperInfo(const QString& wallpaperId) const
{
    // First try to get from regular wallpapers
    if (m_wallpaperManager) {
        auto wallpaperInfo = m_wallpaperManager->getWallpaperInfo(wallpaperId);
        if (wallpaperInfo.has_value()) {
            return wallpaperInfo;
        }
    }
    
    // If not found in regular wallpapers, try external wallpapers
    if (m_wnelAddon) {
        QList<ExternalWallpaperInfo> externalWallpapers = m_wnelAddon->getAllExternalWallpapers();
        for (const ExternalWallpaperInfo& external : externalWallpapers) {
            if (external.id == wallpaperId) {
                return external.toWallpaperInfo();
            }
        }
    }
    
    return std::nullopt;
}
