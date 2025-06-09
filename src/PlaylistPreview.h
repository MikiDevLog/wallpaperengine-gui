#ifndef PLAYLISTPREVIEW_H
#define PLAYLISTPREVIEW_H

#include <QWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QFrame>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDrag>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QMovie>
#include <QNetworkAccessManager>
#include <QRandomGenerator>
#include "WallpaperPlaylist.h"
#include "WallpaperManager.h"

class PlaylistPreviewItem;

class PlaylistPreview : public QWidget
{
    Q_OBJECT

public:
    explicit PlaylistPreview(WallpaperPlaylist* playlist, WallpaperManager* wallpaperManager, QWidget* parent = nullptr);

    void refreshPlaylist();
    void updateCurrentWallpaper(const QString& wallpaperId);

    // Layout constants similar to WallpaperPreview
    static constexpr int ITEM_SPACING = 16;
    static constexpr int MIN_ITEMS_PER_ROW = 1;
    static constexpr int MAX_ITEMS_PER_ROW = 8;
    static constexpr int PREFERRED_ITEMS_PER_ROW = 4;

public slots:
    void onWallpaperAdded(const QString& wallpaperId);
    void onWallpaperRemoved(const QString& wallpaperId);
    void onWallpaperMoved(int fromIndex, int toIndex);
    void onPlaylistCleared();
    void onCurrentWallpaperChanged(const QString& wallpaperId);
    void onPlaybackStarted();
    void onPlaybackStopped();
    void onSettingsChanged();
    void onWallpaperManagerRefreshFinished();

signals:
    void wallpaperSelected(const QString& wallpaperId);
    void removeFromPlaylistRequested(const QString& wallpaperId);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onRemoveButtonClicked();
    void onMoveUpButtonClicked();
    void onMoveDownButtonClicked();
    void onClearPlaylistClicked();
    void onPlaylistSettingsChanged();
    void onPlaybackControlClicked();
    void onNextButtonClicked();
    void onPreviousButtonClicked();

private:
    void setupUI();
    void setupPlaylistControls();
    void setupPlaylistSettings();
    void updatePlaylistItems();
    void updatePlaybackControls();
    void updateSettingsUI();
    PlaylistPreviewItem* createPlaylistPreviewItem(const PlaylistItem& item, int index);
    void moveItem(int fromIndex, int toIndex);
    int getDropPosition(const QPoint& pos);
    
    // Responsive layout methods (similar to WallpaperPreview)
    int calculateItemsPerRow() const;
    int getCurrentItemsPerRow() const { return m_currentItemsPerRow; }
    void recalculateLayout();
    void adjustGridForNewColumnCount(int newColumnCount);
    void clearCurrentItems();

    WallpaperPlaylist* m_playlist;
    WallpaperManager* m_wallpaperManager;

    // UI Components
    QVBoxLayout* m_mainLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_scrollContent;
    QGridLayout* m_gridLayout;
    
    // Controls
    QHBoxLayout* m_controlsLayout;
    QPushButton* m_playPauseButton;
    QPushButton* m_previousButton;
    QPushButton* m_nextButton;
    QPushButton* m_clearButton;
    QLabel* m_currentWallpaperLabel;
    
    // Settings panel
    QGroupBox* m_settingsGroup;
    QComboBox* m_orderCombo;
    QSpinBox* m_delaySpin;
    QCheckBox* m_enabledCheck;
    
    // Item widgets
    QList<PlaylistPreviewItem*> m_itemWidgets;
    PlaylistPreviewItem* m_selectedItem;
    QString m_currentWallpaperId;
    
    // Responsive layout state
    int m_currentItemsPerRow;
    int m_lastContainerWidth;
    bool m_layoutUpdatePending;
    
    // Drag and drop
    int m_dragStartPosition;
    bool m_isDragging;
};

// New PlaylistPreviewItem class that replaces PlaylistItemWidget
class PlaylistPreviewItem : public QWidget
{
    Q_OBJECT

public:
    explicit PlaylistPreviewItem(const PlaylistItem& item, int index, WallpaperManager* wallpaperManager, QWidget* parent = nullptr);
    
    const PlaylistItem& playlistItem() const { return m_item; }
    int getIndex() const { return m_index; }
    void setIndex(int index);
    void setCurrent(bool current);
    bool isCurrent() const { return m_isCurrent; }
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    
    // Get wallpaper info for display
    WallpaperInfo getWallpaperInfo() const { return m_wallpaperInfo; }
    
    // Layout constants (same as WallpaperPreviewItem)
    static constexpr int ITEM_WIDTH = 280;
    static constexpr int ITEM_HEIGHT = 240;
    static constexpr int PREVIEW_WIDTH = 256;
    static constexpr int PREVIEW_HEIGHT = 144;
    static constexpr int PREVIEW_CONTAINER_MARGIN = 12;
    static constexpr int TEXT_AREA_HEIGHT = 80;
    static constexpr int TEXT_LINE_HEIGHT = 16;
    static constexpr int TEXT_MARGIN = 8;

signals:
    void clicked(const QString& wallpaperId);
    void removeRequested(const QString& wallpaperId);
    void moveUpRequested(int index);
    void moveDownRequested(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onRemoveClicked();
    void onMoveUpClicked();
    void onMoveDownClicked();

private:
    void setupUI();
    void loadPreviewImage();
    void setPreviewPixmap(const QPixmap& pixmap);
    QPixmap scalePreviewKeepAspectRatio(const QPixmap& original);
    QSize calculateFitSize(const QSize& imageSize, const QSize& containerSize);
    void drawTextWithWordWrap(QPainter& painter, const QString& text, const QRect& rect, 
                             const QFont& font, const QColor& color, Qt::Alignment alignment = Qt::AlignLeft);
    void updateTextLayout();
    void loadAnimatedPreview();
    bool hasAnimatedPreview() const;
    void startAnimation();
    void stopAnimation();

    PlaylistItem m_item;
    int m_index;
    bool m_isCurrent;
    bool m_selected;
    WallpaperManager* m_wallpaperManager;
    WallpaperInfo m_wallpaperInfo;
    
    // Preview display
    QPixmap m_scaledPreview;
    QMovie* m_previewMovie;
    bool m_useCustomPainting;
    
    // Control buttons (positioned over the preview)
    QPushButton* m_removeButton;
    QPushButton* m_moveUpButton;
    QPushButton* m_moveDownButton;
    QLabel* m_positionLabel;
    
    // Drag support
    QPoint m_dragStartPosition;
};

#endif // PLAYLISTPREVIEW_H
