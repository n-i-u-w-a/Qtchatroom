#pragma once

#include <QObject>
#include <QTcpSocket>

class ChatClient : public QObject {
    Q_OBJECT
public:
    explicit ChatClient(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    void sendLogin(const QString &username, const QString &password);
    void sendRegister(const QString &username, const QString &password,
                      const QString &question, const QString &answer);
    void sendForgotPassword(const QString &username);
    void sendResetPassword(const QString &username, const QString &answer,
                           const QString &newPassword);
    void sendBroadcast(const QString &content);
    void sendPrivate(const QString &to, const QString &content);
    void sendGroup(const QStringList &to, const QString &content);
    void sendDeleteAccount();

    QString username() const { return m_username; }

signals:
    void connected();
    void disconnected();
    void loginSuccess(const QString &username);
    void loginError(const QString &error);
    void securityQuestionReceived(const QString &username, const QString &question);
    void forgotError(const QString &error);
    void passwordResetOk(const QString &message);
    void resetError(const QString &error);
    void userListReceived(const QStringList &users);
    void messageReceived(const QString &sender, const QString &content,
                         const QString &timestamp, const QString &scope,
                         const QString &to);
    void systemMessage(const QString &content);
    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketConnected();
    void onSocketDisconnected();

private:
    void processLine(const QByteArray &line);

    QTcpSocket *m_socket;
    QByteArray m_buffer;
    QString m_username;
};
