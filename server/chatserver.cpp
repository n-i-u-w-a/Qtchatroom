#include "chatserver.h"
#include "clienthandler.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QRandomGenerator>

ChatServer::ChatServer(QObject *parent)
    : QTcpServer(parent)
{
    initDatabase();
}

ChatServer::~ChatServer()
{
    if (m_db.isOpen())
        m_db.close();
}

void ChatServer::initDatabase()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName("chat.db");

    if (!m_db.open()) {
        qCritical() << "Failed to open database:" << m_db.lastError().text();
        return;
    }

    QSqlQuery query(m_db);
    query.exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "  id       INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username TEXT    NOT NULL UNIQUE,"
        "  password TEXT    NOT NULL,"
        "  question TEXT    DEFAULT '',"
        "  answer   TEXT    DEFAULT '',"
        "  created  TEXT    DEFAULT (datetime('now'))"
        ")");

    // Group chat tables
    query.exec(
        "CREATE TABLE IF NOT EXISTS chat_groups ("
        "  id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name    TEXT    NOT NULL,"
        "  owner   TEXT    NOT NULL,"
        "  created TEXT    DEFAULT (datetime('now'))"
        ")");
    query.exec(
        "CREATE TABLE IF NOT EXISTS group_members ("
        "  group_id INTEGER,"
        "  username TEXT,"
        "  joined   TEXT DEFAULT (datetime('now')),"
        "  PRIMARY KEY (group_id, username)"
        ")");

    // Friend system tables
    query.exec(
        "CREATE TABLE IF NOT EXISTS friends ("
        "  username   TEXT,"
        "  friend     TEXT,"
        "  group_name TEXT DEFAULT '',"
        "  created    TEXT DEFAULT (datetime('now')),"
        "  PRIMARY KEY (username, friend)"
        ")");
    query.exec(
        "CREATE TABLE IF NOT EXISTS friend_groups ("
        "  username   TEXT,"
        "  group_name TEXT,"
        "  created    TEXT DEFAULT (datetime('now')),"
        "  PRIMARY KEY (username, group_name)"
        ")");
    query.exec(
        "CREATE TABLE IF NOT EXISTS friend_requests ("
        "  from_user TEXT,"
        "  to_user   TEXT,"
        "  status    TEXT DEFAULT 'pending',"
        "  created   TEXT DEFAULT (datetime('now')),"
        "  PRIMARY KEY (from_user, to_user)"
        ")");

    upgradeDatabase();

    emit logMessage("Database initialized (chat.db)");
}

void ChatServer::upgradeDatabase()
{
    QSqlQuery q(m_db);
    q.exec("SELECT question FROM users LIMIT 1");
    if (q.lastError().isValid()) {
        q.exec("ALTER TABLE users ADD COLUMN question TEXT DEFAULT ''");
        q.exec("ALTER TABLE users ADD COLUMN answer TEXT DEFAULT ''");
        emit logMessage("Database upgraded with security question columns");
    }
    q.exec("SELECT group_name FROM friends LIMIT 1");
    if (q.lastError().isValid()) {
        q.exec("ALTER TABLE friends ADD COLUMN group_name TEXT DEFAULT ''");
        emit logMessage("Database upgraded with friend group column");
    }
}

