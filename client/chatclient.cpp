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
    } else if (type == "system") {
        emit systemMessage(obj["content"].toString());
    }
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
