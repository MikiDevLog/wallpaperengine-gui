#include "SteamApiManager.h"
#include "ConfigManager.h"
#include <QNetworkRequest>
#include <QNetworkReply>  // Add this include for QNetworkReply
#include <QStandardPaths> // Add this include for QStandardPaths
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QLoggingCategory>
#include <QTimer>  // Add this include for QTimer::singleShot

Q_LOGGING_CATEGORY(steamApi, "app.steamApi")

SteamApiManager::SteamApiManager(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_pendingRequests(0)
{
    // Create cache directory if it doesn't exist
    QDir cacheDir(getCachePath());
    if (!cacheDir.exists()) {
        cacheDir.mkpath(".");
    }
    
    // Create user profiles cache directory
    QDir userProfilesDir(getCachePath() + "/userprofiles");
    if (!userProfilesDir.exists()) {
        userProfilesDir.mkpath(".");
    }
    
    // Load API key from config
    m_apiKey = ConfigManager::instance().steamApiKey();
    
    qCInfo(steamApi) << "Steam API manager initialized with" << (m_apiKey.isEmpty() ? "no API key" : "API key");
}

SteamApiManager::~SteamApiManager()
{
}

SteamApiManager& SteamApiManager::instance()
{
    static SteamApiManager instance;
    return instance;
}

bool SteamApiManager::setApiKey(const QString& apiKey)
{
    if (apiKey != m_apiKey) {
        m_apiKey = apiKey;
        ConfigManager::instance().setSteamApiKey(apiKey);
        qCInfo(steamApi) << "API key updated:" << (apiKey.isEmpty() ? "cleared" : "set");
    }
    
    return !apiKey.isEmpty();
}

QString SteamApiManager::apiKey() const
{
    return m_apiKey;
}

bool SteamApiManager::hasApiKey() const
{
    return !m_apiKey.isEmpty();
}

void SteamApiManager::testApiKey(const QString& itemId)
{
    if (m_apiKey.isEmpty()) {
        emit apiKeyTestFailed("API key is not set");
        return;
    }
    
    QUrl url("https://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    
    QUrlQuery params;
    params.addQueryItem("key", m_apiKey);
    params.addQueryItem("itemcount", "1");
    params.addQueryItem("publishedfileids[0]", itemId);
    
    qCDebug(steamApi) << "Testing API key with item:" << itemId;
    
    QNetworkReply* reply = m_networkManager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, itemId]() {
        reply->deleteLater();
        
        if (reply->error() != QNetworkReply::NoError) {
            QString error = QString("Network error: %1").arg(reply->errorString());
            qCWarning(steamApi) << "API test failed:" << error;
            emit apiKeyTestFailed(error);
            return;
        }
        
        QByteArray data = reply->readAll();
        bool ok = false;
        QString errorMsg;
        QJsonObject response = parseApiResponse(data, ok, errorMsg);
        
        if (!ok) {
            qCWarning(steamApi) << "API test failed:" << errorMsg;
            emit apiKeyTestFailed(errorMsg);
            return;
        }
        
        // Check if we got valid response data
        QJsonObject responseDetails = response.value("response").toObject();
        if (responseDetails.value("result").toInt() != 1 || 
            responseDetails.value("resultcount").toInt() != 1) {
            qCWarning(steamApi) << "API test failed: Invalid response data";
            emit apiKeyTestFailed("Invalid response data from Steam API");
            return;
        }
        
        QJsonArray itemDetails = responseDetails.value("publishedfiledetails").toArray();
        if (itemDetails.isEmpty()) {
            qCWarning(steamApi) << "API test failed: No item details returned";
            emit apiKeyTestFailed("No item details returned from Steam API");
            return;
        }
        
        QJsonObject itemDetail = itemDetails.first().toObject();
        if (itemDetail.value("result").toInt() != 1) {
            QString message = itemDetail.value("message").toString();
            qCWarning(steamApi) << "API test failed:" << message;
            emit apiKeyTestFailed(message.isEmpty() ? "Item not found" : message);
            return;
        }
        
        qCInfo(steamApi) << "API test successful!";
        emit apiKeyTestSucceeded();
        
        // Save the test item details
        WorkshopItemInfo info = parseWorkshopItem(itemDetail);
        m_itemCache[itemId] = info;
        saveToCache(info);
        emit itemDetailsReceived(itemId, info);
    });
}

