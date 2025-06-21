#include "WallpaperPreview.h"
#include "../core/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QImageReader>
#include <QPixmap>
#include <QPainter>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QTextStream>
#include <QDateTime>
#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QCursor>

Q_LOGGING_CATEGORY(wallpaperPreview, "app.wallpaperPreview")

// Static member initialization
QNetworkAccessManager* WallpaperPreviewItem::s_networkManager = nullptr;

// WallpaperPreviewItem implementation
WallpaperPreviewItem::WallpaperPreviewItem(const WallpaperInfo& wallpaper, QWidget* parent)
    : QWidget(parent)
    , m_wallpaper(wallpaper)
    , m_previewLabel(nullptr)
    , m_nameLabel(nullptr)
    , m_typeLabel(nullptr)
    , m_authorLabel(nullptr)
    , m_selected(false)
    , m_workshopDataLoaded(false)
    , m_previewMovie(nullptr)
    , m_useCustomPainting(true)
    , m_cancelled(false)  // Initialize cancellation flag for animations
    , m_workshopDataCancelled(false)  // Initialize cancellation flag for workshop data
    , m_dragStartPosition()
{
    setFixedSize(ITEM_WIDTH, ITEM_HEIGHT + 20);
    setupUI();
    loadPreviewImage();
    
    // Use QRandomGenerator instead of deprecated qrand()
    int randomDelay = 100 + (QRandomGenerator::global()->bounded(500));
    QTimer* deferTimer = new QTimer(this);
    deferTimer->setSingleShot(true);
    connect(deferTimer, &QTimer::timeout, this, [this, deferTimer]() {
        loadWorkshopDataDeferred();
        deferTimer->deleteLater();
    });
    deferTimer->start(randomDelay);
}

void WallpaperPreviewItem::setupUI()
{
    setContentsMargins(0, 0, 0, 0);
    
    if (m_useCustomPainting) {
        return;
    }
    
    // This would set up traditional widget-based UI if needed
}

void WallpaperPreviewItem::loadPreviewImage()
{
    if (!m_wallpaper.previewPath.isEmpty() && QFileInfo::exists(m_wallpaper.previewPath)) {
        // Check if it's an animated preview first
        if (hasAnimatedPreview()) {
            loadAnimatedPreview();
            return;
        }
        
        // Load static image
        QPixmap pixmap(m_wallpaper.previewPath);
        if (!pixmap.isNull()) {
            setPreviewPixmap(pixmap);
        }
    }
}

void WallpaperPreviewItem::setPreviewPixmap(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        return;
    }
    
    m_scaledPreview = scalePreviewKeepAspectRatio(pixmap);
    update();
}

