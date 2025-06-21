#ifndef WALLPAPERPREVIEW_H
#define WALLPAPERPREVIEW_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QGridLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QLoggingCategory>
#include <QString>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QFontMetrics>
#include <QMovie>
#include <QTextOption>
#include <QStyleOption>
#include <QPainter>
#include <QDrag>
#include <QMimeData>
#include "../core/WallpaperManager.h"

Q_DECLARE_LOGGING_CATEGORY(wallpaperPreview)

// Forward declaration to ensure WallpaperInfo has equality operator
inline bool operator==(const WallpaperInfo& lhs, const WallpaperInfo& rhs) {
    return lhs.id == rhs.id;
}

class WallpaperPreviewItem : public QWidget
{
    Q_OBJECT

public:
    explicit WallpaperPreviewItem(const WallpaperInfo& wallpaper, QWidget* parent = nullptr);
    
    const WallpaperInfo& wallpaperInfo() const { return m_wallpaper; }
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    void updateStyle();
    void loadWorkshopDataNow();
    bool isWorkshopDataLoaded() const { return m_workshopDataLoaded; }
    
    // Add method to cancel any pending operations
    void cancelPendingOperations() { m_workshopDataCancelled = true; }
    bool isCancelled() const { return m_workshopDataCancelled; }
    
    // Animation methods - made public
    void startAnimation();
    void stopAnimation();
    bool isAnimationPlaying() const;
    bool hasAnimatedPreview() const;
    
    // Layout constants with better sizing for text handling
    static constexpr int ITEM_WIDTH = 280;
    static constexpr int ITEM_HEIGHT = 240; // Increased to accommodate text properly
    static constexpr int PREVIEW_WIDTH = 256;
    static constexpr int PREVIEW_HEIGHT = 144;
    static constexpr int PREVIEW_CONTAINER_MARGIN = 12;
    static constexpr int TEXT_AREA_HEIGHT = 80; // Reserved space for text
    static constexpr int TEXT_LINE_HEIGHT = 16;
    static constexpr int TEXT_MARGIN = 8;
    static constexpr int TEXT_MAX_WIDTH = PREVIEW_WIDTH - (TEXT_MARGIN * 2);

signals:
    void clicked(const WallpaperInfo& wallpaper);
    void doubleClicked(const WallpaperInfo& wallpaper);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUI();
    void loadPreviewImage();
    void loadWorkshopData();
    void fetchWorkshopInfoHTTP(const QString& workshopId);
    void parseWorkshopDataFromJson(const QJsonObject& response, const QString& workshopId);
    void fetchSteamUserName(const QString& steamId);
    QString getUsernameFromLocalSteamData(const QString& steamId);  // Add this method
    QString extractUsernameFromVdf(const QString& vdfPath, const QString& steamId);  // Add this method
    QString extractUsernameFromLoginUsers(const QString& loginUsersPath, const QString& steamId);  // Add this method
    QString cleanBBCode(const QString& text);
    void tryAlternativeWorkshopMethods(const QString& workshopId);
    QString extractWorkshopId();
    QString extractWorkshopIdFromPath(const QString& wallpaperPath);
    QString extractWorkshopIdFromDirectory();
    void setFallbackValues();
    void setupTextLabels();
    QString elidedText(const QString& text, int maxWidth);
    void setPreviewPixmap(const QPixmap& pixmap);
    QPixmap scalePreviewKeepAspectRatio(const QPixmap& original);
    QSize calculateFitSize(const QSize& imageSize, const QSize& containerSize);
    void updateTextLayout();
    void drawTextWithWordWrap(QPainter& painter, const QString& text, const QRect& rect, 
                             const QFont& font, const QColor& color, Qt::Alignment alignment = Qt::AlignLeft);
    void loadAnimatedPreview();
    
    // Enhanced workshop data methods - updated for Steam API
    void parseWorkshopDataFromFilesystem();
    void tryLoadFromSteamCache();
    QString findWorkshopIdInSteamLibraries();
    
public slots:
    void loadWorkshopDataDeferred();
    
private:
    WallpaperInfo m_wallpaper;
    QLabel* m_previewLabel;
    QLabel* m_nameLabel;
    QLabel* m_typeLabel;
    QLabel* m_authorLabel;
    bool m_selected;
    bool m_workshopDataLoaded;
    static QNetworkAccessManager* s_networkManager;
    QMovie* m_previewMovie;
    QPixmap m_scaledPreview;
    bool m_useCustomPainting; // Flag to use custom text rendering

    // Add cancellation flag
    bool m_cancelled;
    bool m_workshopDataCancelled;
    
    // Drag and drop support
    QPoint m_dragStartPosition;
};

