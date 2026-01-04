#include "ScreenSelectionWidget.h"
#include "../core/ConfigManager.h"
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QLabel>
#include <QVBoxLayout>
#include <QFrame>

// ScreenPictogram implementation
ScreenPictogram::ScreenPictogram(int screenNumber, const QString& screenName, 
                                 const QString& technicalName, QSize resolution, QWidget* parent)
    : QWidget(parent)
    , m_screenNumber(screenNumber)
    , m_screenName(screenName)
    , m_technicalName(technicalName)
    , m_resolution(resolution)
    , m_hasWallpaper(false)
    , m_selected(false)
{
    setMinimumSize(200, 150);
    setMaximumSize(250, 180);
    setCursor(Qt::PointingHandCursor);
    setAutoFillBackground(true);
    
    // Set tooltip
    setToolTip(QString("Screen %1: %2\n%3x%4")
        .arg(screenNumber)
        .arg(screenName)
        .arg(resolution.width())
        .arg(resolution.height()));
}

void ScreenPictogram::setWallpaper(const QString& wallpaperId, const QString& wallpaperName, const QPixmap& preview)
{
    m_hasWallpaper = true;
    m_wallpaperId = wallpaperId;
    m_wallpaperName = wallpaperName;
    m_wallpaperPreview = preview;
    update();
}

void ScreenPictogram::clearWallpaper()
{
    m_hasWallpaper = false;
    m_wallpaperId.clear();
    m_wallpaperName.clear();
    m_wallpaperPreview = QPixmap();
    update();
}

void ScreenPictogram::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void ScreenPictogram::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Calculate aspect ratio rectangle
    float aspectRatio = (float)m_resolution.width() / m_resolution.height();
    int rectWidth = width() - 40;
    int rectHeight = rectWidth / aspectRatio;
    
    if (rectHeight > height() - 60) {
        rectHeight = height() - 60;
        rectWidth = rectHeight * aspectRatio;
    }
    
    int rectX = (width() - rectWidth) / 2;
    int rectY = 30;
    
    QRect screenRect(rectX, rectY, rectWidth, rectHeight);
    
    // Draw selection highlight
    if (m_selected) {
        painter.setPen(QPen(QColor(52, 152, 219), 3));
        painter.setBrush(QColor(52, 152, 219, 30));
        painter.drawRoundedRect(screenRect.adjusted(-3, -3, 3, 3), 8, 8);
    }
    
    // Draw screen border
    if (m_hasWallpaper) {
        painter.setPen(QPen(QColor(46, 204, 113), 2));
        painter.setBrush(QColor(46, 204, 113, 20));
    } else {
        painter.setPen(QPen(QColor(127, 140, 141), 2));
        painter.setBrush(QColor(236, 240, 241));
    }
    painter.drawRoundedRect(screenRect, 5, 5);
    
    // Draw wallpaper preview if assigned
    if (m_hasWallpaper && !m_wallpaperPreview.isNull()) {
        QRect previewRect = screenRect.adjusted(4, 4, -4, -4);
        QPixmap scaledPreview = m_wallpaperPreview.scaled(previewRect.size(), 
            Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        
        // Crop to fit
        int offsetX = (scaledPreview.width() - previewRect.width()) / 2;
        int offsetY = (scaledPreview.height() - previewRect.height()) / 2;
        QPixmap croppedPreview = scaledPreview.copy(offsetX, offsetY, 
            previewRect.width(), previewRect.height());
        
        painter.drawPixmap(previewRect, croppedPreview);
        
        // Draw wallpaper name overlay
        QRect nameRect = screenRect.adjusted(0, screenRect.height() - 25, 0, 0);
        painter.fillRect(nameRect, QColor(0, 0, 0, 180));
        painter.setPen(Qt::white);
        painter.setFont(QFont("Sans", 8, QFont::Bold));
        
        QString displayName = m_wallpaperName;
        QFontMetrics fm(painter.font());
        if (fm.horizontalAdvance(displayName) > nameRect.width() - 10) {
            displayName = fm.elidedText(displayName, Qt::ElideRight, nameRect.width() - 10);
        }
        painter.drawText(nameRect, Qt::AlignCenter, displayName);
    } else {
        // Draw "No Wallpaper" text
        painter.setPen(QColor(149, 165, 166));
        painter.setFont(QFont("Sans", 9));
        painter.drawText(screenRect, Qt::AlignCenter, "Not Assigned");
    }
    
    // Draw screen label at top
    painter.setPen(Qt::black);
    painter.setFont(QFont("Sans", 10, QFont::Bold));
    QString label = QString("Screen %1").arg(m_screenNumber);
    painter.drawText(QRect(0, 5, width(), 20), Qt::AlignCenter, label);
    
    // Draw screen name at bottom
    painter.setFont(QFont("Sans", 8));
    painter.setPen(QColor(127, 140, 141));
    QString name = m_screenName;
    QFontMetrics fm(painter.font());
    if (fm.horizontalAdvance(name) > width() - 10) {
        name = fm.elidedText(name, Qt::ElideMiddle, width() - 10);
    }
    painter.drawText(QRect(0, height() - 20, width(), 20), Qt::AlignCenter, name);
}