QPixmap WallpaperPreviewItem::scalePreviewKeepAspectRatio(const QPixmap& original)
{
    QSize containerSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
    QSize scaledSize = calculateFitSize(original.size(), containerSize);
    
    return original.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void WallpaperPreviewItem::loadAnimatedPreview()
{
    if (m_cancelled || !hasAnimatedPreview()) {
        return;
    }
    
    // Clean up existing movie if any
    if (m_previewMovie) {
        m_previewMovie->stop();
        m_previewMovie->deleteLater();
        m_previewMovie = nullptr;
    }
    
    // Create and configure QMovie for animation
    m_previewMovie = new QMovie(m_wallpaper.previewPath, QByteArray(), this);
    
    if (!m_previewMovie->isValid()) {
        qCWarning(wallpaperPreview) << "Invalid animated preview file:" << m_wallpaper.previewPath;
        m_previewMovie->deleteLater();
        m_previewMovie = nullptr;
        // Fall back to static image - load directly to avoid recursion
        QPixmap staticPixmap(m_wallpaper.previewPath);
        if (!staticPixmap.isNull()) {
            setPreviewPixmap(staticPixmap);
        }
        return;
    }
    
    // Connect to movie signals for frame updates
    connect(m_previewMovie, &QMovie::frameChanged, this, [this](int frameNumber) {
        Q_UNUSED(frameNumber)
        if (m_previewMovie && !m_cancelled) {
            QPixmap currentFrame = m_previewMovie->currentPixmap();
            if (!currentFrame.isNull()) {
                m_scaledPreview = scalePreviewKeepAspectRatio(currentFrame);
                update();
                // Debug output for first few frames to avoid spam
                if (frameNumber < 5) {
                    qCDebug(wallpaperPreview) << "Frame" << frameNumber << "updated for:" << m_wallpaper.name;
                }
            }
        }
    });
    
    connect(m_previewMovie, &QMovie::error, this, [this](QImageReader::ImageReaderError error) {
        qCWarning(wallpaperPreview) << "Movie error:" << error << "for file:" << m_wallpaper.previewPath;
        // Fall back to static preview - load directly to avoid recursion
        QPixmap staticPixmap(m_wallpaper.previewPath);
        if (!staticPixmap.isNull()) {
            setPreviewPixmap(staticPixmap);
        }
    });
    
    // Get the first frame by jumping to frame 0
    m_previewMovie->jumpToFrame(0);
    QPixmap firstFrame = m_previewMovie->currentPixmap();
    if (!firstFrame.isNull()) {
        m_scaledPreview = scalePreviewKeepAspectRatio(firstFrame);
        update(); // Ensure the UI shows the first frame immediately
    } else {
        // If we can't get the first frame, try loading as static image fallback
        QPixmap staticPixmap(m_wallpaper.previewPath);
        if (!staticPixmap.isNull()) {
            m_scaledPreview = scalePreviewKeepAspectRatio(staticPixmap);
            update();
        }
    }
    
    // Don't start playing immediately - wait for page visibility
    qCDebug(wallpaperPreview) << "Loaded animated preview for:" << m_wallpaper.name;
}

bool WallpaperPreviewItem::hasAnimatedPreview() const
{
    if (m_wallpaper.previewPath.isEmpty() || !QFileInfo::exists(m_wallpaper.previewPath)) {
        return false;
    }
    
    // Check if the preview is an animated format
    QString lowerPath = m_wallpaper.previewPath.toLower();
    bool isAnimated = lowerPath.endsWith(".gif") || lowerPath.endsWith(".webp");
    
    if (isAnimated) {
        qCDebug(wallpaperPreview) << "Detected animated preview:" << m_wallpaper.previewPath;
    }
    
    return isAnimated;
}

void WallpaperPreviewItem::startAnimation()
{
    if (m_previewMovie && m_previewMovie->isValid() && !m_cancelled) {
        qCDebug(wallpaperPreview) << "Starting animation for:" << m_wallpaper.name 
                                  << "Movie state:" << m_previewMovie->state()
                                  << "Frame count:" << m_previewMovie->frameCount();
        m_previewMovie->start();
        qCDebug(wallpaperPreview) << "Animation started, new state:" << m_previewMovie->state();
    } else {
        qCDebug(wallpaperPreview) << "Cannot start animation for:" << m_wallpaper.name 
                                  << "Movie valid:" << (m_previewMovie && m_previewMovie->isValid())
                                  << "Cancelled:" << m_cancelled;
    }
}

void WallpaperPreviewItem::stopAnimation()
{
    if (m_previewMovie && m_previewMovie->isValid()) {
        qCDebug(wallpaperPreview) << "Stopping animation for:" << m_wallpaper.name;
        m_previewMovie->stop();
    }
}

bool WallpaperPreviewItem::isAnimationPlaying() const
{
    return m_previewMovie && m_previewMovie->state() == QMovie::Running;
}

QSize WallpaperPreviewItem::calculateFitSize(const QSize& imageSize, const QSize& containerSize)
{
    if (imageSize.isEmpty() || containerSize.isEmpty()) {
        return containerSize;
    }
    
    QSize result = imageSize.scaled(containerSize, Qt::KeepAspectRatio);
    return result;
}

void WallpaperPreviewItem::loadWorkshopDataDeferred()
{
    if (m_workshopDataLoaded || m_workshopDataCancelled) {
        return;
    }
    
    loadWorkshopData();
}

void WallpaperPreviewItem::loadWorkshopData()
{
    if (m_workshopDataLoaded || m_workshopDataCancelled) {
        return;
    }
    
    QString workshopId = extractWorkshopId();
    if (!workshopId.isEmpty()) {
        fetchWorkshopInfoHTTP(workshopId);
    } else {
        setFallbackValues();
    }
}

QString WallpaperPreviewItem::extractWorkshopId()
{
    QString workshopId = extractWorkshopIdFromPath(m_wallpaper.path);
    if (workshopId.isEmpty()) {
        workshopId = extractWorkshopIdFromDirectory();
    }
    return workshopId;
}

QString WallpaperPreviewItem::extractWorkshopIdFromPath(const QString& wallpaperPath)
{
    QRegularExpression workshopRegex(R"(/workshop/content/431960/(\d+))");
    QRegularExpressionMatch match = workshopRegex.match(wallpaperPath);
    
    if (match.hasMatch()) {
        return match.captured(1);
    }
    
    return QString();
}

QString WallpaperPreviewItem::extractWorkshopIdFromDirectory()
{
    QFileInfo pathInfo(m_wallpaper.path);
    QString dirName = pathInfo.fileName();
    
    bool ok;
    dirName.toULongLong(&ok);
    if (ok) {
        return dirName;
    }
    
    return QString();
}

void WallpaperPreviewItem::fetchWorkshopInfoHTTP(const QString& workshopId)
{
    if (!s_networkManager) {
        s_networkManager = new QNetworkAccessManager();
    }
    
    QString url = QString("https://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/");
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::UserAgentHeader, "WallpaperEngineGUI/1.0");
    
    QByteArray postData;
    postData.append("itemcount=1");
    postData.append("&publishedfileids[0]=");
    postData.append(workshopId.toUtf8());
    
    QNetworkReply* reply = s_networkManager->post(request, postData);
    
    connect(reply, &QNetworkReply::finished, [this, reply, workshopId]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(responseData, &error);
            
            if (error.error == QJsonParseError::NoError) {
                parseWorkshopDataFromJson(doc.object(), workshopId);
            } else {
                qCWarning(wallpaperPreview) << "Failed to parse Steam API response:" << error.errorString();
                setFallbackValues();
            }
        } else {
            qCWarning(wallpaperPreview) << "Steam API request failed:" << reply->errorString();
            tryAlternativeWorkshopMethods(workshopId);
        }
        
        reply->deleteLater();
    });
}

void WallpaperPreviewItem::parseWorkshopDataFromJson(const QJsonObject& response, const QString& workshopId)
{
    QJsonObject responseObj = response.value("response").toObject();
    QJsonArray publishedFileDetails = responseObj.value("publishedfiledetails").toArray();
    
    if (publishedFileDetails.isEmpty()) {
        qCWarning(wallpaperPreview) << "No published file details found for workshop ID:" << workshopId;
        setFallbackValues();
        return;
    }
    
    QJsonObject fileDetails = publishedFileDetails.first().toObject();
    
    QString title = fileDetails.value("title").toString();
    if (!title.isEmpty()) {
        m_wallpaper.name = title;
        qCDebug(wallpaperPreview) << "Updated wallpaper name from Steam API:" << title;
    }
    
    QString description = fileDetails.value("description").toString();
    if (!description.isEmpty()) {
        description = cleanBBCode(description);
        m_wallpaper.description = description;
        qCDebug(wallpaperPreview) << "Found description from Steam API:" << description.left(50) << "...";
    }
    
    qint64 fileSize = fileDetails.value("file_size").toVariant().toLongLong();
    if (fileSize > 0) {
        m_wallpaper.fileSize = fileSize;
        qCDebug(wallpaperPreview) << "Found file size from Steam API:" << fileSize << "bytes";
    }
    
    qint64 timeCreated = fileDetails.value("time_created").toVariant().toLongLong();
    qint64 timeUpdated = fileDetails.value("time_updated").toVariant().toLongLong();
    
    if (timeCreated > 0) {
        QDateTime created = QDateTime::fromSecsSinceEpoch(timeCreated);
        qCDebug(wallpaperPreview) << "Created:" << created.toString("dd MMM, yyyy @ h:mmap");
    }
    
    if (timeUpdated > 0) {
        QDateTime updated = QDateTime::fromSecsSinceEpoch(timeUpdated);
        qCDebug(wallpaperPreview) << "Updated:" << updated.toString("dd MMM, yyyy @ h:mmap");
    }
    
    QJsonArray tagsArray = fileDetails.value("tags").toArray();
    QStringList tags;
    QString type, genre, ageRating;
    
    for (const QJsonValue& tagValue : tagsArray) {
        QJsonObject tagObj = tagValue.toObject();
        QString tag = tagObj.value("tag").toString();
        if (!tag.isEmpty()) {
            tags.append(tag);
            
            if (tag == "Scene" || tag == "Video" || tag == "Web") {
                type = tag;
            } else if (tag == "Pixel art" || tag == "Abstract" || tag == "Anime" || tag == "Nature") {
                genre = tag;
            } else if (tag == "Everyone" || tag == "Mature") {
                ageRating = tag;
            }
        }
    }
    
    if (!tags.isEmpty()) {
        m_wallpaper.tags = tags;
        qCDebug(wallpaperPreview) << "Found tags from Steam API:" << tags;
    }
    
    if (!type.isEmpty()) {
        m_wallpaper.type = type;
        qCDebug(wallpaperPreview) << "Updated type from tags:" << type;
    }
    
    int visibility = fileDetails.value("visibility").toInt();
    QString visibilityStr = (visibility == 0) ? "Public" : "Private";
    qCDebug(wallpaperPreview) << "Visibility:" << visibilityStr;
    
    QString creatorSteamId = fileDetails.value("creator").toString();
    if (!creatorSteamId.isEmpty()) {
        QString localUsername = getUsernameFromLocalSteamData(creatorSteamId);
        if (!localUsername.isEmpty()) {
            m_wallpaper.author = localUsername;
            qCDebug(wallpaperPreview) << "Found username from local Steam data:" << localUsername;
        } else {
            m_wallpaper.author = QString("Steam User %1").arg(creatorSteamId.right(8));
            qCDebug(wallpaperPreview) << "Using Steam ID fallback:" << m_wallpaper.author;
        }
    }
    
    m_workshopDataLoaded = true;
    update();
}