class WallpaperPreview : public QWidget
{
    Q_OBJECT

public:
    explicit WallpaperPreview(QWidget* parent = nullptr);
    
    void setWallpaperManager(WallpaperManager* manager);
    void refreshWallpapers();
    void selectWallpaper(const QString& wallpaperId);
    void updateTheme();
    
    WallpaperInfo getSelectedWallpaper() const;
    QString getSelectedWallpaperId() const;

    // Dynamic layout constants
    static constexpr int ITEMS_PER_PAGE = 20;
    static constexpr int MIN_ITEMS_PER_ROW = 1;
    static constexpr int MAX_ITEMS_PER_ROW = 8;
    static constexpr int PREFERRED_ITEMS_PER_ROW = 4;
    static constexpr int ITEMS_PER_ROW = PREFERRED_ITEMS_PER_ROW; // Backward compatibility
    static constexpr int ITEM_SPACING = 16;
    static constexpr int MIN_CONTAINER_WIDTH = 400; // Minimum width before reducing columns
    static constexpr int SIDEBAR_WIDTH = 300; // Estimated sidebar width
    
    // Workshop batch processing constants
    static constexpr int WORKSHOP_BATCH_SIZE = 3; // Reduced for better performance
    static constexpr int WORKSHOP_BATCH_DELAY = 200; // Increased delay

signals:
    void wallpaperSelected(const WallpaperInfo& wallpaper);
    void wallpaperDoubleClicked(const WallpaperInfo& wallpaper);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onWallpapersChanged();
    void onSearchTextChanged(const QString& text);
    void onFilterChanged();
    void onRefreshClicked();
    void onApplyClicked();
    void onWallpaperItemClicked(const WallpaperInfo& wallpaper);
    void onWallpaperItemDoubleClicked(const WallpaperInfo& wallpaper);
    void onPreviousPage();
    void onNextPage();
    void onPageChanged();
    void loadWorkshopDataBatch();

private:
    void setupUI();
    void updateWallpaperGrid();
    void setupPagination();
    void updatePageInfo();
    void loadCurrentPage();
    void clearCurrentPage();
    QList<WallpaperInfo> getFilteredWallpapers() const;
    void clearSelection();
    void startWallpaperDataLoading();
    void processNextWorkshopBatch();
    
    // Enhanced responsive layout methods
    int calculateItemsPerRow() const;
    int getCurrentItemsPerRow() const { return m_currentItemsPerRow; }
    void updateGridLayout();
    void recalculateLayout();
    void adjustGridForNewColumnCount(int newColumnCount);
    
    // Workshop handling improvements
    ::QString getWorkshopDirectory() const;
    QStringList getWorkshopIds() const;
    ::QString findWallpaperIdForWallpaper(const ::QString& wallpaperPath);
    bool isAnimatedWallpaper(const WallpaperInfo& wallpaper);
    void handleAnimatedPreview(WallpaperPreviewItem* item);
    ::QString getWorkshopIdFromFilesystem(const ::QString& wallpaperPath);
    QStringList scanWorkshopDirectory();

    WallpaperManager* m_wallpaperManager;
    
    // UI components
    QLineEdit* m_searchEdit;
    QComboBox* m_filterCombo;
    QPushButton* m_refreshButton;
    QPushButton* m_applyButton;
    QScrollArea* m_scrollArea;
    QWidget* m_gridWidget;
    QGridLayout* m_gridLayout;
    
    // Pagination UI
    QWidget* m_paginationWidget;
    QPushButton* m_prevPageButton;
    QPushButton* m_nextPageButton;
    QLabel* m_pageInfoLabel;
    
    // Pagination state
    QList<WallpaperInfo> m_filteredWallpapers;
    QList<WallpaperPreviewItem*> m_currentPageItems;
    WallpaperPreviewItem* m_selectedItem;
    int m_currentPage;
    int m_totalPages;
    
    // Wallpaper data loading
    QTimer* m_workshopLoadTimer;
    QList<WallpaperPreviewItem*> m_pendingWallpaperItems;
    int m_pendingItemIndex;
    QList<WallpaperPreviewItem*> m_pendingWorkshopItems;
    int m_workshopBatchIndex;
    
    // Responsive layout
    int m_currentItemsPerRow;
    int m_lastContainerWidth;
    bool m_layoutUpdatePending;

    // Add helper method to safely cancel all pending operations
    void cancelAllPendingOperations();

    // add missing calculateLayout alias
    void calculateLayout();
    
    // Scroll to make an item visible in the scroll area
    void scrollToItem(WallpaperPreviewItem* item);
    
    // Animation management methods
    void startCurrentPageAnimations();
    void stopCurrentPageAnimations();
};

#endif // WALLPAPERPREVIEW_H