QString ChatServer::generateSalt() const
{
    QByteArray salt;
    salt.resize(16);
    for (int i = 0; i < 16; ++i)
        salt[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    return salt.toHex();
}

QString ChatServer::hashPassword(const QString &password, const QString &salt) const
{
    QByteArray data = (salt + password).toUtf8();
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

bool ChatServer::start(quint16 port)
{
    if (!listen(QHostAddress::Any, port)) {
        emit logMessage(QString("Failed to start: %1").arg(errorString()));
        return false;
    }
    emit logMessage(QString("Server listening on port %1").arg(port));
    return true;
}

void ChatServer::stop()
{
    close();
    QList<ClientHandler *> snapshot;
    { QWriteLocker locker(&m_lock); snapshot = m_clients; m_clients.clear(); m_usernames.clear(); }
    for (auto *h : snapshot) h->disconnectClient();
    emit logMessage("Server stopped");
}

void ChatServer::incomingConnection(qintptr socketDescriptor)
{
    auto *handler = new ClientHandler(socketDescriptor, this);

    connect(handler, &ClientHandler::loginRequested, this,
            [this, handler](ClientHandler *h, const QString &username,
                            const QString &password) {
                QJsonObject resp;
                if (loginUser(h, username, password)) {
                    resp["type"] = "login_ok";
                    resp["username"] = username;
                } else {
                    resp["type"] = "login_error";
                    resp["content"] = "Invalid username or password";
                }
                h->sendMessage(QJsonDocument(resp).toJson(QJsonDocument::Compact));
            });

    connect(handler, &ClientHandler::registerRequested, this,
            [this, handler](ClientHandler *h, const QString &username,
                            const QString &password, const QString &question,
                            const QString &answer) {
                QJsonObject resp;
                if (registerUser(h, username, password, question, answer)) {
                    resp["type"] = "login_ok";
                    resp["username"] = username;
                } else {
                    resp["type"] = "login_error";
                    resp["content"] = QString("Username '%1' is invalid or already taken")
                                          .arg(username);
                }
                h->sendMessage(QJsonDocument(resp).toJson(QJsonDocument::Compact));
            });

    connect(handler, &ClientHandler::forgotPasswordRequested,
            this, &ChatServer::handleForgotPassword);

    connect(handler, &ClientHandler::resetPasswordRequested,
            this, &ChatServer::handleResetPassword);

    connect(handler, &ClientHandler::deleteAccountRequested,
            this, &ChatServer::deleteUser);

    // Group chat
    connect(handler, &ClientHandler::createGroupRequested, this, &ChatServer::createGroup);
    connect(handler, &ClientHandler::joinGroupRequested, this, &ChatServer::joinGroup);
    connect(handler, &ClientHandler::leaveGroupRequested, this, &ChatServer::leaveGroup);
    connect(handler, &ClientHandler::groupMessageRequested, this, &ChatServer::sendGroupMessage);
    connect(handler, &ClientHandler::listGroupsRequested, this, &ChatServer::listGroups);
    connect(handler, &ClientHandler::listGroupMembersRequested, this, &ChatServer::listGroupMembers);

    // Friend system
    connect(handler, &ClientHandler::searchUsersRequested, this, &ChatServer::searchUsers);
    connect(handler, &ClientHandler::friendRequestRequested, this, &ChatServer::sendFriendRequest);
    connect(handler, &ClientHandler::friendResponseRequested, this, &ChatServer::respondFriendRequest);
    connect(handler, &ClientHandler::friendListRequested, this, &ChatServer::listFriends);
    connect(handler, &ClientHandler::setFriendGroupRequested, this, &ChatServer::setFriendGroup);
    connect(handler, &ClientHandler::createFriendGroupRequested, this, &ChatServer::createFriendGroup);
    connect(handler, &ClientHandler::renameFriendGroupRequested, this, &ChatServer::renameFriendGroup);
    connect(handler, &ClientHandler::deleteFriendGroupRequested, this, &ChatServer::deleteFriendGroup);
    connect(handler, &ClientHandler::listFriendGroupsRequested, this, &ChatServer::listFriendGroups);
    connect(handler, &ClientHandler::listPendingRequestsRequested, this, &ChatServer::listPendingRequests);

    connect(handler, &ClientHandler::disconnected, this, &ChatServer::removeHandler);

    connect(handler, &ClientHandler::typingRelayRequested, this,
            [this](const QString &to, const QByteArray &data) {
                sendToUser(to, data);
            });
    connect(handler, &ClientHandler::messageForRoute, this,
            [this, handler](const QByteArray &msg, const QString &target,
                            const QStringList &targets) {
                if (!targets.isEmpty()) sendToUsers(targets, msg);
                else if (!target.isEmpty()) sendToUser(target, msg);
                else broadcast(msg, handler);
            }, Qt::QueuedConnection);

    { QWriteLocker locker(&m_lock); m_clients.append(handler); }
    emit logMessage(QString("New connection from %1").arg(handler->username()));
}

bool ChatServer::registerUser(ClientHandler *handler, const QString &username,
                               const QString &password, const QString &question,
                               const QString &answer)
{
    if (username.isEmpty() || username.contains(' ') || username.length() > 20)
        return false;
    if (password.length() < 4 || password.length() > 64)
        return false;
    if (question.isEmpty() || answer.isEmpty())
        return false;

    QString passSalt = generateSalt();
    QString passHash = hashPassword(password, passSalt);
    QString passStored = passSalt + ":" + passHash;

    QString ansSalt = generateSalt();
    QString ansHash = hashPassword(answer.toLower().trimmed(), ansSalt);
    QString ansStored = ansSalt + ":" + ansHash;

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO users (username, password, question, answer) VALUES (?, ?, ?, ?)");
    query.addBindValue(username);
    query.addBindValue(passStored);
    query.addBindValue(question);
    query.addBindValue(ansStored);

    if (!query.exec()) {
        emit logMessage(QString("Register failed: %1").arg(query.lastError().text()));
        return false;
    }

    { QWriteLocker locker(&m_lock); m_usernames[username] = handler; }
    handler->setUsername(username);

    emit logMessage(QString("[%1] registered & logged in  (%2 online)")
                        .arg(username).arg(m_usernames.size()));

    QJsonObject sysMsg;
    sysMsg["type"] = "system";
    sysMsg["content"] = QString("%1 joined the chat").arg(username);
    broadcast(QJsonDocument(sysMsg).toJson(QJsonDocument::Compact));
    sendUserList();
    return true;
}

bool ChatServer::loginUser(ClientHandler *handler, const QString &username,
                            const QString &password)
{
    if (username.isEmpty() || password.isEmpty()) return false;
    { QReadLocker locker(&m_lock); if (m_usernames.contains(username)) return false; }

    QSqlQuery query(m_db);
    query.prepare("SELECT password FROM users WHERE username = ?");
    query.addBindValue(username);
    if (!query.exec() || !query.next()) return false;

    QString stored = query.value(0).toString();
    QStringList parts = stored.split(':');
    if (parts.size() != 2) return false;
    if (hashPassword(password, parts[0]) != parts[1]) return false;

    { QWriteLocker locker(&m_lock); m_usernames[username] = handler; }
    handler->setUsername(username);

    emit logMessage(QString("[%1] logged in  (%2 online)").arg(username).arg(m_usernames.size()));

    QJsonObject sysMsg;
    sysMsg["type"] = "system";
    sysMsg["content"] = QString("%1 joined the chat").arg(username);
    broadcast(QJsonDocument(sysMsg).toJson(QJsonDocument::Compact));
    sendUserList();
    return true;
}

void ChatServer::handleForgotPassword(ClientHandler *handler, const QString &username)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT question FROM users WHERE username = ?");
    query.addBindValue(username);

    QJsonObject resp;
    if (query.exec() && query.next() && !query.value(0).toString().isEmpty()) {
        resp["type"] = "security_question";
        resp["username"] = username;
        resp["question"] = query.value(0).toString();
    } else {
        resp["type"] = "forgot_error";
        resp["content"] = "User not found or no security question set";
    }
    handler->sendMessage(QJsonDocument(resp).toJson(QJsonDocument::Compact));
}

void ChatServer::handleResetPassword(ClientHandler *handler, const QString &username,
                                      const QString &answer, const QString &newPassword)
{
    if (newPassword.length() < 4 || newPassword.length() > 64) {
        QJsonObject resp;
        resp["type"] = "reset_error";
        resp["content"] = "Password must be 4-64 characters";
        handler->sendMessage(QJsonDocument(resp).toJson(QJsonDocument::Compact));
        return;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT answer FROM users WHERE username = ?");
    query.addBindValue(username);

    QJsonObject resp;
    if (!query.exec() || !query.next()) {
        resp["type"] = "reset_error";
        resp["content"] = "User not found";
        handler->sendMessage(QJsonDocument(resp).toJson(QJsonDocument::Compact));
        return;
    }

    QString stored = query.value(0).toString();
    QStringList parts = stored.split(':');
    if (parts.size() != 2 || hashPassword(answer.toLower().trimmed(), parts[0]) != parts[1]) {
        resp["type"] = "reset_error";
        resp["content"] = "Wrong answer";
        handler->sendMessage(QJsonDocument(resp).toJson(QJsonDocument::Compact));
        return;
    }

    // Update password
    QString passSalt = generateSalt();
    QString passHash = hashPassword(newPassword, passSalt);
    QString passStored = passSalt + ":" + passHash;

    QSqlQuery update(m_db);
    update.prepare("UPDATE users SET password = ? WHERE username = ?");
    update.addBindValue(passStored);
    update.addBindValue(username);
    update.exec();

    resp["type"] = "password_reset_ok";
    resp["content"] = "Password reset successfully. You can now login.";
    handler->sendMessage(QJsonDocument(resp).toJson(QJsonDocument::Compact));

    emit logMessage(QString("[%1] password reset via security question").arg(username));
}

void ChatServer::unregisterUser(ClientHandler *handler, const QString &username)
{
    if (!username.isEmpty()) {
        QWriteLocker locker(&m_lock);
        if (m_usernames.value(username) == handler)
            m_usernames.remove(username);
    }
}

void ChatServer::broadcast(const QByteArray &message, ClientHandler *exclude)
{
    QReadLocker locker(&m_lock);
    for (auto *h : m_clients)
        if (h != exclude && h->isLoggedIn()) h->sendMessage(message);
}

void ChatServer::sendToUser(const QString &username, const QByteArray &message)
{
    QReadLocker locker(&m_lock);
    auto *h = m_usernames.value(username);
    if (h) h->sendMessage(message);
}

void ChatServer::sendToUsers(const QStringList &usernames, const QByteArray &message)
{
    QReadLocker locker(&m_lock);
    for (const auto &name : usernames) {
        auto *h = m_usernames.value(name);
        if (h) h->sendMessage(message);
    }
}

void ChatServer::deleteUser(ClientHandler *handler)
{
    QString name = handler->username();
    if (!handler->isLoggedIn() || name.isEmpty()) return;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM users WHERE username = ?");
    query.addBindValue(name);
    query.exec();

    bool wasLoggedIn = handler->isLoggedIn();
    { QWriteLocker locker(&m_lock); m_clients.removeAll(handler); if (wasLoggedIn && m_usernames.value(name) == handler) m_usernames.remove(name); }

    QJsonObject confirm;
    confirm["type"] = "system";
    confirm["content"] = "Your account has been deleted. Goodbye.";
    handler->sendMessage(QJsonDocument(confirm).toJson(QJsonDocument::Compact));
    handler->disconnectClient();
    handler->deleteLater();

    emit logMessage(QString("[%1] account deleted").arg(name));

    QJsonObject sysMsg;
    sysMsg["type"] = "system";
    sysMsg["content"] = QString("%1 has deleted their account and left").arg(name);
    broadcast(QJsonDocument(sysMsg).toJson(QJsonDocument::Compact));
    sendUserList();
}

// ========== Group Chat ==========

void ChatServer::createGroup(ClientHandler *handler, const QString &name)
{
    if (name.isEmpty() || name.length() > 30) {
        QJsonObject r; r["type"] = "group_error"; r["content"] = "Invalid group name";
        handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
        return;
    }
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO chat_groups (name, owner) VALUES (?, ?)");
    q.addBindValue(name); q.addBindValue(handler->username());
    if (!q.exec()) { QJsonObject r; r["type"] = "group_error"; r["content"] = "Group name exists"; handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact)); return; }
    int gid = q.lastInsertId().toInt();
    q.prepare("INSERT INTO group_members (group_id, username) VALUES (?, ?)");
    q.addBindValue(gid); q.addBindValue(handler->username()); q.exec();

    QJsonObject r; r["type"] = "group_created"; r["group_id"] = gid; r["name"] = name;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
    emit logMessage(QString("Group '%1' created by %2").arg(name, handler->username()));
}