QString WallpaperPreviewItem::getUsernameFromLocalSteamData(const QString& steamId)
{
    QStringList steamCachePaths = {
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.steam/steam",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/share/Steam",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.var/app/com.valvesoftware.Steam/.local/share/Steam"
    };
    
    for (const QString& steamPath : steamCachePaths) {
        QString userDataPath = QDir(steamPath).filePath("userdata");
        QDir userDataDir(userDataPath);
        
        if (userDataDir.exists()) {
            QStringList userDirs = userDataDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& userDir : userDirs) {
                QString configPath = userDataDir.filePath(userDir + "/config/localconfig.vdf");
                if (QFileInfo::exists(configPath)) {
                    QString username = extractUsernameFromVdf(configPath, steamId);
                    if (!username.isEmpty()) {
                        return username;
                    }
                }
            }
        }
        
        QString loginUsersPath = QDir(steamPath).filePath("config/loginusers.vdf");
        if (QFileInfo::exists(loginUsersPath)) {
            QString username = extractUsernameFromLoginUsers(loginUsersPath, steamId);
            if (!username.isEmpty()) {
                return username;
            }
        }
    }
    
    return QString();
}

QString WallpaperPreviewItem::extractUsernameFromVdf(const QString& vdfPath, const QString& steamId)
{
    QFile file(vdfPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    
    QRegularExpression steamIdRegex(QString("\"76561[0-9]{12}\"\\s*\\{[^}]*\"PersonaName\"\\s*\"([^\"]+)\""));
    QRegularExpressionMatchIterator matches = steamIdRegex.globalMatch(content);
    
    while (matches.hasNext()) {
        QRegularExpressionMatch match = matches.next();
        if (match.captured(0).contains(steamId)) {
            return match.captured(1);
        }
    }
    
    return QString();
}

QString WallpaperPreviewItem::extractUsernameFromLoginUsers(const QString& loginUsersPath, const QString& steamId)
{
    QFile file(loginUsersPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    
    QRegularExpression userBlockRegex(QString("\"%1\"\\s*\\{([^}]+)\\}").arg(steamId));
    QRegularExpressionMatch userMatch = userBlockRegex.match(content);
    
    if (userMatch.hasMatch()) {
        QString userBlock = userMatch.captured(1);
        QRegularExpression nameRegex("\"PersonaName\"\\s*\"([^\"]+)\"");
        QRegularExpressionMatch nameMatch = nameRegex.match(userBlock);
        
        if (nameMatch.hasMatch()) {
            return nameMatch.captured(1);
        }
    }
    
    return QString();
}

void WallpaperPreviewItem::fetchSteamUserName(const QString& steamId)
{
    if (!s_networkManager) {
        s_networkManager = new QNetworkAccessManager();
    }
    
    QString url = QString("https://api.steampowered.com/ISteamUser/GetPlayerSummaries/v0002/?steamids=%1&format=json").arg(steamId);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "WallpaperEngineGUI/1.0");
    
    QNetworkReply* reply = s_networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, [this, reply, steamId]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(responseData, &error);
            
            if (error.error == QJsonParseError::NoError) {
                QJsonObject response = doc.object().value("response").toObject();
                QJsonArray players = response.value("players").toArray();
                
                if (!players.isEmpty()) {
                    QJsonObject player = players.first().toObject();
                    QString personaName = player.value("personaname").toString();
                    
                    if (!personaName.isEmpty()) {
                        m_wallpaper.author = personaName;
                        qCDebug(wallpaperPreview) << "Found Steam username:" << personaName << "for ID:" << steamId;
                        update();
                    }
                }
            }
        } else {
            m_wallpaper.author = QString("Steam User %1").arg(steamId.right(8));
            qCDebug(wallpaperPreview) << "Failed to get username for Steam ID:" << steamId << "using fallback";
            update();
        }
        
        reply->deleteLater();
    });
}