void SteamApiManager::fetchItemDetails(const QString& itemId)
{
    if (m_apiKey.isEmpty()) {
        emit itemDetailsError(itemId, "API key is not set");
        return;
    }
    
    // Check cache first
    WorkshopItemInfo cachedInfo;
    if (loadFromCache(itemId, cachedInfo)) {
        qCDebug(steamApi) << "Using cached data for item:" << itemId;
        emit itemDetailsReceived(itemId, cachedInfo);
        return;
    }
    
    QUrl url("https://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    
    QUrlQuery params;
    params.addQueryItem("key", m_apiKey);
    params.addQueryItem("itemcount", "1");
    params.addQueryItem("publishedfileids[0]", itemId);
    
    qCDebug(steamApi) << "Fetching details for item:" << itemId;
    
    QNetworkReply* reply = m_networkManager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, itemId]() {
        reply->deleteLater();
        
        if (reply->error() != QNetworkReply::NoError) {
            QString error = QString("Network error: %1").arg(reply->errorString());
            qCWarning(steamApi) << "Failed to fetch item details:" << error;
            emit itemDetailsError(itemId, error);
            return;
        }
        
        QByteArray data = reply->readAll();
        bool ok = false;
        QString errorMsg;
        QJsonObject response = parseApiResponse(data, ok, errorMsg);
        
        if (!ok) {
            qCWarning(steamApi) << "Failed to parse item details:" << errorMsg;
            emit itemDetailsError(itemId, errorMsg);
            return;
        }
        
        QJsonObject responseDetails = response.value("response").toObject();
        if (responseDetails.value("result").toInt() != 1) {
            QString error = "API error: " + QString::number(responseDetails.value("result").toInt());
            qCWarning(steamApi) << "API error:" << error;
            emit itemDetailsError(itemId, error);
            return;
        }
        
        QJsonArray itemDetails = responseDetails.value("publishedfiledetails").toArray();
        if (itemDetails.isEmpty()) {
            qCWarning(steamApi) << "No item details returned for:" << itemId;
            emit itemDetailsError(itemId, "No item details returned");
            return;
        }
        
        QJsonObject itemDetail = itemDetails.first().toObject();
        if (itemDetail.value("result").toInt() != 1) {
            QString message = itemDetail.value("message").toString();
            qCWarning(steamApi) << "Item fetch failed:" << message;
            emit itemDetailsError(itemId, message.isEmpty() ? "Item not found" : message);
            return;
        }
        
        WorkshopItemInfo info = parseWorkshopItem(itemDetail);
        
        // Check for updates
        checkForUpdates(info);
        
        // Cache the result
        m_itemCache[itemId] = info;
        saveToCache(info);
        
        qCDebug(steamApi) << "Successfully fetched details for item:" << itemId;
        emit itemDetailsReceived(itemId, info);
    });
}