void ChatServer::joinGroup(ClientHandler *handler, int groupId)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT name FROM chat_groups WHERE id = ?");
    q.addBindValue(groupId);
    if (!q.exec() || !q.next()) { QJsonObject r; r["type"] = "group_error"; r["content"] = "Group not found"; handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact)); return; }
    QString name = q.value(0).toString();
    q.prepare("INSERT OR IGNORE INTO group_members (group_id, username) VALUES (?, ?)");
    q.addBindValue(groupId); q.addBindValue(handler->username()); q.exec();

    QJsonObject r; r["type"] = "group_joined"; r["group_id"] = groupId; r["name"] = name;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
}

void ChatServer::leaveGroup(ClientHandler *handler, int groupId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM group_members WHERE group_id = ? AND username = ?");
    q.addBindValue(groupId); q.addBindValue(handler->username()); q.exec();
    QJsonObject r; r["type"] = "group_left"; r["group_id"] = groupId;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
}

void ChatServer::sendGroupMessage(ClientHandler *sender, int groupId,
                                   const QString &content, const QString &timestamp)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT name FROM chat_groups WHERE id = ?");
    q.addBindValue(groupId);
    QString gname = "Group";
    if (q.exec() && q.next()) gname = q.value(0).toString();

    q.prepare("SELECT username FROM group_members WHERE group_id = ?");
    q.addBindValue(groupId);
    QStringList members;
    while (q.next()) members << q.value(0).toString();

    QJsonObject msg;
    msg["type"] = "message"; msg["sender"] = sender->username();
    msg["content"] = content; msg["timestamp"] = timestamp;
    msg["scope"] = "group"; msg["group_id"] = groupId; msg["group_name"] = gname;
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);

    for (const auto &m : members)
        sendToUser(m, data);
}