void WallpaperPreviewItem::paintEvent(QPaintEvent* event)
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
    
    QColor bgColor = m_selected ? palette().color(QPalette::Highlight) : palette().color(QPalette::Base);
    painter.fillRect(rect(), bgColor);
    
    if (m_selected) {
        painter.setPen(QPen(palette().color(QPalette::Highlight), 2));
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
    }
    
    int availableWidth = currentSize.width() - 2 * PREVIEW_CONTAINER_MARGIN;
    int availableHeight = currentSize.height() - 2 * PREVIEW_CONTAINER_MARGIN;
    int previewHeight = qMin(PREVIEW_HEIGHT, availableHeight - TEXT_AREA_HEIGHT);
    int previewWidth = qMin(PREVIEW_WIDTH, availableWidth);
    
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
    
    int textY = PREVIEW_CONTAINER_MARGIN + previewHeight + TEXT_MARGIN;
    int maxTextWidth = availableWidth - TEXT_MARGIN;
    int maxTextHeight = currentSize.height() - textY - TEXT_MARGIN;
    
    if (maxTextWidth > 0 && maxTextHeight > 0) {
        QRect textRect = QRect(PREVIEW_CONTAINER_MARGIN + TEXT_MARGIN/2, textY, maxTextWidth, maxTextHeight);
        
        painter.setPen(palette().color(QPalette::Text));
        
        QFont nameFont = font();
        nameFont.setBold(true);
        nameFont.setPointSize(qMax(8, font().pointSize()));
        
        QString displayName = m_wallpaper.name.isEmpty() ? "Unknown" : m_wallpaper.name;
        
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
            if (!m_wallpaper.author.isEmpty()) {
                infoLines << QString("By: %1").arg(m_wallpaper.author);
            }
            if (!m_wallpaper.type.isEmpty()) {
                infoLines << QString("Type: %1").arg(m_wallpaper.type);
            }
            
            QString infoText = infoLines.join(" • ");
            if (!infoText.isEmpty()) {
                painter.setPen(palette().color(QPalette::Mid));
                drawTextWithWordWrap(painter, infoText, infoRect, infoFont, palette().color(QPalette::Mid));
            }
        }
    }
}

void WallpaperPreviewItem::drawTextWithWordWrap(QPainter& painter, const QString& text, const QRect& rect, 
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
                currentLine.clear();
            }
        }
    }
    
    if (!currentLine.isEmpty()) {
        lines.append(currentLine);
    }
    
    int lineHeight = fm.height();
    int maxLines = qMax(1, rect.height() / lineHeight);
    
    if (lines.size() > maxLines) {
        lines = lines.mid(0, maxLines);
        if (!lines.isEmpty()) {
            QString& lastLine = lines.last();
            lastLine = fm.elidedText(lastLine, Qt::ElideRight, rect.width());
        }
    }
    
    int startY = rect.y();
    if (alignment & Qt::AlignVCenter) {
        int usedHeight = lines.size() * lineHeight;
        startY = rect.y() + (rect.height() - usedHeight) / 2;
    } else if (alignment & Qt::AlignBottom) {
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

void WallpaperPreviewItem::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = event->pos();
        qCDebug(wallpaperPreview) << "Mouse pressed at position:" << event->pos() << "for wallpaper:" << m_wallpaper.name;
        emit clicked(m_wallpaper);
    }
    QWidget::mousePressEvent(event);
}

void WallpaperPreviewItem::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked(m_wallpaper);
    }
    QWidget::mouseDoubleClickEvent(event);
}

void WallpaperPreviewItem::mouseMoveEvent(QMouseEvent* event)
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

    qCDebug(wallpaperPreview) << "Starting drag operation for wallpaper:" << m_wallpaper.name << "with ID:" << m_wallpaper.id;

    // Start drag operation
    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = new QMimeData;
    
    // Set the wallpaper ID as MIME data
    mimeData->setText(m_wallpaper.id);
    mimeData->setData("application/x-wallpaper-id", m_wallpaper.id.toUtf8());
    
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
    Qt::DropAction dropAction = drag->exec(Qt::CopyAction);
    qCDebug(wallpaperPreview) << "Drag completed with action:" << dropAction;
    
    QWidget::mouseMoveEvent(event);
}

void WallpaperPreviewItem::resizeEvent(QResizeEvent* event)
{
    if (!event) {
        return;
    }
    
    QWidget::resizeEvent(event);
    
    if (isVisible() && !m_wallpaper.id.isEmpty()) {
        updateTextLayout();
    }
}

void WallpaperPreviewItem::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        update();
    }
}

void WallpaperPreviewItem::updateStyle()
{
    update();
}

void WallpaperPreviewItem::setFallbackValues()
{
    if (m_wallpaper.author.isEmpty()) {
        m_wallpaper.author = "Unknown Author";
    }
    
    if (m_wallpaper.description.isEmpty()) {
        m_wallpaper.description = "No description available";
    }
    
    m_workshopDataLoaded = true;
    update();
}