void ScreenPictogram::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_technicalName);
    }
    QWidget::mousePressEvent(event);
}

// ScreenSelectionWidget implementation
ScreenSelectionWidget::ScreenSelectionWidget(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setSpacing(15);
    m_layout->setContentsMargins(10, 10, 10, 10);
    
    // Add stretch to center the screens
    m_layout->addStretch();
    m_layout->addStretch();
    
    setMinimumHeight(200);
    
    // Set background to match application theme
    setAutoFillBackground(true);
}

void ScreenSelectionWidget::updateScreens()
{
    // Clear existing pictograms
    for (auto* pictogram : m_screenPictograms) {
        m_layout->removeWidget(pictogram);
        pictogram->deleteLater();
    }
    m_screenPictograms.clear();
    m_selectedScreen.clear();
    
    ConfigManager& config = ConfigManager::instance();
    QStringList screenOrder = config.multiMonitorScreenOrder();
    QMap<QString, QString> customNames = config.multiMonitorScreenNames();
    
    if (screenOrder.isEmpty()) {
        // No saved order, detect screens
        QScreen* primaryScreen = qApp->primaryScreen();
        if (primaryScreen) {
            screenOrder << primaryScreen->name();
        }
        
        for (QScreen* screen : qApp->screens()) {
            if (screen != primaryScreen && !screenOrder.contains(screen->name())) {
                screenOrder << screen->name();
            }
        }
    }
    
    // Create pictograms
    int screenNumber = 1;
    for (const QString& technicalName : screenOrder) {
        // Find the screen
        QScreen* screen = nullptr;
        for (QScreen* s : qApp->screens()) {
            if (s->name() == technicalName) {
                screen = s;
                break;
            }
        }
        
        if (!screen) continue;  // Screen not currently connected
        
        QString displayName = customNames.value(technicalName, technicalName);
        QSize resolution = screen->size();
        
        ScreenPictogram* pictogram = new ScreenPictogram(screenNumber++, displayName, 
                                                         technicalName, resolution, this);
        connect(pictogram, &ScreenPictogram::clicked, this, &ScreenSelectionWidget::onScreenClicked);
        
        // Insert before the last stretch
        m_layout->insertWidget(m_layout->count() - 1, pictogram);
        m_screenPictograms[technicalName] = pictogram;
    }
    
    // Select first screen by default if any exist
    if (!m_screenPictograms.isEmpty()) {
        QString firstScreen = screenOrder.first();
        onScreenClicked(firstScreen);
    }
}

void ScreenSelectionWidget::setScreenWallpaper(const QString& technicalName, const QString& wallpaperId,
                                               const QString& wallpaperName, const QPixmap& preview)
{
    if (m_screenPictograms.contains(technicalName)) {
        m_screenPictograms[technicalName]->setWallpaper(wallpaperId, wallpaperName, preview);
    }
}

void ScreenSelectionWidget::clearScreenWallpaper(const QString& technicalName)
{
    if (m_screenPictograms.contains(technicalName)) {
        m_screenPictograms[technicalName]->clearWallpaper();
    }
}

void ScreenSelectionWidget::clearAllScreenWallpapers()
{
    for (auto* pictogram : m_screenPictograms) {
        pictogram->clearWallpaper();
    }
}

QString ScreenSelectionWidget::getSelectedScreen() const
{
    return m_selectedScreen;
}

QMap<QString, QString> ScreenSelectionWidget::getScreenAssignments() const
{
    QMap<QString, QString> assignments;
    for (auto it = m_screenPictograms.constBegin(); it != m_screenPictograms.constEnd(); ++it) {
        if (it.value()->hasWallpaper()) {
            assignments[it.key()] = it.value()->getWallpaperId();
        }
    }
    return assignments;
}

bool ScreenSelectionWidget::areAllScreensAssigned() const
{
    for (auto* pictogram : m_screenPictograms) {
        if (!pictogram->hasWallpaper()) {
            return false;
        }
    }
    return !m_screenPictograms.isEmpty();
}

void ScreenSelectionWidget::onScreenClicked(const QString& technicalName)
{
    // Deselect all
    for (auto* pictogram : m_screenPictograms) {
        pictogram->setSelected(false);
    }
    
    // Select clicked screen
    if (m_screenPictograms.contains(technicalName)) {
        m_screenPictograms[technicalName]->setSelected(true);
        m_selectedScreen = technicalName;
        emit screenSelected(technicalName);
    }
}