void ChatServer::listGroups(ClientHandler *handler)
{
    QSqlQuery q(m_db);
    q.exec("SELECT g.id, g.name, g.owner, (SELECT COUNT(*) FROM group_members WHERE group_id=g.id) "
           "FROM chat_groups g JOIN group_members gm ON g.id=gm.group_id "
           "WHERE gm.username=? GROUP BY g.id");
    q.addBindValue(handler->username());
    q.exec();
    // Re-query since we need to bind first
    q.finish();
    q.prepare("SELECT g.id, g.name, g.owner, "
              "(SELECT COUNT(*) FROM group_members WHERE group_id=g.id) "
              "FROM chat_groups g JOIN group_members gm ON g.id=gm.group_id "
              "WHERE gm.username=? GROUP BY g.id");
    q.addBindValue(handler->username());
    q.exec();
    QJsonArray arr;
    while (q.next()) {
        QJsonObject g;
        g["id"] = q.value(0).toInt(); g["name"] = q.value(1).toString();
        g["owner"] = q.value(2).toString(); g["members"] = q.value(3).toInt();
        arr.append(g);
    }
    QJsonObject r; r["type"] = "group_list"; r["groups"] = arr;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
}

void ChatServer::listGroupMembers(ClientHandler *handler, int groupId)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT username FROM group_members WHERE group_id = ?");
    q.addBindValue(groupId); q.exec();
    QJsonArray arr;
    while (q.next()) arr.append(q.value(0).toString());
    QJsonObject r; r["type"] = "group_members_list"; r["group_id"] = groupId; r["members"] = arr;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
}