void WallpaperPreviewItem::updateTextLayout()
{
    if (!isVisible() || size().isEmpty() || size().width() <= 0 || size().height() <= 0) {
        return;
    }
    
    // Use QTimer for deferred update
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

void WallpaperPreviewItem::loadWorkshopDataNow()
{
    if (m_workshopDataLoaded || m_workshopDataCancelled) {
        return;
    }
    
    qCDebug(wallpaperPreview) << "loadWorkshopDataNow called for wallpaper:" << m_wallpaper.name;
    
    loadWorkshopData();
}

QString WallpaperPreviewItem::cleanBBCode(const QString& text)
{
    QString cleaned = text;
    
    QStringList bbcodeTags = {
        "\\[b\\]", "\\[/b\\]",
        "\\[i\\]", "\\[/i\\]",
        "\\[u\\]", "\\[/u\\]",
        "\\[h1\\]", "\\[/h1\\]",
        "\\[quote\\]", "\\[/quote\\]",
        "\\[code\\]", "\\[/code\\]",
        "\\[list\\]", "\\[/list\\]",
        "\\[\\*\\]",
        "\\[hr\\]", "\\[/hr\\]",
        "\\[img\\]", "\\[/img\\]"
    };
    
    for (const QString& tag : bbcodeTags) {
        QRegularExpression regex(tag, QRegularExpression::CaseInsensitiveOption);
        cleaned = cleaned.replace(regex, "");
    }
    
    QRegularExpression urlRegex("\\[url=[^\\]]*\\]([^\\[]*)\\[/url\\]", QRegularExpression::CaseInsensitiveOption);
    cleaned = cleaned.replace(urlRegex, "\\1");
    
    QRegularExpression simpleUrlRegex("\\[url\\]([^\\[]*)\\[/url\\]", QRegularExpression::CaseInsensitiveOption);
    cleaned = cleaned.replace(simpleUrlRegex, "\\1");
    
    cleaned = cleaned.replace(QRegularExpression("\\s+"), " ");
    cleaned = cleaned.trimmed();
    
    return cleaned;
}

void WallpaperPreviewItem::tryAlternativeWorkshopMethods(const QString& workshopId)
{
    Q_UNUSED(workshopId);
    
    // Fix: Changed parseWallpaperDataFromFilesystem to parseWorkshopDataFromFilesystem
    parseWorkshopDataFromFilesystem();
    
    // If still no data, try loading from Steam cache
    if (!m_workshopDataLoaded) {
        tryLoadFromSteamCache();
    }
}

void WallpaperPreviewItem::parseWorkshopDataFromFilesystem()
{
    qCDebug(wallpaperPreview) << "parseWorkshopDataFromFilesystem called for wallpaper:" << m_wallpaper.name;
    
    QString workshopMetaPath = QDir(m_wallpaper.path).filePath(".workshop_metadata.json");
    
    if (QFileInfo::exists(workshopMetaPath)) {
        QFile file(workshopMetaPath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            
            if (error.error == QJsonParseError::NoError) {
                QJsonObject metadata = doc.object();
                
                if (m_wallpaper.author.isEmpty()) {
                    m_wallpaper.author = metadata.value("author").toString();
                    qCDebug(wallpaperPreview) << "Found author in filesystem metadata:" << m_wallpaper.author;
                }
                
                if (m_wallpaper.description.isEmpty()) {
                    m_wallpaper.description = metadata.value("description").toString();
                    qCDebug(wallpaperPreview) << "Found description in filesystem metadata";
                }
                
                if (m_wallpaper.tags.isEmpty()) {
                    QJsonArray tagsArray = metadata.value("tags").toArray();
                    for (const QJsonValue& tag : tagsArray) {
                        m_wallpaper.tags.append(tag.toString());
                    }
                }
            }
        }
    }
    
    QString projectPath = QDir(m_wallpaper.path).filePath("project.json");
    if (QFileInfo::exists(projectPath)) {
        QFile file(projectPath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            
            if (error.error == QJsonParseError::NoError) {
                QJsonObject project = doc.object();
                QJsonObject generalObj = project.value("general").toObject();
                
                QString workshopId = project.value("workshopid").toString();
                if (workshopId.isEmpty()) {
                    workshopId = generalObj.value("workshopid").toString();
                }
                
                if (!workshopId.isEmpty()) {
                    qCDebug(wallpaperPreview) << "Found workshop ID in project.json:" << workshopId;
                }
                
                if (m_wallpaper.author.isEmpty()) {
                    m_wallpaper.author = project.value("author").toString();
                    if (m_wallpaper.author.isEmpty()) {
                        m_wallpaper.author = generalObj.value("author").toString();
                    }
                }
                
                if (m_wallpaper.description.isEmpty()) {
                    m_wallpaper.description = project.value("description").toString();
                }
            }
        }
    }
}

void WallpaperPreviewItem::tryLoadFromSteamCache()
{
    qCDebug(wallpaperPreview) << "tryLoadFromSteamCache called for wallpaper:" << m_wallpaper.name;
    
    QString workshopId = extractWorkshopId();
    if (workshopId.isEmpty()) {
        qCDebug(wallpaperPreview) << "No workshop ID found, cannot load from Steam cache";
        return;
    }
    
    QStringList steamCachePaths = {
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.steam/steam/appcache/workshop",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/share/Steam/appcache/workshop",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.var/app/com.valvesoftware.Steam/.local/share/Steam/appcache/workshop"
    };
    
    for (const QString& cachePath : steamCachePaths) {
        QString metaFile = QDir(cachePath).filePath(QString("431960_%1.meta").arg(workshopId));
        
        if (QFileInfo::exists(metaFile)) {
            qCDebug(wallpaperPreview) << "Found Steam cache file:" << metaFile;
            
            QFile file(metaFile);
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray data = file.readAll();
                QString content = QString::fromUtf8(data);
                
                QRegularExpression authorRegex("\"creator\"\\s*\"([^\"]+)\"");
                QRegularExpressionMatch match = authorRegex.match(content);
                if (match.hasMatch() && m_wallpaper.author.isEmpty()) {
                    QString creator = match.captured(1);
                    if (creator.length() > 10 && creator.toULongLong() > 0) {
                        m_wallpaper.author = QString("Steam User %1").arg(creator.right(8));
                    } else {
                        m_wallpaper.author = creator;
                    }
                    qCDebug(wallpaperPreview) << "Found author in Steam cache:" << m_wallpaper.author;
                }
                
                QRegularExpression titleRegex("\"title\"\\s*\"([^\"]+)\"");
                match = titleRegex.match(content);
                if (match.hasMatch() && m_wallpaper.name.isEmpty()) {
                    m_wallpaper.name = match.captured(1);
                    qCDebug(wallpaperPreview) << "Found title in Steam cache:" << m_wallpaper.name;
                }
                
                QRegularExpression descRegex("\"description\"\\s*\"([^\"]+)\"");
                match = descRegex.match(content);
                if (match.hasMatch() && m_wallpaper.description.isEmpty()) {
                    QString description = match.captured(1);
                    description = description.replace("\\n", "\n").replace("\\t", "\t");
                    m_wallpaper.description = description;
                    qCDebug(wallpaperPreview) << "Found description in Steam cache";
                }
                
                break;
            }
        }
    }
}

// WallpaperPreview implementation
WallpaperPreview::WallpaperPreview(QWidget* parent)
    : QWidget(parent)
    , m_wallpaperManager(nullptr)
    , m_searchEdit(nullptr)
    , m_filterCombo(nullptr)
    , m_refreshButton(nullptr)
    , m_applyButton(nullptr)
    , m_scrollArea(nullptr)
    , m_gridWidget(nullptr)
    , m_gridLayout(nullptr)
    , m_paginationWidget(nullptr)
    , m_prevPageButton(nullptr)
    , m_nextPageButton(nullptr)
    , m_pageInfoLabel(nullptr)
    , m_selectedItem(nullptr)
    , m_currentPage(0)
    , m_totalPages(0)
    , m_workshopLoadTimer(new QTimer(this))
    , m_pendingItemIndex(0)
    , m_workshopBatchIndex(0)
    , m_currentItemsPerRow(PREFERRED_ITEMS_PER_ROW)
    , m_lastContainerWidth(0)
    , m_layoutUpdatePending(false)
{
    setupUI();
    
    connect(m_workshopLoadTimer, &QTimer::timeout, this, &WallpaperPreview::loadWorkshopDataBatch);
    m_workshopLoadTimer->setSingleShot(false);
}

