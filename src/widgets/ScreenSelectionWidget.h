#ifndef SCREENSELECTIONWIDGET_H
#define SCREENSELECTIONWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMap>
#include <QString>
#include <QPainter>
#include <QMouseEvent>
#include <QPixmap>

// Widget for displaying a single screen pictogram
class ScreenPictogram : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenPictogram(int screenNumber, const QString& screenName, 
                             const QString& technicalName, QSize resolution, QWidget* parent = nullptr);
    
    void setWallpaper(const QString& wallpaperId, const QString& wallpaperName, const QPixmap& preview);
    void clearWallpaper();
    bool hasWallpaper() const { return m_hasWallpaper; }
    QString getWallpaperId() const { return m_wallpaperId; }
    QString getTechnicalName() const { return m_technicalName; }
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }

signals:
    void clicked(const QString& technicalName);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    int m_screenNumber;
    QString m_screenName;
    QString m_technicalName;
    QSize m_resolution;
    bool m_hasWallpaper;
    bool m_selected;
    QString m_wallpaperId;
    QString m_wallpaperName;
    QPixmap m_wallpaperPreview;
};

// Main screen selection widget
class ScreenSelectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenSelectionWidget(QWidget* parent = nullptr);
    
    void updateScreens();
    void setScreenWallpaper(const QString& technicalName, const QString& wallpaperId, 
                           const QString& wallpaperName, const QPixmap& preview);
    void clearScreenWallpaper(const QString& technicalName);
    void clearAllScreenWallpapers();
    QString getSelectedScreen() const;
    QMap<QString, QString> getScreenAssignments() const;
    int getScreenCount() const { return m_screenPictograms.size(); }
    bool areAllScreensAssigned() const;

signals:
    void screenSelected(const QString& technicalName);

private slots:
    void onScreenClicked(const QString& technicalName);

private:
    QHBoxLayout* m_layout;
    QMap<QString, ScreenPictogram*> m_screenPictograms;  // Technical name -> pictogram
    QString m_selectedScreen;
};

#endif // SCREENSELECTIONWIDGET_H
