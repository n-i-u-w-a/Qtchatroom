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
    void sendTyping(const QString &to);

    // Group chat
    void sendCreateGroup(const QString &name);
    void sendJoinGroup(int groupId);
    void sendLeaveGroup(int groupId);
    void sendGroupMessage(int groupId, const QString &content);
    void sendListGroups();
    void sendGroupMembers(int groupId);

    // Friend system
    void sendSearchUsers(const QString &query);
    void sendFriendRequest(const QString &to);
    void sendFriendResponse(const QString &from, const QString &action); // accept/ignore/reject
    void sendFriendList();
    void sendSetFriendGroup(const QString &friendName, const QString &groupName);
    void sendCreateFriendGroup(const QString &groupName);
    void sendRenameFriendGroup(const QString &oldName, const QString &newName);
    void sendDeleteFriendGroup(const QString &groupName);
    void sendListFriendGroups();
    void sendListPendingRequests();

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

    // Group signals
    void groupCreated(int groupId, const QString &name);
    void groupJoined(int groupId, const QString &name);
    void groupLeft(int groupId);
    void groupListReceived(const QJsonArray &groups);
    void groupMembersReceived(int groupId, const QStringList &members);
    void groupError(const QString &error);

    // Friend signals
    void searchResults(const QString &query, const QStringList &users);
    void friendRequestSent(const QString &to);
    void friendRequestReceived(const QString &from);
    void friendAdded(const QString &friendName);
    void friendRejected(const QString &friendName);
    void friendListReceived(const QJsonArray &friends);
    void friendGroupSet(const QString &friendName, const QString &group);
    void friendGroupCreated(const QString &groupName);
    void friendGroupRenamed(const QString &oldName, const QString &newName);
    void friendGroupDeleted(const QString &groupName);
    void friendGroupsReceived(const QJsonArray &groups);
    void pendingRequestsReceived(const QStringList &from);
    void friendIgnored(const QString &from);
    void typingReceived(const QString &from);
    void friendError(const QString &error);

private slots:
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketConnected();
    void onSocketDisconnected();

private:
    void processLine(const QByteArray &line);
    void writeJson(const QJsonObject &msg);

    QTcpSocket *m_socket;
    QByteArray m_buffer;
    QString m_username;
};
