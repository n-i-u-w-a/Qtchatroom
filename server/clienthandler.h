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

    // Group chat
    void createGroupRequested(ClientHandler *handler, const QString &name);
    void joinGroupRequested(ClientHandler *handler, int groupId);
    void leaveGroupRequested(ClientHandler *handler, int groupId);
    void groupMessageRequested(ClientHandler *handler, int groupId,
                               const QString &content, const QString &timestamp);
    void listGroupsRequested(ClientHandler *handler);
    void listGroupMembersRequested(ClientHandler *handler, int groupId);

    // Friend system
    void searchUsersRequested(ClientHandler *handler, const QString &query);
    void friendRequestRequested(ClientHandler *handler, const QString &to);
    void friendResponseRequested(ClientHandler *handler, const QString &from, const QString &action);
    void friendListRequested(ClientHandler *handler);
    void setFriendGroupRequested(ClientHandler *handler, const QString &friendName,
                                 const QString &groupName);
    void createFriendGroupRequested(ClientHandler *handler, const QString &groupName);
    void renameFriendGroupRequested(ClientHandler *handler, const QString &oldName,
                                    const QString &newName);
    void deleteFriendGroupRequested(ClientHandler *handler, const QString &groupName);
    void listFriendGroupsRequested(ClientHandler *handler);
    void listPendingRequestsRequested(ClientHandler *handler);

    void messageForRoute(const QByteArray &message, const QString &target,
                         const QStringList &targets);
    void typingRelayRequested(const QString &to, const QByteArray &data);
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
