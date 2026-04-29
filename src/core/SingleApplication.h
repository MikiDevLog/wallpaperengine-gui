#ifndef SINGLEAPPLICATION_H
#define SINGLEAPPLICATION_H

#include <QObject>
#include <QLocalServer>
#include <QString>

class SingleApplication : public QObject {
    Q_OBJECT
public:
    explicit SingleApplication(QObject *parent = nullptr);
    ~SingleApplication();

    bool isFirstInstance() const;

signals:
    void messageReceived(const QString &message);

private slots:
    void handleNewConnection();

private:
    QString m_serverName;
    QLocalServer *m_server = nullptr;
    bool m_isFirstInstance = false;
};

#endif // SINGLEAPPLICATION_H