void WallpaperPreview::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    // Search and filter controls
    auto* controlsWidget = new QWidget;
    auto* controlsLayout = new QHBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("Search wallpapers...");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &WallpaperPreview::onSearchTextChanged);
    
    m_filterCombo = new QComboBox;
    m_filterCombo->addItems({"All Types", "Scene", "Video", "Web"});
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &WallpaperPreview::onFilterChanged);
    
    m_refreshButton = new QPushButton("Refresh");
    connect(m_refreshButton, &QPushButton::clicked, this, &WallpaperPreview::onRefreshClicked);
    
    m_applyButton = new QPushButton("Apply Wallpaper");
    connect(m_applyButton, &QPushButton::clicked, this, &WallpaperPreview::onApplyClicked);
    
    controlsLayout->addWidget(m_searchEdit);
    controlsLayout->addWidget(m_filterCombo);
    controlsLayout->addWidget(m_refreshButton);
    controlsLayout->addWidget(m_applyButton);
    
    mainLayout->addWidget(controlsWidget);
    
    // Scroll area with grid
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    m_gridWidget = new QWidget;
    m_gridLayout = new QGridLayout(m_gridWidget);
    m_gridLayout->setSpacing(ITEM_SPACING);
    m_gridLayout->setContentsMargins(ITEM_SPACING, ITEM_SPACING, ITEM_SPACING, ITEM_SPACING);
    
    m_scrollArea->setWidget(m_gridWidget);
    mainLayout->addWidget(m_scrollArea);
    
    // Pagination controls
    setupPagination();
    mainLayout->addWidget(m_paginationWidget);
}

void WallpaperPreview::setupPagination()
{
    m_paginationWidget = new QWidget;
    auto* paginationLayout = new QHBoxLayout(m_paginationWidget);
    paginationLayout->setContentsMargins(0, 0, 0, 0);
    
    m_prevPageButton = new QPushButton("Previous");
    connect(m_prevPageButton, &QPushButton::clicked, this, &WallpaperPreview::onPreviousPage);
    
    m_pageInfoLabel = new QLabel("Page 1 of 1");
    m_pageInfoLabel->setAlignment(Qt::AlignCenter);
    
    m_nextPageButton = new QPushButton("Next");
    connect(m_nextPageButton, &QPushButton::clicked, this, &WallpaperPreview::onNextPage);
    
    paginationLayout->addWidget(m_prevPageButton);
    paginationLayout->addWidget(m_pageInfoLabel);
    paginationLayout->addWidget(m_nextPageButton);
    paginationLayout->addStretch();
}

void WallpaperPreview::setWallpaperManager(WallpaperManager* manager)
{
    m_wallpaperManager = manager;
    
    if (m_wallpaperManager) {
        connect(m_wallpaperManager, &WallpaperManager::wallpapersChanged,
                this, &WallpaperPreview::onWallpapersChanged);
    }
}

void WallpaperPreview::onWallpapersChanged()
{
    qCDebug(wallpaperPreview) << "onWallpapersChanged - refreshing grid";
    m_currentPage = 0;
    updateWallpaperGrid();
}

void WallpaperPreview::onSearchTextChanged(const QString& text)
{
    Q_UNUSED(text)
    m_currentPage = 0;
    updateWallpaperGrid();
}

void WallpaperPreview::onFilterChanged()
{
    qCDebug(wallpaperPreview) << "onFilterChanged to:" << m_filterCombo->currentText();
    m_currentPage = 0;
    updateWallpaperGrid();
}

void WallpaperPreview::onRefreshClicked()
{
    if (m_wallpaperManager) {
        m_wallpaperManager->refreshWallpapers();
    }
}

void WallpaperPreview::onApplyClicked()
{
    if (m_selectedItem) {
        emit wallpaperDoubleClicked(m_selectedItem->wallpaperInfo());
    }
}

void WallpaperPreview::onWallpaperItemClicked(const WallpaperInfo& wallpaper)
{
    if (m_selectedItem) {
        m_selectedItem->setSelected(false);
    }
    
    for (WallpaperPreviewItem* item : m_currentPageItems) {
        if (item && item->wallpaperInfo().id == wallpaper.id) {
            item->setSelected(true);
            m_selectedItem = item;
            break;
        }
    }
    
    emit wallpaperSelected(wallpaper);
}

void WallpaperPreview::onWallpaperItemDoubleClicked(const WallpaperInfo& wallpaper)
{
    emit wallpaperDoubleClicked(wallpaper);
}

QList<WallpaperInfo> WallpaperPreview::getFilteredWallpapers() const
{
    if (!m_wallpaperManager) {
        return QList<WallpaperInfo>();
    }
    
    QList<WallpaperInfo> allWallpapers = m_wallpaperManager->getAllWallpapers();
    QList<WallpaperInfo> filtered;
    
    QString searchText = m_searchEdit->text().toLower();
    QString filterType = m_filterCombo->currentText();
    
    for (const WallpaperInfo& wallpaper : allWallpapers) {
        bool matchesSearch = searchText.isEmpty() || 
                           wallpaper.name.toLower().contains(searchText) ||
                           wallpaper.description.toLower().contains(searchText);
        
        bool matchesFilter = (filterType == "All Types") || 
                           (wallpaper.type.compare(filterType, Qt::CaseInsensitive) == 0);
        
        if (matchesSearch && matchesFilter) {
            filtered.append(wallpaper);
        }
    }
    
    return filtered;
}

