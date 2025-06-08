#include <QApplication>
#include <QMessageBox>
#include <QStyleFactory>
#include <QDir>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <unistd.h>
#include <pwd.h>
#include <cstdlib>
#include "MainWindow.h"
#include "ConfigManager.h"

// Logging categories
Q_LOGGING_CATEGORY(appMain, "app.main")

static void setupLogging()
{
    // Set up logging patterns
    qSetMessagePattern("[%{time hh:mm:ss.zzz}] [%{category}] %{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}%{if-critical}C%{endif}: %{message}");
    
    // Enable debug logging for our app but filter out Qt noise
    QLoggingCategory::setFilterRules(
        "app.*.debug=true\n"
        "qt.*.debug=false\n"
        "qt.quick.*.debug=false\n"
        "qt.qpa.*.debug=false\n"
        "qt.widgets.*.debug=false\n"
        "qt.core.*.debug=false\n"
        "qt.gui.*.debug=false\n"
        "qt.accessibility.*.debug=false\n"
        "*.debug=false"
    );
    
    qDebug() << "Debug logging enabled with Qt noise filtering";
}

static bool checkSudoStatus()
{
    // Check if running as root
    if (getuid() == 0) {
        return false;
    }
    
    // Check for SUDO_UID environment variable (indicates sudo was used)
    const char* sudoUid = std::getenv("SUDO_UID");
    if (sudoUid != nullptr) {
        return false;
    }
    
    // Check for SUDO_USER environment variable
    const char* sudoUser = std::getenv("SUDO_USER");
    if (sudoUser != nullptr) {
        return false;
    }
    
    return true;
}

static void showSudoWarning()
{
    int argc = 0;
    QApplication app(argc, nullptr);
    app.setQuitOnLastWindowClosed(true);
    
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setWindowTitle("Wallpaper Engine GUI - Permission Error");
    msgBox.setText("This application should not be run with sudo/root privileges!");
    msgBox.setInformativeText(
        "Running GUI applications as root can be dangerous and is not supported.\n\n"
        "Please run this application as a normal user:\n"
        "$ wallpaperengine-gui\n\n"
        "The application stores its configuration in your user home directory "
        "and does not require elevated privileges.");
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

static void createConfigDirectory()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    configDir += "/wallpaperengine-gui";
    
    QDir dir;
    if (!dir.exists(configDir)) {
        if (!dir.mkpath(configDir)) {
            qCCritical(appMain) << "Failed to create config directory:" << configDir;
        } else {
            qCInfo(appMain) << "Created config directory:" << configDir;
        }
    }
}

static void setupApplicationMetadata()
{
    QApplication::setApplicationName("Wallpaper Engine GUI");
    QApplication::setApplicationDisplayName("Wallpaper Engine GUI");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("WallpaperEngine");
    QApplication::setOrganizationDomain("wallpaperengine.io");
    QApplication::setDesktopFileName("wallpaperengine-gui");
}

static void setupApplicationStyle(QApplication &app)
{
    ConfigManager &config = ConfigManager::instance();
    QString configuredTheme = config.theme();
    
    // Apply configured theme if available
    if (!configuredTheme.isEmpty() && configuredTheme != "System Default") {
        if (QStyleFactory::keys().contains(configuredTheme)) {
            app.setStyle(QStyleFactory::create(configuredTheme));
            qCInfo(appMain) << "Applied user-configured theme:" << configuredTheme;
        } else {
            qCWarning(appMain) << "Configured theme" << configuredTheme << "not available, using system default";
        }
    } else {
        // Try to use a modern style
        QStringList preferredStyles = {"Fusion", "Windows", "GTK+"};
        
        for (const QString &style : preferredStyles) {
            if (QStyleFactory::keys().contains(style, Qt::CaseInsensitive)) {
                app.setStyle(QStyleFactory::create(style));
                qCInfo(appMain) << "Using style:" << style;
                break;
            }
        }
    }
    
    // Don't override the system palette - let it inherit naturally
    // The system will provide the correct colors based on the user's theme
    qCInfo(appMain) << "Using system palette for theme compatibility";
}