// ========== Friend System ==========

void ChatServer::searchUsers(ClientHandler *handler, const QString &query)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT username FROM users WHERE username LIKE ? AND username != ? LIMIT 20");
    q.addBindValue("%" + query + "%"); q.addBindValue(handler->username()); q.exec();
    QJsonArray arr;
    while (q.next()) arr.append(q.value(0).toString());
    QJsonObject r; r["type"] = "search_results"; r["query"] = query; r["users"] = arr;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
}

void ChatServer::sendFriendRequest(ClientHandler *from, const QString &to)
{
    if (from->username() == to) {
        QJsonObject r; r["type"] = "friend_error"; r["content"] = "Cannot add yourself";
        from->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact)); return;
    }
    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM users WHERE username = ?"); q.addBindValue(to); q.exec();
    if (!q.next() || q.value(0).toInt() == 0) {
        QJsonObject r; r["type"] = "friend_error"; r["content"] = "User not found";
        from->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact)); return;
    }
    q.prepare("INSERT OR IGNORE INTO friend_requests (from_user, to_user) VALUES (?, ?)");
    q.addBindValue(from->username()); q.addBindValue(to); q.exec();

    QJsonObject r; r["type"] = "friend_request_sent"; r["to"] = to;
    from->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));

    // Notify target if online
    QJsonObject notif; notif["type"] = "friend_request"; notif["from"] = from->username();
    sendToUser(to, QJsonDocument(notif).toJson(QJsonDocument::Compact));
}

