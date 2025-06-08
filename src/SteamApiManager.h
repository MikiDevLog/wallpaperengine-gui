#ifndef STEAMAPIMANAGER_H
#define STEAMAPIMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QDateTime>
#include <QStringList>

// Structure to hold Steam workshop item metadata
struct WorkshopItemInfo {
    QString itemId;
    QString title;
    QString description;
    QString creator;
    QString creatorName;
    QString type;
    QString genre;        // Added missing genre field
    bool hasUpdate = false; // Added missing hasUpdate field
    QDateTime created;
    QDateTime updated;
    qint64 fileSize = 0;
    int views = 0;
    int subscriptions = 0;
    int favorites = 0;
    QStringList tags;
    QString previewUrl;
    
    // Use getter function instead of reference for backward compatibility
    const QString& id() const { return itemId; }
    
    WorkshopItemInfo() = default;
};

// New structure for Steam user profile information
struct SteamUserProfile {
    QString steamId;
    QString personaName;
    QString profileUrl;
    QString avatarUrl;
    QString countryCode;  // Added missing countryCode field
    
    SteamUserProfile() = default;
};

class SteamApiManager : public QObject
{
    Q_OBJECT

public:
    static SteamApiManager& instance();
    
    // API key management
    bool setApiKey(const QString& apiKey);
    QString apiKey() const;
    bool hasApiKey() const;
    
    // Test the API key with a specific item ID
    void testApiKey(const QString& itemId);
    
    // Fetch item details
    void fetchItemDetails(const QString& itemId);
    void fetchItemDetails(const QStringList& itemIds);
    
    // New methods to fetch user profile information
    void fetchUserProfile(const QString& steamId);
    void fetchUserProfiles(const QStringList& steamIds);
    bool hasCachedUserProfile(const QString& steamId) const;
    SteamUserProfile getCachedUserProfile(const QString& steamId) const;
    
    // Check for updates (compare with cache)
    bool hasUpdates(const QString& itemId) const;
    
    // Get cached item info
    WorkshopItemInfo getCachedItemInfo(const QString& itemId) const;
    bool hasCachedInfo(const QString& itemId) const;
    
    // Save/load API results to/from cache
    bool saveToCache(const WorkshopItemInfo& info);
    bool loadFromCache(const QString& itemId, WorkshopItemInfo& info) const;
    
    // User profile cache management
    bool saveUserProfileToCache(const SteamUserProfile& profile);
    bool loadUserProfileFromCache(const QString& steamId, SteamUserProfile& profile) const;
    
    // Get all cached item IDs
    QStringList getAllCachedItemIds() const;
    
    // Clear cache
    void clearCache();

signals:
    void apiKeyTestSucceeded();
    void apiKeyTestFailed(const QString& error);
    void itemDetailsReceived(const QString& itemId, const WorkshopItemInfo& info);
    void itemDetailsError(const QString& itemId, const QString& error);
    void batchDetailsCompleted();
    
    // New signal for user profile information
    void userProfileReceived(const QString& steamId, const SteamUserProfile& profile);
    void userProfileError(const QString& steamId, const QString& error);

private:
    explicit SteamApiManager(QObject* parent = nullptr);
    ~SteamApiManager();
    
    // Prevent copying
    SteamApiManager(const SteamApiManager&) = delete;
    SteamApiManager& operator=(const SteamApiManager&) = delete;
    
    // Helper methods
    QJsonObject parseApiResponse(const QByteArray& response, bool& ok, QString& errorMsg);
    WorkshopItemInfo parseWorkshopItem(const QJsonObject& itemObject);
    SteamUserProfile parseUserProfile(const QJsonObject& profileObject);
    QString getCachePath() const;
    QString getItemCachePath(const QString& itemId) const;
    QString getUserProfileCachePath(const QString& steamId) const;
    
    // Compare with cached data to detect updates
    bool checkForUpdates(WorkshopItemInfo& info);
    
    QNetworkAccessManager* m_networkManager;
    QString m_apiKey;
    QMap<QString, WorkshopItemInfo> m_itemCache;
    QMap<QString, SteamUserProfile> m_userProfileCache;
    int m_pendingRequests;
};

#endif // STEAMAPIMANAGER_H
