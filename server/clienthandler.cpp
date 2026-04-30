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

        if (type == "delete_account") {
            result["_action"] = QStringLiteral("delete_account");
            return result;
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
        } else if (action == "delete_account") {
            emit deleteAccountRequested(this);
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