void ChatServer::respondFriendRequest(ClientHandler *handler, const QString &from, const QString &action)
{
    QSqlQuery q(m_db);
    if (action == "accept") {
        q.prepare("INSERT OR IGNORE INTO friends (username, friend) VALUES (?, ?)");
        q.addBindValue(handler->username()); q.addBindValue(from); q.exec();
        q.prepare("INSERT OR IGNORE INTO friends (username, friend) VALUES (?, ?)");
        q.addBindValue(from); q.addBindValue(handler->username()); q.exec();
    }
    if (action == "ignore") {
        q.prepare("UPDATE friend_requests SET status='ignored' WHERE from_user=? AND to_user=?");
        q.addBindValue(from); q.addBindValue(handler->username()); q.exec();
        QJsonObject r; r["type"] = "friend_ignored"; r["from"] = from;
        handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
        return;
    }
    // accept or reject: delete the request
    q.prepare("DELETE FROM friend_requests WHERE from_user = ? AND to_user = ?");
    q.addBindValue(from); q.addBindValue(handler->username()); q.exec();

    QJsonObject r; r["type"] = action == "accept" ? "friend_added" : "friend_rejected";
    r["friend"] = from;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));

    if (action == "accept") {
        QJsonObject notif; notif["type"] = "friend_added"; notif["friend"] = handler->username();
        sendToUser(from, QJsonDocument(notif).toJson(QJsonDocument::Compact));
    }
}

void ChatServer::listFriends(ClientHandler *handler)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT friend, group_name FROM friends WHERE username = ?");
    q.addBindValue(handler->username()); q.exec();
    QJsonArray arr;
    while (q.next()) {
        QJsonObject f;
        f["name"] = q.value(0).toString();
        f["group"] = q.value(1).toString();
        arr.append(f);
    }
    QJsonObject r; r["type"] = "friend_list"; r["friends"] = arr;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
}

void ChatServer::setFriendGroup(ClientHandler *handler, const QString &friendName,
                                 const QString &groupName)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE friends SET group_name = ? WHERE username = ? AND friend = ?");
    q.addBindValue(groupName); q.addBindValue(handler->username()); q.addBindValue(friendName); q.exec();
    QJsonObject r; r["type"] = "friend_group_set"; r["friend"] = friendName; r["group"] = groupName;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
}

void ChatServer::createFriendGroup(ClientHandler *handler, const QString &groupName)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO friend_groups (username, group_name) VALUES (?, ?)");
    q.addBindValue(handler->username()); q.addBindValue(groupName); q.exec();
    QJsonObject r; r["type"] = "friend_group_created"; r["group"] = groupName;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
}

