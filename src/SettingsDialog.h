#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QScrollArea>

class ConfigManager;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void browseEnginePath();
    void testEnginePath();
    void detectSteam();
    void browseSteamPath();
    void browseSteamLibrary();
    void addSteamLibrary();
    void removeSteamLibrary();
    void onSteamLibraryChanged();
    void resetToDefaults();
    
    // Steam API slots
    void testApiKey();
    void saveApiKey();
    void toggleApiKeyVisibility(bool show);
    void onApiKeyTestSucceeded();
    void onApiKeyTestFailed(const QString& error);

private:
    void setupUI();
    QWidget* createPathsTab();
    QWidget* createApiTab();       // Method for Steam API tab
    QWidget* createThemeTab();     // Method for Theme tab
    void loadSettings();
    void saveSettings();
    void updateSteamStatus();
    void updateSteamLibraryList();
    void accept() override;

    // UI Components - only keeping Paths tab components
    QLineEdit* m_enginePathEdit;
    QLineEdit* m_assetsDirEdit;
    QLineEdit* m_steamPathEdit;
    QLabel* m_steamStatusLabel;
    QListWidget* m_steamLibraryList;
    QPushButton* m_addLibraryButton;
    QPushButton* m_removeLibraryButton;
    QPushButton* m_browseLibraryButton;
    
    // Steam API tab components
    QLineEdit* m_apiKeyEdit;
    QCheckBox* m_useApiCheckbox;
    QLabel* m_apiStatusLabel;
    QPushButton* m_testApiButton;
    QLabel* m_testResultLabel;
    QCheckBox* m_showApiKeyCheckbox;
    
    // Theme tab components
    QComboBox* m_themeComboBox;
    QLabel* m_themePreviewLabel;
    
    // Configuration
    ConfigManager& m_config;
};

#endif // SETTINGSDIALOG_H