void SteamApiManager::fetchItemDetails(const QStringList& itemIds)
{
    if (m_apiKey.isEmpty()) {
        for (const QString& itemId : itemIds) {
            emit itemDetailsError(itemId, "API key is not set");
        }
        emit batchDetailsCompleted();
        return;
    }
    
    if (itemIds.isEmpty()) {
        emit batchDetailsCompleted();
        return;
    }
    
    // Process in batches of 100 (Steam API limit)
    const int BATCH_SIZE = 100;
    int totalBatches = (itemIds.size() + BATCH_SIZE - 1) / BATCH_SIZE;
    m_pendingRequests = totalBatches;
    
    qCInfo(steamApi) << "Fetching details for" << itemIds.size() 
                     << "items in" << totalBatches << "batches";
    
    for (int batchIndex = 0; batchIndex < totalBatches; batchIndex++) {
        QUrl url("https://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        
        QUrlQuery params;
        params.addQueryItem("key", m_apiKey);
        
        // Calculate start and end indices for this batch
        int startIdx = batchIndex * BATCH_SIZE;
        int endIdx = qMin(startIdx + BATCH_SIZE, itemIds.size());
        int batchCount = endIdx - startIdx;
        
        params.addQueryItem("itemcount", QString::number(batchCount));
        
        // Add all item IDs for this batch
        for (int i = 0; i < batchCount; i++) {
            QString itemId = itemIds[startIdx + i];
            params.addQueryItem(QString("publishedfileids[%1]").arg(i), itemId);
            
            // Check cache first
            WorkshopItemInfo cachedInfo;
            if (loadFromCache(itemId, cachedInfo)) {
                qCDebug(steamApi) << "Using cached data for item:" << itemId;
                emit itemDetailsReceived(itemId, cachedInfo);
                
                // Remove from batch request if we already have it cached
                params.removeAllQueryItems(QString("publishedfileids[%1]").arg(i));
                batchCount--;
            }
        }
        
        // If all items in this batch were cached, skip the request
        if (batchCount == 0) {
            m_pendingRequests--;
            if (m_pendingRequests == 0) {
                emit batchDetailsCompleted();
            }
            continue;
        }
        
        // Update the itemcount after potential cache hits
        params.removeAllQueryItems("itemcount");
        params.addQueryItem("itemcount", QString::number(batchCount));
        
        qCDebug(steamApi) << "Fetching batch" << (batchIndex + 1) << "of" << totalBatches
                          << "with" << batchCount << "items";
        
        QNetworkReply* reply = m_networkManager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
        
        connect(reply, &QNetworkReply::finished, this, [this, reply, itemIds, startIdx, endIdx]() {
            reply->deleteLater();
            
            if (reply->error() != QNetworkReply::NoError) {
                QString error = QString("Network error: %1").arg(reply->errorString());
                qCWarning(steamApi) << "Failed to fetch batch details:" << error;
                
                // Report error for each item in the batch
                for (int i = startIdx; i < endIdx; i++) {
                    emit itemDetailsError(itemIds[i], error);
                }
            } else {
                QByteArray data = reply->readAll();
                bool ok = false;
                QString errorMsg;
                QJsonObject response = parseApiResponse(data, ok, errorMsg);
                
                if (ok) {
                    QJsonObject responseDetails = response.value("response").toObject();
                    QJsonArray itemDetails = responseDetails.value("publishedfiledetails").toArray();
                    
                    for (int i = 0; i < itemDetails.size(); i++) {
                        QJsonObject itemDetail = itemDetails[i].toObject();
                        QString itemId = itemDetail.value("publishedfileid").toString();
                        
                        if (itemDetail.value("result").toInt() == 1) {
                            WorkshopItemInfo info = parseWorkshopItem(itemDetail);
                            
                            // Check for updates
                            checkForUpdates(info);
                            
                            // Cache the result
                            m_itemCache[itemId] = info;
                            saveToCache(info);
                            
                            emit itemDetailsReceived(itemId, info);
                        } else {
                            QString message = itemDetail.value("message").toString();
                            emit itemDetailsError(itemId, message.isEmpty() ? "Item not found" : message);
                        }
                    }
                } else {
                    qCWarning(steamApi) << "Failed to parse batch response:" << errorMsg;
                    for (int i = startIdx; i < endIdx; i++) {
                        emit itemDetailsError(itemIds[i], errorMsg);
                    }
                }
            }
            
            m_pendingRequests--;
            if (m_pendingRequests == 0) {
                emit batchDetailsCompleted();
            }
        });
    }
}

bool SteamApiManager::hasUpdates(const QString& itemId) const
{
    if (m_itemCache.contains(itemId)) {
        return m_itemCache[itemId].hasUpdate;
    }
    
    WorkshopItemInfo info;
    if (loadFromCache(itemId, info)) {
        return info.hasUpdate;
    }
    
    return false;
}

WorkshopItemInfo SteamApiManager::getCachedItemInfo(const QString& itemId) const
{
    // Check memory cache first
    if (m_itemCache.contains(itemId)) {
        return m_itemCache[itemId];
    }
    
    // Try to load from disk cache
    WorkshopItemInfo info;
    if (loadFromCache(itemId, info)) {
        return info;
    }
    
    // Return empty info if not found
    info.itemId = itemId;  // Changed from info.id = itemId
    info.title = "Unknown";
    info.description = "No data available";
    info.creator = "";
    info.creatorName = "";
    info.previewUrl = "";
    info.type = "";
    info.genre = "";
    info.fileSize = 0;
    info.hasUpdate = false;
    info.views = 0;
    info.subscriptions = 0;
    info.favorites = 0;
    return info;
}

bool SteamApiManager::hasCachedInfo(const QString& itemId) const
{
    if (m_itemCache.contains(itemId)) {
        return true;
    }
    
    QFile file(getItemCachePath(itemId));
    return file.exists();
}

bool SteamApiManager::saveToCache(const WorkshopItemInfo& info)
{
    QJsonObject json;
    json["id"] = info.id();
    json["title"] = info.title;
    json["description"] = info.description;
    json["creator"] = info.creator;
    json["creatorName"] = info.creatorName;
    json["previewUrl"] = info.previewUrl;
    json["type"] = info.type;
    json["genre"] = info.genre;
    json["tags"] = QJsonArray::fromStringList(info.tags);
    json["fileSize"] = QString::number(info.fileSize);  // Use string to avoid precision loss
    json["created"] = info.created.toString(Qt::ISODate);
    json["updated"] = info.updated.toString(Qt::ISODate);
    json["views"] = info.views;
    json["subscriptions"] = info.subscriptions;
    json["favorites"] = info.favorites;
    json["hasUpdate"] = info.hasUpdate;
    
    QJsonDocument doc(json);
    QFile file(getItemCachePath(info.id()));
    
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(steamApi) << "Failed to open cache file for writing:" << file.fileName();
        return false;
    }
    
    file.write(doc.toJson());
    return true;
}