void ChatServer::renameFriendGroup(ClientHandler *handler, const QString &oldName, const QString &newName)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE friends SET group_name = ? WHERE username = ? AND group_name = ?");
    q.addBindValue(newName); q.addBindValue(handler->username()); q.addBindValue(oldName); q.exec();
    q.prepare("UPDATE OR IGNORE friend_groups SET group_name = ? WHERE username = ? AND group_name = ?");
    q.addBindValue(newName); q.addBindValue(handler->username()); q.addBindValue(oldName); q.exec();
    QJsonObject r; r["type"] = "friend_group_renamed"; r["old"] = oldName; r["new"] = newName;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
}

void ChatServer::deleteFriendGroup(ClientHandler *handler, const QString &groupName)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE friends SET group_name = '' WHERE username = ? AND group_name = ?");
    q.addBindValue(handler->username()); q.addBindValue(groupName); q.exec();
    q.prepare("DELETE FROM friend_groups WHERE username = ? AND group_name = ?");
    q.addBindValue(handler->username()); q.addBindValue(groupName); q.exec();
    QJsonObject r; r["type"] = "friend_group_deleted"; r["group"] = groupName;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
}

void ChatServer::listFriendGroups(ClientHandler *handler)
{
    QSqlQuery q(m_db);
    // Union: groups from friend_groups table (including empty) + groups from friends table
    q.prepare("SELECT group_name, 0 as cnt FROM friend_groups WHERE username = ? "
              "UNION "
              "SELECT group_name, COUNT(*) FROM friends WHERE username = ? AND group_name != '' "
              "GROUP BY group_name ORDER BY group_name");
    q.addBindValue(handler->username());
    q.addBindValue(handler->username());
    q.exec();
    QJsonArray arr;
    QSet<QString> seen;
    while (q.next()) {
        QString name = q.value(0).toString();
        int cnt = q.value(1).toInt();
        if (name.isEmpty() || seen.contains(name)) continue;
        seen.insert(name);
        QJsonObject g;
        g["name"] = name;
        g["count"] = cnt;
        arr.append(g);
    }
    QJsonObject r; r["type"] = "friend_groups"; r["groups"] = arr;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
}

void ChatServer::listPendingRequests(ClientHandler *handler)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT from_user FROM friend_requests WHERE to_user = ? AND status='pending'");
    q.addBindValue(handler->username()); q.exec();
    QJsonArray arr;
    while (q.next()) arr.append(q.value(0).toString());
    QJsonObject r; r["type"] = "pending_requests"; r["requests"] = arr;
    handler->sendMessage(QJsonDocument(r).toJson(QJsonDocument::Compact));
}

void ChatServer::sendUserList(ClientHandler *to)
{
    QStringList users;
    { QReadLocker locker(&m_lock); users = m_usernames.keys(); users.sort(Qt::CaseInsensitive); }
    QJsonObject msg; msg["type"] = "userlist"; msg["users"] = QJsonArray::fromStringList(users);
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    if (to) to->sendMessage(data); else broadcast(data);
}

void ChatServer::removeHandler(ClientHandler *handler)
{
    QString name = handler->username(); bool wasLoggedIn = handler->isLoggedIn();
    { QWriteLocker locker(&m_lock); m_clients.removeAll(handler); if (wasLoggedIn && m_usernames.value(name) == handler) m_usernames.remove(name); }
    handler->deleteLater();
    emit logMessage(QString("[%1] disconnected  (%2 online)").arg(name).arg(m_usernames.size()));
    if (wasLoggedIn) {
        QJsonObject sysMsg; sysMsg["type"] = "system"; sysMsg["content"] = QString("%1 left the chat").arg(name);
        broadcast(QJsonDocument(sysMsg).toJson(QJsonDocument::Compact)); sendUserList();
    }
}
