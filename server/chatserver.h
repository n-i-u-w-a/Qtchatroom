#pragma once

#include <QTcpServer>
#include <QReadWriteLock>
#include <QHash>
#include <QSqlDatabase>

class ClientHandler;

class ChatServer : public QTcpServer {
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);
    ~ChatServer() override;

    bool start(quint16 port);
    void stop();

    bool registerUser(ClientHandler *handler, const QString &username,
                      const QString &password, const QString &question,
                      const QString &answer);
    bool loginUser(ClientHandler *handler, const QString &username,
                   const QString &password);
    void handleForgotPassword(ClientHandler *handler, const QString &username);
    void handleResetPassword(ClientHandler *handler, const QString &username,
                             const QString &answer, const QString &newPassword);
    void unregisterUser(ClientHandler *handler, const QString &username);

    // Group chat
    void createGroup(ClientHandler *handler, const QString &name);
    void joinGroup(ClientHandler *handler, int groupId);
    void leaveGroup(ClientHandler *handler, int groupId);
    void sendGroupMessage(ClientHandler *sender, int groupId, const QString &content,
                          const QString &timestamp);
    void listGroups(ClientHandler *handler);
    void listGroupMembers(ClientHandler *handler, int groupId);

    // Friend system
    void searchUsers(ClientHandler *handler, const QString &query);
    void sendFriendRequest(ClientHandler *from, const QString &to);
    void respondFriendRequest(ClientHandler *handler, const QString &from, const QString &action); // accept/ignore/reject
    void listFriends(ClientHandler *handler);
    void setFriendGroup(ClientHandler *handler, const QString &friendName, const QString &groupName);
    void createFriendGroup(ClientHandler *handler, const QString &groupName);
    void renameFriendGroup(ClientHandler *handler, const QString &oldName, const QString &newName);
    void deleteFriendGroup(ClientHandler *handler, const QString &groupName);
    void listFriendGroups(ClientHandler *handler);
    void listPendingRequests(ClientHandler *handler);

    void broadcast(const QByteArray &message, ClientHandler *exclude = nullptr);
    void sendToUser(const QString &username, const QByteArray &message);
    void sendToUsers(const QStringList &usernames, const QByteArray &message);
    void deleteUser(ClientHandler *handler);
    void sendUserList(ClientHandler *to = nullptr);
    void removeHandler(ClientHandler *handler);

signals:
    void logMessage(const QString &msg);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    void initDatabase();
    void upgradeDatabase();
    QString hashPassword(const QString &password, const QString &salt) const;
    QString generateSalt() const;

    QList<ClientHandler *> m_clients;
    QHash<QString, ClientHandler *> m_usernames;
    QReadWriteLock m_lock;
    QSqlDatabase m_db;
};