bool SteamApiManager::loadFromCache(const QString& itemId, WorkshopItemInfo& info) const
{
    QFile file(getItemCachePath(itemId));
    
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        qCWarning(steamApi) << "Invalid cache file for item:" << itemId;
        return false;
    }
    
    QJsonObject json = doc.object();
    
    info.itemId = json["id"].toString();  // Changed from info.id = json["id"].toString()
    info.title = json["title"].toString();
    info.description = json["description"].toString();
    info.creator = json["creator"].toString();
    info.creatorName = json["creatorName"].toString();
    info.previewUrl = json["previewUrl"].toString();
    info.type = json["type"].toString();
    info.genre = json["genre"].toString();
    
    QJsonArray tagsArray = json["tags"].toArray();
    info.tags.clear();
    for (const QJsonValue& tag : tagsArray) {
        info.tags.append(tag.toString());
    }
    
    info.fileSize = json["fileSize"].toString().toLongLong();
    info.created = QDateTime::fromString(json["created"].toString(), Qt::ISODate);
    info.updated = QDateTime::fromString(json["updated"].toString(), Qt::ISODate);
    info.views = json["views"].toInt();
    info.subscriptions = json["subscriptions"].toInt();
    info.favorites = json["favorites"].toInt();
    info.hasUpdate = json["hasUpdate"].toBool();
    
    return true;
}

QStringList SteamApiManager::getAllCachedItemIds() const
{
    QDir cacheDir(getCachePath());
    QStringList itemIds;
    
    QStringList entries = cacheDir.entryList(QStringList() << "*.json", QDir::Files);
    for (const QString& entry : entries) {
        QString itemId = entry;
        itemId.chop(5); // Remove .json extension
        itemIds.append(itemId);
    }
    
    return itemIds;
}

void SteamApiManager::clearCache()
{
    QDir cacheDir(getCachePath());
    QStringList entries = cacheDir.entryList(QStringList() << "*.json", QDir::Files);
    
    for (const QString& entry : entries) {
        cacheDir.remove(entry);
    }
    
    m_itemCache.clear();
    qCInfo(steamApi) << "Cache cleared";
}

QJsonObject SteamApiManager::parseApiResponse(const QByteArray& response, bool& ok, QString& errorMsg)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        ok = false;
        errorMsg = QString("JSON parse error: %1").arg(parseError.errorString());
        return QJsonObject();
    }
    
    if (!doc.isObject()) {
        ok = false;
        errorMsg = "Response is not a JSON object";
        return QJsonObject();
    }
    
    ok = true;
    return doc.object();
}

