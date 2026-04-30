#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>

class ClientHandler : public QObject {
    Q_OBJECT
public:
    explicit ClientHandler(qintptr socketDescriptor, QObject *parent = nullptr);
    ~ClientHandler() override;

    QString username() const;
    void setUsername(const QString &name);
    void sendMessage(const QByteArray &message);
    void disconnectClient();
    bool isLoggedIn() const;

signals:
    void loginRequested(ClientHandler *handler, const QString &username,
                        const QString &password);
    void registerRequested(ClientHandler *handler, const QString &username,
                           const QString &password, const QString &question,
                           const QString &answer);
    void forgotPasswordRequested(ClientHandler *handler, const QString &username);
    void resetPasswordRequested(ClientHandler *handler, const QString &username,
                                const QString &answer, const QString &newPassword);
    void deleteAccountRequested(ClientHandler *handler);
    void messageForRoute(const QByteArray &message, const QString &target,
                         const QStringList &targets);
    void disconnected(ClientHandler *handler);

private slots:
    void onReadyRead();
    void onDisconnected();

    void processMessage(const QByteArray &line);

private:
    QTcpSocket *m_socket;
    QString m_username;
    bool m_loggedIn = false;
    QByteArray m_buffer;
};
