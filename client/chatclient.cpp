#include "chatclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>

ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &ChatClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &ChatClient::onSocketError);
}

void ChatClient::connectToServer(const QString &host, quint16 port)
{
    m_buffer.clear();
    m_socket->connectToHost(host, port);
}

void ChatClient::disconnectFromServer()
{
    m_socket->disconnectFromHost();
}

bool ChatClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void ChatClient::sendLogin(const QString &username, const QString &password)
{
    QJsonObject msg;
    msg["type"] = "login";
    msg["username"] = username;
    msg["password"] = password;
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    m_socket->write(data + "\n");
    m_socket->flush();
}

void ChatClient::sendRegister(const QString &username, const QString &password,
                               const QString &question, const QString &answer)
{
    QJsonObject msg;
    msg["type"] = "register";
    msg["username"] = username;
    msg["password"] = password;
    msg["question"] = question;
    msg["answer"] = answer;
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    m_socket->write(data + "\n");
    m_socket->flush();
}

void ChatClient::sendForgotPassword(const QString &username)
{
    QJsonObject msg;
    msg["type"] = "forgot_password";
    msg["username"] = username;
    m_socket->write(QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n");
    m_socket->flush();
}

void ChatClient::sendResetPassword(const QString &username, const QString &answer,
                                    const QString &newPassword)
{
    QJsonObject msg;
    msg["type"] = "reset_password";
    msg["username"] = username;
    msg["answer"] = answer;
    msg["new_password"] = newPassword;
    m_socket->write(QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n");
    m_socket->flush();
}

void ChatClient::writeJson(const QJsonObject &msg)
{
    m_socket->write(QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n");
    m_socket->flush();
}

void ChatClient::sendTyping(const QString &to)
    { QJsonObject m; m["type"]="typing"; m["to"]=to; writeJson(m); }

void ChatClient::sendCreateGroup(const QString &name)
    { QJsonObject m; m["type"]="create_group"; m["name"]=name; writeJson(m); }
void ChatClient::sendJoinGroup(int groupId)
    { QJsonObject m; m["type"]="join_group"; m["group_id"]=groupId; writeJson(m); }
void ChatClient::sendLeaveGroup(int groupId)
    { QJsonObject m; m["type"]="leave_group"; m["group_id"]=groupId; writeJson(m); }
void ChatClient::sendGroupMessage(int groupId, const QString &content)
    { QJsonObject m; m["type"]="group_msg"; m["group_id"]=groupId; m["content"]=content; writeJson(m); }
void ChatClient::sendListGroups()
    { QJsonObject m; m["type"]="list_groups"; writeJson(m); }
void ChatClient::sendGroupMembers(int groupId)
    { QJsonObject m; m["type"]="group_members"; m["group_id"]=groupId; writeJson(m); }
void ChatClient::sendSearchUsers(const QString &query)
    { QJsonObject m; m["type"]="search_users"; m["query"]=query; writeJson(m); }
void ChatClient::sendFriendRequest(const QString &to)
    { QJsonObject m; m["type"]="friend_request"; m["to"]=to; writeJson(m); }
void ChatClient::sendFriendResponse(const QString &from, const QString &action)
    { QJsonObject m; m["type"]="friend_response"; m["from"]=from; m["action"]=action; writeJson(m); }
void ChatClient::sendFriendList()
    { QJsonObject m; m["type"]="friend_list"; writeJson(m); }
void ChatClient::sendSetFriendGroup(const QString &friendName, const QString &groupName)
    { QJsonObject m; m["type"]="set_friend_group"; m["friend"]=friendName; m["group_name"]=groupName; writeJson(m); }
void ChatClient::sendCreateFriendGroup(const QString &groupName)
    { QJsonObject m; m["type"]="create_friend_group"; m["group_name"]=groupName; writeJson(m); }
void ChatClient::sendRenameFriendGroup(const QString &oldName, const QString &newName)
    { QJsonObject m; m["type"]="rename_friend_group"; m["old"]=oldName; m["new"]=newName; writeJson(m); }
void ChatClient::sendDeleteFriendGroup(const QString &groupName)
    { QJsonObject m; m["type"]="delete_friend_group"; m["group"]=groupName; writeJson(m); }
void ChatClient::sendListFriendGroups()
    { QJsonObject m; m["type"]="list_friend_groups"; writeJson(m); }
void ChatClient::sendListPendingRequests()
    { QJsonObject m; m["type"]="list_pending_requests"; writeJson(m); }

void ChatClient::sendBroadcast(const QString &content)
{
    QJsonObject msg;
    msg["type"] = "broadcast";
    msg["content"] = content;
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    m_socket->write(data + "\n");
    m_socket->flush();
}

void ChatClient::sendPrivate(const QString &to, const QString &content)
{
    QJsonObject msg;
    msg["type"] = "private";
    msg["to"] = to;
    msg["content"] = content;
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    m_socket->write(data + "\n");
    m_socket->flush();
}

void ChatClient::sendDeleteAccount()
{
    QJsonObject msg;
    msg["type"] = "delete_account";
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    m_socket->write(data + "\n");
    m_socket->flush();
}

void ChatClient::sendGroup(const QStringList &to, const QString &content)
{
    QJsonObject msg;
    msg["type"] = "group";
    msg["to"] = QJsonArray::fromStringList(to);
    msg["content"] = content;
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    m_socket->write(data + "\n");
    m_socket->flush();
}

void ChatClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    while (true) {
        int idx = m_buffer.indexOf('\n');
        if (idx < 0)
            break;

        QByteArray line = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);

        if (!line.isEmpty())
            processLine(line);
    }
}

void ChatClient::processLine(const QByteArray &line)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError)
        return;

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "login_ok") {
        m_username = obj["username"].toString();
        emit loginSuccess(m_username);
    } else if (type == "login_error") {
        emit loginError(obj["content"].toString());
    } else if (type == "security_question") {
        emit securityQuestionReceived(obj["username"].toString(),
                                       obj["question"].toString());
    } else if (type == "forgot_error") {
        emit forgotError(obj["content"].toString());
    } else if (type == "password_reset_ok") {
        emit passwordResetOk(obj["content"].toString());
    } else if (type == "reset_error") {
        emit resetError(obj["content"].toString());
    } else if (type == "userlist") {
        QStringList users;
        for (const auto &v : obj["users"].toArray())
            users << v.toString();
        emit userListReceived(users);
    } else if (type == "message") {
        emit messageReceived(
            obj["sender"].toString(),
            obj["content"].toString(),
            obj["timestamp"].toString(),
            obj["scope"].toString("broadcast"),
            obj["to"].toString());
    } else if (type == "group_created")
        emit groupCreated(obj["group_id"].toInt(), obj["name"].toString());
    else if (type == "group_joined")
        emit groupJoined(obj["group_id"].toInt(), obj["name"].toString());
    else if (type == "group_left")
        emit groupLeft(obj["group_id"].toInt());
    else if (type == "group_list")
        emit groupListReceived(obj["groups"].toArray());
    else if (type == "group_members_list") {
        QStringList members;
        for (const auto &v : obj["members"].toArray()) members << v.toString();
        emit groupMembersReceived(obj["group_id"].toInt(), members);
    } else if (type == "group_error")
        emit groupError(obj["content"].toString());
    // Friend responses
    else if (type == "search_results") {
        QStringList users;
        for (const auto &v : obj["users"].toArray()) users << v.toString();
        emit searchResults(obj["query"].toString(), users);
    } else if (type == "friend_request_sent")
        emit friendRequestSent(obj["to"].toString());
    else if (type == "friend_request")
        emit friendRequestReceived(obj["from"].toString());
    else if (type == "friend_added")
        emit friendAdded(obj["friend"].toString());
    else if (type == "friend_rejected")
        emit friendRejected(obj["friend"].toString());
    else if (type == "friend_list")
        emit friendListReceived(obj["friends"].toArray());
    else if (type == "friend_group_set")
        emit friendGroupSet(obj["friend"].toString(), obj["group"].toString());
    else if (type == "friend_group_created")
        emit friendGroupCreated(obj["group"].toString());
    else if (type == "friend_group_renamed")
        emit friendGroupRenamed(obj["old"].toString(), obj["new"].toString());
    else if (type == "friend_group_deleted")
        emit friendGroupDeleted(obj["group"].toString());
    else if (type == "friend_groups")
        emit friendGroupsReceived(obj["groups"].toArray());
    else if (type == "pending_requests") {
        QStringList reqs;
        for (const auto &v : obj["requests"].toArray()) reqs << v.toString();
        emit pendingRequestsReceived(reqs);
    } else if (type == "friend_ignored")
        emit friendIgnored(obj["from"].toString());
    else if (type == "typing")
        emit typingReceived(obj["from"].toString());
    else if (type == "friend_error")
        emit friendError(obj["content"].toString());
    else if (type == "system")
        emit systemMessage(obj["content"].toString());
}

void ChatClient::onSocketConnected()
{
    emit connected();
}

void ChatClient::onSocketDisconnected()
{
    emit disconnected();
}

void ChatClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    emit errorOccurred(m_socket->errorString());
}