void SteamApiManager::fetchUserProfile(const QString& steamId)
{
    if (m_apiKey.isEmpty()) {
        emit userProfileError(steamId, "API key is not set");
        return;
    }
    
    // Check cache first
    SteamUserProfile cachedProfile;
    if (loadUserProfileFromCache(steamId, cachedProfile)) {
        qCDebug(steamApi) << "Using cached profile data for user:" << steamId;
        emit userProfileReceived(steamId, cachedProfile);
        return;
    }
    
    // Build API URL
    QUrl url("https://api.steampowered.com/ISteamUser/GetPlayerSummaries/v0002/");
    QUrlQuery query;
    query.addQueryItem("key", m_apiKey);
    query.addQueryItem("steamids", steamId);
    url.setQuery(query);
    
    QNetworkRequest request(url);
    
    qCDebug(steamApi) << "Fetching user profile for Steam ID:" << steamId;
    
    QNetworkReply* reply = m_networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, steamId]() {
        reply->deleteLater();
        
        if (reply->error() != QNetworkReply::NoError) {
            QString error = QString("Network error: %1").arg(reply->errorString());
            qCWarning(steamApi) << "Failed to fetch user profile:" << error;
            emit userProfileError(steamId, error);
            return;
        }
        
        QByteArray data = reply->readAll();
        bool ok = false;
        QString errorMsg;
        QJsonObject response = parseApiResponse(data, ok, errorMsg);
        
        if (!ok) {
            qCWarning(steamApi) << "Failed to parse user profile response:" << errorMsg;
            emit userProfileError(steamId, errorMsg);
            return;
        }
        
        QJsonObject responseObj = response.value("response").toObject();
        QJsonArray players = responseObj.value("players").toArray();
        
        if (players.isEmpty()) {
            qCWarning(steamApi) << "No user profile found for Steam ID:" << steamId;
            emit userProfileError(steamId, "User not found");
            return;
        }
        
        QJsonObject playerObj = players.first().toObject();
        SteamUserProfile profile = parseUserProfile(playerObj);
        
        // Cache the result
        m_userProfileCache[steamId] = profile;
        saveUserProfileToCache(profile);
        
        qCDebug(steamApi) << "Successfully fetched profile for user:" << steamId 
                         << "Name:" << profile.personaName;
        emit userProfileReceived(steamId, profile);
    });
}