int main(int argc, char *argv[])
{
    // Check sudo status before creating QApplication
    if (!checkSudoStatus()) {
        showSudoWarning();
        return 1;
    }
    
    QApplication app(argc, argv);
    
    // Set up logging FIRST
    setupLogging();
    
    qDebug() << "Application created, setting up metadata";
    
    // Set up application metadata
    setupApplicationMetadata();
    
    qCInfo(appMain) << "Starting" << QApplication::applicationDisplayName() 
                    << "version" << QApplication::applicationVersion();
    
    // Install improved message handler that filters Qt noise
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        // Filter out noisy Qt debug messages
        QString category = context.category ? QString(context.category) : QString();
        
        // Skip Qt internal debug messages that are too verbose
        if (type == QtDebugMsg) {
            if (category.startsWith("qt.") || 
                msg.contains("QWidget::") ||
                msg.contains("QEvent::") ||
                msg.contains("QMouseEvent") ||
                msg.contains("QHoverEvent") ||
                msg.contains("QMoveEvent") ||
                msg.contains("QResizeEvent") ||
                msg.contains("QEnterEvent") ||
                msg.contains("QLeaveEvent") ||
                msg.contains("QPaintEvent") ||
                msg.contains("QShowEvent") ||
                msg.contains("QHideEvent") ||
                msg.contains("focus") ||
                msg.contains("hover") ||
                msg.contains("geometry") ||
                msg.contains("palette") ||
                msg.contains("style") ||
                msg.contains("QStyleOption") ||
                msg.contains("QStylePainter") ||
                msg.contains("QOpenGLContext") ||
                msg.contains("QSurface") ||
                msg.contains("GLX") ||
                msg.contains("XCB") ||
                (msg.contains("Wayland") && !msg.contains("ERROR") && !msg.contains("WARNING"))) {
                return; // Skip these messages
            }
        }
        
        QString txt;
        switch (type) {
        case QtDebugMsg:
            txt = QString("Debug: %1").arg(msg);
            break;
        case QtWarningMsg:
            txt = QString("Warning: %1").arg(msg);
            break;
        case QtCriticalMsg:
            txt = QString("Critical: %1").arg(msg);
            break;
        case QtFatalMsg:
            txt = QString("Fatal: %1").arg(msg);
            abort();
        case QtInfoMsg:
            txt = QString("Info: %1").arg(msg);
            break;
        }
        
        // Print to console for important messages
        if (type >= QtWarningMsg || category.startsWith("app.")) {
            fprintf(stderr, "%s\n", txt.toLocal8Bit().constData());
            fflush(stderr);
        }
        
        // Write to log file (but still filter some noise)
        if (type >= QtInfoMsg || category.startsWith("app.")) {
            static QFile logFile(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/wallpaperengine-gui/debug.log");
            if (!logFile.isOpen()) {
                logFile.open(QIODevice::WriteOnly | QIODevice::Append);
            }
            if (logFile.isOpen()) {
                QTextStream stream(&logFile);
                stream << QDateTime::currentDateTime().toString() << " " << txt << Qt::endl;
                stream.flush();
            }
        }
    });
    
    qDebug() << "Improved message handler installed";
    
    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("GUI for linux-wallpaperengine");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption debugOption(QStringList() << "d" << "debug",
        "Enable debug output");
    parser.addOption(debugOption);
    
    QCommandLineOption configOption(QStringList() << "c" << "config",
        "Use custom config file", "config");
    parser.addOption(configOption);
    
    QCommandLineOption minimizedOption(QStringList() << "m" << "minimized",
        "Start minimized to system tray");
    parser.addOption(minimizedOption);
    
    parser.process(app);
    
    qDebug() << "Command line arguments processed";
    
    // Enable debug logging if requested
    if (parser.isSet(debugOption)) {
        QLoggingCategory::setFilterRules("*.debug=true");
        qCInfo(appMain) << "All debug logging enabled via command line";
    }
    
    qDebug() << "Creating config directory";
    // Create necessary directories
    createConfigDirectory();
    
    qDebug() << "Setting up application style";
    // Set up application style
    setupApplicationStyle(app);
    
    qDebug() << "Initializing ConfigManager";
    // Initialize configuration
    ConfigManager &config = ConfigManager::instance();
    
    // Use custom config file if specified
    if (parser.isSet(configOption)) {
        QString configFile = parser.value(configOption);
        qCInfo(appMain) << "Using custom config file:" << configFile;
    }
    
    qCInfo(appMain) << "Config directory:" << config.configDir();
    qCInfo(appMain) << "Using Qt version:" << qVersion();
    
    qDebug() << "Creating main window";
    // Create and show main window
    MainWindow window;
    
    // Check if application should start minimized
    bool startMinimized = parser.isSet(minimizedOption);
    window.setStartMinimized(startMinimized);
    
    qDebug() << "Showing main window" << (startMinimized ? "(minimized)" : "");
    if (!startMinimized) {
        window.show();
    } else {
        qCInfo(appMain) << "Starting minimized to system tray";
    }
    
    qCInfo(appMain) << "Application started successfully";
    
    qDebug() << "Starting event loop";
    // Run application event loop
    int result = app.exec();
    
    qCInfo(appMain) << "Application exiting with code:" << result;
    return result;
}
