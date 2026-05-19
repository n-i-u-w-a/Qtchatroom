#include "clienthandler.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QDateTime>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

ClientHandler::ClientHandler(qintptr socketDescriptor, QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_username(QString("anon_%1").arg(reinterpret_cast<quintptr>(this), 0, 16))
{
    m_socket->setSocketDescriptor(socketDescriptor);
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);
}

ClientHandler::~ClientHandler() = default;

QString ClientHandler::username() const { return m_username; }
void ClientHandler::setUsername(const QString &name) { m_username = name; m_loggedIn = true; }
bool ClientHandler::isLoggedIn() const { return m_loggedIn; }

void ClientHandler::sendMessage(const QByteArray &message)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(message + "\n");
        m_socket->flush();
    }
}

void ClientHandler::disconnectClient()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
        m_socket->disconnectFromHost();
}

void ClientHandler::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    while (true) {
        int idx = m_buffer.indexOf('\n');
        if (idx < 0)
            break;

        QByteArray line = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);

        if (!line.isEmpty())
            processMessage(line);
    }
}

void ClientHandler::onDisconnected()
{
    emit disconnected(this);
}

void ClientHandler::processMessage(const QByteArray &line)
{
    QString senderName = m_username;
    bool loggedIn = m_loggedIn;

    auto future = QtConcurrent::run([line, senderName, loggedIn]() -> QJsonObject {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError)
            return {};

        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();

        QJsonObject result;

        // Auth messages — allowed before login
        if (type == "login") {
            result["_action"] = QStringLiteral("login");
            result["username"] = obj["username"].toString();
            result["password"] = obj["password"].toString();
            return result;
        }

        if (type == "register") {
            result["_action"] = QStringLiteral("register");
            result["username"] = obj["username"].toString();
            result["password"] = obj["password"].toString();
            result["question"] = obj["question"].toString();
            result["answer"] = obj["answer"].toString();
            return result;
        }

        if (type == "forgot_password") {
            result["_action"] = QStringLiteral("forgot_password");
            result["username"] = obj["username"].toString();
            return result;
        }

        if (type == "reset_password") {
            result["_action"] = QStringLiteral("reset_password");
            result["username"] = obj["username"].toString();
            result["answer"] = obj["answer"].toString();
            result["new_password"] = obj["new_password"].toString();
            return result;
        }

        // All other messages require login
        if (!loggedIn) {
            result["_action"] = QStringLiteral("reject");
            return result;
        }

        if (type == "typing") {
            result["_action"] = "typing";
            result["to"] = obj["to"].toString();
            result["from"] = loggedIn ? senderName : QString();
            return result;
        }
        if (type == "delete_account") {
            result["_action"] = QStringLiteral("delete_account");
            return result;
        }

        // Group chat
        if (type == "create_group") {
            result["_action"] = "create_group"; result["name"] = obj["name"].toString(); return result;
        }
        if (type == "join_group") {
            result["_action"] = "join_group"; result["group_id"] = obj["group_id"].toInt(); return result;
        }
        if (type == "leave_group") {
            result["_action"] = "leave_group"; result["group_id"] = obj["group_id"].toInt(); return result;
        }
        if (type == "group_msg") {
            result["_action"] = "group_msg"; result["group_id"] = obj["group_id"].toInt();
            result["content"] = obj["content"].toString();
            result["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss");
            return result;
        }
        if (type == "list_groups") {
            result["_action"] = "list_groups"; return result;
        }
        if (type == "group_members") {
            result["_action"] = "group_members"; result["group_id"] = obj["group_id"].toInt(); return result;
        }

        // Friend system
        if (type == "search_users") {
            result["_action"] = "search_users"; result["query"] = obj["query"].toString(); return result;
        }
        if (type == "friend_request") {
            result["_action"] = "friend_request"; result["to"] = obj["to"].toString(); return result;
        }
        if (type == "friend_response") {
            result["_action"] = "friend_response"; result["from"] = obj["from"].toString();
            result["action"] = obj["action"].toString("accept"); return result;
        }
        if (type == "friend_list") {
            result["_action"] = "friend_list"; return result;
        }
        if (type == "set_friend_group") {
            result["_action"] = "set_friend_group";
            result["friend"] = obj["friend"].toString();
            result["group_name"] = obj["group_name"].toString();
            return result;
        }
        if (type == "create_friend_group") {
            result["_action"] = "create_friend_group"; result["group_name"] = obj["group_name"].toString(); return result;
        }
        if (type == "list_friend_groups") {
            result["_action"] = "list_friend_groups"; return result;
        }
        if (type == "rename_friend_group") {
            result["_action"] = "rename_friend_group";
            result["old"] = obj["old"].toString();
            result["new"] = obj["new"].toString();
            return result;
        }
        if (type == "delete_friend_group") {
            result["_action"] = "delete_friend_group";
            result["group"] = obj["group"].toString();
            return result;
        }
        if (type == "list_pending_requests") {
            result["_action"] = "list_pending_requests"; return result;
        }

        QString content = obj["content"].toString().trimmed();
        if (content.isEmpty()) {
            result["_action"] = QStringLiteral("empty");
            return result;
        }

        if (type == "broadcast") {
            result["_action"] = QStringLiteral("broadcast");
            result["sender"] = senderName;
            result["content"] = content;
            result["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss");
        } else if (type == "private") {
            result["_action"] = QStringLiteral("private");
            result["_target"] = obj["to"].toString();
            result["sender"] = senderName;
            result["content"] = content;
            result["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss");
        } else if (type == "group") {
            result["_action"] = QStringLiteral("group");
            QJsonArray arr = obj["to"].toArray();
            QStringList targets;
            for (const auto &v : arr)
                targets << v.toString();
            result["_targets"] = QJsonArray::fromStringList(targets);
            result["sender"] = senderName;
            result["content"] = content;
            result["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss");
        } else {
            result["_action"] = QStringLiteral("unknown");
        }

        return result;
    });

    auto *watcher = new QFutureWatcher<QJsonObject>(this);
    connect(watcher, &QFutureWatcher<QJsonObject>::finished, this, [this, watcher]() {
        QJsonObject result = watcher->result();
        QString action = result["_action"].toString();

        if (action == "login") {
            emit loginRequested(this, result["username"].toString(),
                                result["password"].toString());
        } else if (action == "register") {
            emit registerRequested(this, result["username"].toString(),
                                   result["password"].toString(),
                                   result["question"].toString(),
                                   result["answer"].toString());
        } else if (action == "forgot_password") {
            emit forgotPasswordRequested(this, result["username"].toString());
        } else if (action == "reset_password") {
            emit resetPasswordRequested(this, result["username"].toString(),
                                        result["answer"].toString(),
                                        result["new_password"].toString());
        } else if (action == "typing") {
            QJsonObject t; t["type"] = "typing"; t["from"] = result["from"].toString();
            emit typingRelayRequested(result["to"].toString(),
                                       QJsonDocument(t).toJson(QJsonDocument::Compact));
        } else if (action == "delete_account") {
            emit deleteAccountRequested(this);
        } else if (action == "create_group") {
            emit createGroupRequested(this, result["name"].toString());
        } else if (action == "join_group") {
            emit joinGroupRequested(this, result["group_id"].toInt());
        } else if (action == "leave_group") {
            emit leaveGroupRequested(this, result["group_id"].toInt());
        } else if (action == "group_msg") {
            emit groupMessageRequested(this, result["group_id"].toInt(),
                                       result["content"].toString(),
                                       result["timestamp"].toString());
        } else if (action == "list_groups") {
            emit listGroupsRequested(this);
        } else if (action == "group_members") {
            emit listGroupMembersRequested(this, result["group_id"].toInt());
        } else if (action == "search_users") {
            emit searchUsersRequested(this, result["query"].toString());
        } else if (action == "friend_request") {
            emit friendRequestRequested(this, result["to"].toString());
        } else if (action == "friend_response") {
            emit friendResponseRequested(this, result["from"].toString(),
                                         result["action"].toString());
        } else if (action == "friend_list") {
            emit friendListRequested(this);
        } else if (action == "set_friend_group") {
            emit setFriendGroupRequested(this, result["friend"].toString(),
                                         result["group_name"].toString());
        } else if (action == "create_friend_group") {
            emit createFriendGroupRequested(this, result["group_name"].toString());
        } else if (action == "rename_friend_group") {
            emit renameFriendGroupRequested(this, result["old"].toString(),
                                            result["new"].toString());
        } else if (action == "delete_friend_group") {
            emit deleteFriendGroupRequested(this, result["group"].toString());
        } else if (action == "list_friend_groups") {
            emit listFriendGroupsRequested(this);
        } else if (action == "list_pending_requests") {
            emit listPendingRequestsRequested(this);
        } else if (action == "broadcast") {
            result.remove("_action");
            result["type"] = "message";
            result["scope"] = QStringLiteral("broadcast");
            QByteArray msg = QJsonDocument(result).toJson(QJsonDocument::Compact);
            sendMessage(msg);
            emit messageForRoute(msg, QString(), QStringList());
        } else if (action == "private") {
            QString targetUser = result["_target"].toString();
            result.remove("_action");
            result.remove("_target");
            result["type"] = "message";
            result["scope"] = QStringLiteral("private");
            result["to"] = targetUser;
            QByteArray msg = QJsonDocument(result).toJson(QJsonDocument::Compact);
            sendMessage(msg);
            emit messageForRoute(msg, targetUser, QStringList());
        } else if (action == "group") {
            QStringList targets;
            for (const auto &v : result["_targets"].toArray())
                targets << v.toString();
            result.remove("_action");
            result.remove("_targets");
            result["type"] = "message";
            result["scope"] = QStringLiteral("group");
            result["recipients"] = QJsonArray::fromStringList(targets);
            QByteArray msg = QJsonDocument(result).toJson(QJsonDocument::Compact);
            sendMessage(msg);
            emit messageForRoute(msg, QString(), targets);
        } else if (action == "reject") {
            QJsonObject err;
            err["type"] = "login_error";
            err["content"] = "Please login first";
            sendMessage(QJsonDocument(err).toJson(QJsonDocument::Compact));
        }

        watcher->deleteLater();
    });
    watcher->setFuture(future);
}
