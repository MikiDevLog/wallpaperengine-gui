#include "SingleApplication.h"

#include <QLoggingCategory>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QLocalSocket>
#include <QAbstractSocket>
#include <unistd.h>
#include <pwd.h>

Q_LOGGING_CATEGORY(saLog, "app.singleapplication")

static QString getUserName()
{
    struct passwd *pw = getpwuid(geteuid());
    return pw ? QString(pw->pw_name) : QString::number(geteuid());
}

SingleApplication::SingleApplication(QObject *parent)
    : QObject(parent)
{
    // Build a unique server name using a stable internal identifier rather than
    // qApp->applicationName(), which can differ between build-dir and installed
    // runs (setApplicationName vs. WMClass/desktop file).  This keeps single-
    // instance detection working regardless of how the app is launched.
    const QString appId = "wallpaperengine-gui";

    m_serverName = QCryptographicHash::hash(
            (appId + ":" + getUserName()).toUtf8(),
            QCryptographicHash::Sha256)
            .toBase64();

    // Attempt to connect to an already-running instance.
    QLocalSocket socket;
    socket.connectToServer(m_serverName, QIODevice::ReadWrite);

    // Use SIGNAL() macro to explicitly target the error signal (not the getter).
    QEventLoop loop;
    QObject::connect(&socket, &QLocalSocket::connected, &loop, &QEventLoop::quit);
    QObject::connect(&socket, SIGNAL(error(QLocalSocket::LocalSocketError)), &loop, SLOT(quit()));

    if (socket.waitForConnected(200)) {
        // Another instance is running — we are the second.
        m_isFirstInstance = false;
        qCDebug(saLog) << "Another instance detected, sending focus request";
        socket.write("focus\n");
        socket.flush();
        // Give the server a moment to receive the data before we exit.
        socket.waitForBytesWritten(100);
        socket.disconnectFromServer();
    } else {
        // No existing instance — become the primary.
        m_isFirstInstance = true;

        if (m_server) {
            delete m_server;
            m_server = nullptr;
        }

        m_server = new QLocalServer(this);

        // Clean up stale socket files from previous crashes using Qt's API.
        QLocalServer::removeServer(m_serverName);

        if (!m_server->listen(m_serverName) &&
            m_server->serverError() == QAbstractSocket::AddressInUseError) {
            // Race condition: another process may have beaten us to it.
            qCWarning(saLog) << "Failed to listen on local server — another instance exists";
            m_isFirstInstance = false;
        }

        if (m_server->isListening()) {
            QObject::connect(m_server, &QLocalServer::newConnection,
                             this, &SingleApplication::handleNewConnection);
            qCDebug(saLog) << "Local server listening — first instance";
        } else {
            qCWarning(saLog) << "Failed to listen on local server:"
                             << m_server->errorString();
            delete m_server;
            m_server = nullptr;
        }
    }
}

SingleApplication::~SingleApplication()
{
    if (m_server) {
        QLocalServer::removeServer(m_serverName);
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
}

bool SingleApplication::isFirstInstance() const
{
    return m_isFirstInstance;
}

void SingleApplication::handleNewConnection()
{
    if (!m_server) return;

    QLocalSocket *socket = m_server->nextPendingConnection();
    if (!socket) return;

    QObject::connect(socket, &QLocalSocket::readyRead, [this, socket]() {
        QByteArray data = socket->readAll();
        QString message = QString::fromUtf8(data.trimmed());
        qCDebug(saLog) << "Message from another instance:" << message;
        emit messageReceived(message);

        // Clean up after the message is consumed.
        socket->deleteLater();
    });

    QObject::connect(socket, &QLocalSocket::disconnected,
                     socket, &QLocalSocket::deleteLater);
}
