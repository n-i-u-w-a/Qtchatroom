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

    upgradeDatabase();

    emit logMessage("Database initialized (chat.db)");
}

void ChatServer::upgradeDatabase()
{
    // Add question/answer columns if upgrading from older schema
    QSqlQuery q(m_db);
    q.exec("SELECT question FROM users LIMIT 1");
    if (q.lastError().isValid()) {
        q.exec("ALTER TABLE users ADD COLUMN question TEXT DEFAULT ''");
        q.exec("ALTER TABLE users ADD COLUMN answer TEXT DEFAULT ''");
        emit logMessage("Database upgraded with security question columns");
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

    connect(handler, &ClientHandler::disconnected, this, &ChatServer::removeHandler);

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