void WallpaperPreview::updateWallpaperGrid()
{
    clearCurrentPage();
    
    m_filteredWallpapers = getFilteredWallpapers();
    m_totalPages = qMax(1, (m_filteredWallpapers.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);
    
    if (m_currentPage >= m_totalPages) {
        m_currentPage = qMax(0, m_totalPages - 1);
    }
    
    loadCurrentPage();
    updatePageInfo();
}

void WallpaperPreview::loadCurrentPage()
{
    int startIndex = m_currentPage * ITEMS_PER_PAGE;
    int endIndex = qMin(startIndex + ITEMS_PER_PAGE, m_filteredWallpapers.size());
    
    int itemsPerRow = calculateItemsPerRow();
    int row = 0, col = 0;
    
    for (int i = startIndex; i < endIndex; ++i) {
        WallpaperPreviewItem* item = new WallpaperPreviewItem(m_filteredWallpapers[i]);
        
        connect(item, &WallpaperPreviewItem::clicked, 
                this, &WallpaperPreview::onWallpaperItemClicked);
        connect(item, &WallpaperPreviewItem::doubleClicked, 
                this, &WallpaperPreview::onWallpaperItemDoubleClicked);
        
        m_gridLayout->addWidget(item, row, col);
        m_currentPageItems.append(item);
        
        col++;
        if (col >= itemsPerRow) {
            col = 0;
            row++;
        }
    }
    
    m_gridLayout->setHorizontalSpacing(ITEM_SPACING);
    m_gridLayout->setVerticalSpacing(ITEM_SPACING);
    
    startWallpaperDataLoading();
    
    // Start animations for current page items after a short delay
    // This allows the UI to settle before starting resource-intensive animations
    QTimer* animTimer = new QTimer(this);
    animTimer->setSingleShot(true);
    connect(animTimer, &QTimer::timeout, this, [this, animTimer]() {
        startCurrentPageAnimations();
        animTimer->deleteLater();
    });
    animTimer->start(200);
}

void WallpaperPreview::clearCurrentPage()
{
    // Stop any pending workshop data loading first
    m_workshopLoadTimer->stop();
    cancelAllPendingOperations();
    m_pendingWorkshopItems.clear();
    
    // Stop animations before clearing items
    stopCurrentPageAnimations();
    
    // Now it's safe to delete the items
    for (WallpaperPreviewItem* item : m_currentPageItems) {
        if (item) {
            m_gridLayout->removeWidget(item);
            item->setParent(nullptr);
            item->deleteLater();
        }
    }
    m_currentPageItems.clear();
    m_selectedItem = nullptr;
}

void WallpaperPreview::startCurrentPageAnimations()
{
    qCDebug(wallpaperPreview) << "Starting animations for current page items:" << m_currentPageItems.size();
    
    int animatedCount = 0;
    for (WallpaperPreviewItem* item : m_currentPageItems) {
        if (item && item->hasAnimatedPreview()) {
            animatedCount++;
            qCDebug(wallpaperPreview) << "Starting animation for item:" << item->wallpaperInfo().name;
            item->startAnimation();
        }
    }
    
    qCDebug(wallpaperPreview) << "Started animations for" << animatedCount << "items";
}

void WallpaperPreview::stopCurrentPageAnimations()
{
    qCDebug(wallpaperPreview) << "Stopping animations for current page items";
    
    for (WallpaperPreviewItem* item : m_currentPageItems) {
        if (item && item->isAnimationPlaying()) {
            item->stopAnimation();
        }
    }
}

void WallpaperPreview::updatePageInfo()
{
    m_pageInfoLabel->setText(QString("Page %1 of %2 (%3 wallpapers)")
                            .arg(m_currentPage + 1)
                            .arg(m_totalPages)
                            .arg(m_filteredWallpapers.size()));
    
    m_prevPageButton->setEnabled(m_currentPage > 0);
    m_nextPageButton->setEnabled(m_currentPage < m_totalPages - 1);
}

void WallpaperPreview::onPreviousPage()
{
    if (m_currentPage > 0) {
        // Stop any pending workshop data loading
        m_workshopLoadTimer->stop();
        cancelAllPendingOperations();
        m_pendingWorkshopItems.clear();
        
        m_currentPage--;
        onPageChanged();
    }
}

void WallpaperPreview::onNextPage()
{
    if (m_currentPage < m_totalPages - 1) {
        // Stop any pending workshop data loading
        m_workshopLoadTimer->stop();
        cancelAllPendingOperations();
        m_pendingWorkshopItems.clear();
        
        m_currentPage++;
        onPageChanged();
    }
}

void WallpaperPreview::onPageChanged()
{
    updateWallpaperGrid();
}

int WallpaperPreview::calculateItemsPerRow() const
{
    if (!m_scrollArea) {
        return PREFERRED_ITEMS_PER_ROW;
    }
    
    int availableWidth = m_scrollArea->viewport()->width() - 2 * ITEM_SPACING;
    int itemWidthWithSpacing = WallpaperPreviewItem::ITEM_WIDTH + ITEM_SPACING;
    
    int itemsPerRow = qMax(MIN_ITEMS_PER_ROW, availableWidth / itemWidthWithSpacing);
    return qMin(itemsPerRow, MAX_ITEMS_PER_ROW);
}

void WallpaperPreview::resizeEvent(QResizeEvent* event)
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

void WallpaperPreview::recalculateLayout()
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
        qCDebug(wallpaperPreview) << "Recalculating layout: columns" << m_currentItemsPerRow << "->" << newItemsPerRow;
        adjustGridForNewColumnCount(newItemsPerRow);
        m_currentItemsPerRow = newItemsPerRow;
    }
    
    m_lastContainerWidth = currentSize.width();
}