void SteamApiManager::fetchUserProfiles(const QStringList& steamIds)
{
    if (m_apiKey.isEmpty()) {
        for (const QString& steamId : steamIds) {
            emit userProfileError(steamId, "API key is not set");
        }
        return;
    }
    
    if (steamIds.isEmpty()) {
        return;
    }
    
    // Process in batches of 100 (Steam API limit)
    const int BATCH_SIZE = 100;
    int totalBatches = (steamIds.size() + BATCH_SIZE - 1) / BATCH_SIZE;
    
    for (int batchIndex = 0; batchIndex < totalBatches; batchIndex++) {
        // Calculate start and end indices for this batch
        int startIdx = batchIndex * BATCH_SIZE;
        int endIdx = qMin(startIdx + BATCH_SIZE, steamIds.size());
        
        // Build comma-separated list of Steam IDs for this batch
        QStringList batchIds;
        for (int i = startIdx; i < endIdx; i++) {
            QString steamId = steamIds[i];
            
            // Check cache first
            SteamUserProfile cachedProfile;
            if (loadUserProfileFromCache(steamId, cachedProfile)) {
                qCDebug(steamApi) << "Using cached profile data for user:" << steamId;
                emit userProfileReceived(steamId, cachedProfile);
            } else {
                batchIds.append(steamId);
            }
        }
        
        // Skip API call if all profiles in this batch were cached
        if (batchIds.isEmpty()) {
            continue;
        }
        
        // Build API URL
        QUrl url("https://api.steampowered.com/ISteamUser/GetPlayerSummaries/v0002/");
        QUrlQuery query;
        query.addQueryItem("key", m_apiKey);
        query.addQueryItem("steamids", batchIds.join(","));
        url.setQuery(query);
        
        QNetworkRequest request(url);
        
        qCDebug(steamApi) << "Fetching profiles for" << batchIds.size() << "users in batch" 
                         << (batchIndex + 1) << "of" << totalBatches;
        
        QNetworkReply* reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply, batchIds]() {
            reply->deleteLater();
            
            if (reply->error() != QNetworkReply::NoError) {
                QString error = QString("Network error: %1").arg(reply->errorString());
                qCWarning(steamApi) << "Failed to fetch user profiles:" << error;
                for (const QString& steamId : batchIds) {
                    emit userProfileError(steamId, error);
                }
                return;
            }
            
            QByteArray data = reply->readAll();
            bool ok = false;
            QString errorMsg;
            QJsonObject response = parseApiResponse(data, ok, errorMsg);
            
            if (!ok) {
                qCWarning(steamApi) << "Failed to parse user profiles response:" << errorMsg;
                for (const QString& steamId : batchIds) {
                    emit userProfileError(steamId, errorMsg);
                }
                return;
            }
            
            QJsonObject responseObj = response.value("response").toObject();
            QJsonArray players = responseObj.value("players").toArray();
            
            // Create a map of steam IDs to profiles for quick lookups
            QMap<QString, SteamUserProfile> profilesMap;
            
            for (const QJsonValue& playerValue : players) {
                QJsonObject playerObj = playerValue.toObject();
                SteamUserProfile profile = parseUserProfile(playerObj);
                
                // Cache the result
                m_userProfileCache[profile.steamId] = profile;
                saveUserProfileToCache(profile);
                profilesMap[profile.steamId] = profile;
                
                emit userProfileReceived(profile.steamId, profile);
            }
            
            // Report errors for any IDs that weren't found
            for (const QString& steamId : batchIds) {
                if (!profilesMap.contains(steamId)) {
                    qCWarning(steamApi) << "No user profile found for Steam ID:" << steamId;
                    emit userProfileError(steamId, "User not found");
                }
            }
        });
    }
}

SteamUserProfile SteamApiManager::parseUserProfile(const QJsonObject& profileObject)
{
    SteamUserProfile profile;
    
    profile.steamId = profileObject.value("steamid").toString();
    profile.personaName = profileObject.value("personaname").toString();
    profile.profileUrl = profileObject.value("profileurl").toString();
    profile.avatarUrl = profileObject.value("avatarfull").toString();
    profile.countryCode = profileObject.value("loccountrycode").toString();
    
    return profile;
}

bool SteamApiManager::hasCachedUserProfile(const QString& steamId) const
{
    if (m_userProfileCache.contains(steamId)) {
        return true;
    }
    
    QFile file(getUserProfileCachePath(steamId));
    return file.exists();
}

SteamUserProfile SteamApiManager::getCachedUserProfile(const QString& steamId) const
{
    // Check memory cache first
    if (m_userProfileCache.contains(steamId)) {
        return m_userProfileCache[steamId];
    }
    
    // Try to load from disk cache
    SteamUserProfile profile;
    if (loadUserProfileFromCache(steamId, profile)) {
        return profile;
    }
    
    // Return empty profile if not found
    profile.steamId = steamId;
    profile.personaName = "Unknown User";
    return profile;
}

bool SteamApiManager::saveUserProfileToCache(const SteamUserProfile& profile)
{
    QJsonObject json;
    json["steamId"] = profile.steamId;
    json["personaName"] = profile.personaName;
    json["profileUrl"] = profile.profileUrl;
    json["avatarUrl"] = profile.avatarUrl;
    json["countryCode"] = profile.countryCode;
    
    QJsonDocument doc(json);
    QFile file(getUserProfileCachePath(profile.steamId));
    
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(steamApi) << "Failed to open cache file for writing:" << file.fileName();
        return false;
    }
    
    file.write(doc.toJson());
    return true;
}