void WallpaperPreview::adjustGridForNewColumnCount(int newColumnCount)
{
    if (!m_gridLayout || newColumnCount <= 0) {
        return;
    }
    
    QList<WallpaperPreviewItem*> allItems;
    
    for (int i = 0; i < m_gridLayout->count(); ++i) {
        QLayoutItem* item = m_gridLayout->itemAt(i);
        if (item && item->widget()) {
            WallpaperPreviewItem* previewItem = qobject_cast<WallpaperPreviewItem*>(item->widget());
            if (previewItem) {
                allItems.append(previewItem);
            }
        }
    }
    
    while (QLayoutItem* item = m_gridLayout->takeAt(0)) {
        delete item;
    }
    
    int row = 0, col = 0;
    for (WallpaperPreviewItem* item : allItems) {
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

void WallpaperPreview::startWallpaperDataLoading()
{
    // Cancel any previous loading operations
    m_workshopLoadTimer->stop();
    cancelAllPendingOperations();
    
    m_pendingWorkshopItems = m_currentPageItems;
    m_workshopBatchIndex = 0;
    
    if (!m_pendingWorkshopItems.isEmpty()) {
        // Add a short delay before starting the batch loading
        // This helps avoid race conditions when rapidly switching pages
        QTimer* loadTimer = new QTimer(this);
        loadTimer->setSingleShot(true);
        connect(loadTimer, &QTimer::timeout, this, [this, loadTimer]() {
            if (!m_pendingWorkshopItems.isEmpty()) {
                m_workshopLoadTimer->start(WORKSHOP_BATCH_DELAY);
                loadWorkshopDataBatch();
            }
            loadTimer->deleteLater();
        });
        loadTimer->start(100);
    }
}

void WallpaperPreview::loadWorkshopDataBatch()
{
    // Add safety check in case we have no items (deleted during page change)
    if (m_pendingWorkshopItems.isEmpty()) {
        m_workshopLoadTimer->stop();
        return;
    }
    
    if (m_workshopBatchIndex >= m_pendingWorkshopItems.size()) {
        m_workshopLoadTimer->stop();
        return;
    }
    
    int endIndex = qMin(m_workshopBatchIndex + WORKSHOP_BATCH_SIZE, m_pendingWorkshopItems.size());
    
    for (int i = m_workshopBatchIndex; i < endIndex; ++i) {
        if (i >= m_pendingWorkshopItems.size()) {
            // Double check index bounds
            break;
        }
        
        WallpaperPreviewItem* item = m_pendingWorkshopItems[i];
        
        // Comprehensive validity check
        if (!item || item->isCancelled() || !m_currentPageItems.contains(item)) {
            continue;
        }
        
        if (!item->isWorkshopDataLoaded()) {
            item->loadWorkshopDataNow();
        }
    }
    
    m_workshopBatchIndex = endIndex;
    
    if (m_workshopBatchIndex >= m_pendingWorkshopItems.size()) {
        m_workshopLoadTimer->stop();
    }
}

void WallpaperPreview::refreshWallpapers()
{
    if (m_wallpaperManager) {
        m_wallpaperManager->refreshWallpapers();
    }
}

void WallpaperPreview::selectWallpaper(const QString& wallpaperId)
{
    // First, try to find the wallpaper on the current page
    for (WallpaperPreviewItem* item : m_currentPageItems) {
        if (item && item->wallpaperInfo().id == wallpaperId) {
            // Update visual selection without emitting signal to avoid loops
            if (m_selectedItem) {
                m_selectedItem->setSelected(false);
            }
            
            item->setSelected(true);
            m_selectedItem = item;
            
            // Scroll to make the selected item visible
            scrollToItem(item);
            return; // Found on current page, we're done
        }
    }
    
    // If not found on current page, search through all filtered wallpapers
    // and navigate to the correct page
    QList<WallpaperInfo> allFiltered = getFilteredWallpapers();
    for (int i = 0; i < allFiltered.size(); ++i) {
        if (allFiltered[i].id == wallpaperId) {
            // Found the wallpaper, calculate which page it's on
            int targetPage = i / ITEMS_PER_PAGE;
            
            if (targetPage != m_currentPage) {
                // Navigate to the correct page
                qCDebug(wallpaperPreview) << "Navigating to page" << (targetPage + 1) << "to select wallpaper:" << wallpaperId;
                m_currentPage = targetPage;
                updateWallpaperGrid(); // This will load the new page and clear current selection
                
                // After the page loads, try to select the wallpaper again
                QTimer::singleShot(100, this, [this, wallpaperId]() {
                    // Try selecting on the new page
                    for (WallpaperPreviewItem* item : m_currentPageItems) {
                        if (item && item->wallpaperInfo().id == wallpaperId) {
                            if (m_selectedItem) {
                                m_selectedItem->setSelected(false);
                            }
                            item->setSelected(true);
                            m_selectedItem = item;
                            
                            // Scroll to make the selected item visible
                            scrollToItem(item);
                            qCDebug(wallpaperPreview) << "Successfully selected wallpaper on new page:" << wallpaperId;
                            break;
                        }
                    }
                });
            }
            return; // Found the wallpaper (will be selected after page load)
        }
    }
    
    // Wallpaper not found in current filters - it might be filtered out
    qCDebug(wallpaperPreview) << "Wallpaper not found in current view (may be filtered out):" << wallpaperId;
}

void WallpaperPreview::updateTheme()
{
    for (WallpaperPreviewItem* item : m_currentPageItems) {
        if (item) {
            item->updateStyle();
        }
    }
    update();
}

WallpaperInfo WallpaperPreview::getSelectedWallpaper() const
{
    if (m_selectedItem) {
        return m_selectedItem->wallpaperInfo();
    }
    return WallpaperInfo();
}

QString WallpaperPreview::getSelectedWallpaperId() const
{
    if (m_selectedItem) {
        return m_selectedItem->wallpaperInfo().id;
    }
    return QString();
}

QString WallpaperPreview::getWorkshopDirectory() const
{
    return QString();
}

QStringList WallpaperPreview::getWorkshopIds() const
{
    return QStringList();
}

QString WallpaperPreview::getWorkshopIdFromFilesystem(const QString& wallpaperPath)
{
    Q_UNUSED(wallpaperPath)
    return QString();
}

QStringList WallpaperPreview::scanWorkshopDirectory()
{
    return QStringList();
}

void WallpaperPreview::cancelAllPendingOperations()
{
    // Cancel any pending operations in all current page items
    for (WallpaperPreviewItem* item : m_currentPageItems) {
        if (item) {
            item->cancelPendingOperations();
        }
    }
    
    // Also cancel any operations in pending items list
    for (WallpaperPreviewItem* item : m_pendingWorkshopItems) {
        if (item) {
            item->cancelPendingOperations();
        }
    }
}

void WallpaperPreview::scrollToItem(WallpaperPreviewItem* item)
{
    if (!item || !m_scrollArea) {
        return;
    }
    
    // Get the item's position relative to the grid widget
    QPoint itemPos = item->pos();
    QSize itemSize = item->size();
    
    // Calculate the center of the item
    QPoint itemCenter = itemPos + QPoint(itemSize.width() / 2, itemSize.height() / 2);
    
    // Get the scroll area's viewport size
    QSize viewportSize = m_scrollArea->viewport()->size();
    
    // Calculate the desired scroll position to center the item in the viewport
    int desiredX = itemCenter.x() - viewportSize.width() / 2;
    int desiredY = itemCenter.y() - viewportSize.height() / 2;
    
    // Ensure the scroll position is within valid bounds
    QScrollBar* hScrollBar = m_scrollArea->horizontalScrollBar();
    QScrollBar* vScrollBar = m_scrollArea->verticalScrollBar();
    
    if (hScrollBar) {
        desiredX = qMax(hScrollBar->minimum(), qMin(desiredX, hScrollBar->maximum()));
        hScrollBar->setValue(desiredX);
    }
    
    if (vScrollBar) {
        desiredY = qMax(vScrollBar->minimum(), qMin(desiredY, vScrollBar->maximum()));
        vScrollBar->setValue(desiredY);
    }
    
    qCDebug(wallpaperPreview) << "Scrolled to item at position:" << itemPos << "center:" << itemCenter << "viewport size:" << viewportSize;
}

// alias for the calculateLayout() calls in this cpp
void WallpaperPreview::calculateLayout()
{
    recalculateLayout();
}

// include the moc output for WallpaperPreview.h so staticMetaObject, vtables, signals, etc. are available
#include "moc_WallpaperPreview.cpp"