bool SteamApiManager::loadUserProfileFromCache(const QString& steamId, SteamUserProfile& profile) const
{
    QFile file(getUserProfileCachePath(steamId));
    
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        qCWarning(steamApi) << "Invalid cache file for user profile:" << steamId;
        return false;
    }
    
    QJsonObject json = doc.object();
    
    profile.steamId = json["steamId"].toString();
    profile.personaName = json["personaName"].toString();
    profile.profileUrl = json["profileUrl"].toString();
    profile.avatarUrl = json["avatarUrl"].toString();
    profile.countryCode = json["countryCode"].toString();
    
    return true;
}

QString SteamApiManager::getUserProfileCachePath(const QString& steamId) const
{
    return getCachePath() + "/userprofiles/" + steamId + ".json";
}

// Update parseWorkshopItem to initiate user profile fetching
WorkshopItemInfo SteamApiManager::parseWorkshopItem(const QJsonObject& itemObject)
{
    WorkshopItemInfo info;
    
    info.itemId = itemObject.value("publishedfileid").toString();  // Changed from info.id = itemObject.value(...)
    info.title = itemObject.value("title").toString();
    info.description = itemObject.value("description").toString();
    info.creator = itemObject.value("creator").toString();
    info.previewUrl = itemObject.value("preview_url").toString();
    info.fileSize = itemObject.value("file_size").toVariant().toLongLong();
    info.views = itemObject.value("views").toInt();
    info.subscriptions = itemObject.value("subscriptions").toInt();
    info.favorites = itemObject.value("favorited").toInt();
    
    // Extract created and updated times
    qint64 created = itemObject.value("time_created").toVariant().toLongLong();
    qint64 updated = itemObject.value("time_updated").toVariant().toLongLong();
    
    if (created > 0) {
        info.created = QDateTime::fromSecsSinceEpoch(created);
    }
    
    if (updated > 0) {
        info.updated = QDateTime::fromSecsSinceEpoch(updated);
    }
    
    // Extract tags
    QJsonArray tagsArray = itemObject.value("tags").toArray();
    for (const QJsonValue& tagValue : tagsArray) {
        QJsonObject tagObj = tagValue.toObject();
        QString tag = tagObj.value("tag").toString();
        if (!tag.isEmpty()) {
            info.tags.append(tag);
            
            // Check for special tags
            QString tagLower = tag.toLower();
            if (tagLower == "scene" || tagLower == "video" || tagLower == "web") {
                info.type = tag;
            } else if (tagLower.startsWith("genre:")) {
                info.genre = tag.mid(6).trimmed(); // Remove "genre:" prefix
            }
        }
    }
    
    // Check if we have creator info and automatically fetch user profile
    if (!info.creator.isEmpty()) {
        // Check if we already have this user's profile cached
        if (hasCachedUserProfile(info.creator)) {
            SteamUserProfile profile = getCachedUserProfile(info.creator);
            info.creatorName = profile.personaName;
        } else {
            // Fetch user profile asynchronously - it will be updated later
            QTimer::singleShot(0, this, [this, creatorId = info.creator]() {
                fetchUserProfile(creatorId);
            });
        }
    }
    
    return info;
}

QString SteamApiManager::getCachePath() const
{
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cachePath.isEmpty()) {
        cachePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    }
    
    if (cachePath.isEmpty()) {
        cachePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + 
                  "/.cache/wallpaperengine-gui";
    } else {
        cachePath += "/wallpaperengine-gui";
    }
    
    return cachePath + "/steam_api";
}

QString SteamApiManager::getItemCachePath(const QString& itemId) const
{
    return getCachePath() + "/" + itemId + ".json";
}

bool SteamApiManager::checkForUpdates(WorkshopItemInfo& info)
{
    WorkshopItemInfo cachedInfo;
    if (!loadFromCache(info.id(), cachedInfo)) {
        // No cached data, no update
        info.hasUpdate = false;
        return false;
    }
    
    // Check if updated timestamp has changed
    if (info.updated.isValid() && cachedInfo.updated.isValid() && 
        info.updated > cachedInfo.updated) {
        qCDebug(steamApi) << "Update detected for item" << info.id();
        qCDebug(steamApi) << "Old update time:" << cachedInfo.updated.toString();
        qCDebug(steamApi) << "New update time:" << info.updated.toString();
        info.hasUpdate = true;
        return true;
    }
    
    info.hasUpdate = false;
    return false;
}
