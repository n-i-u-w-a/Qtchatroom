#pragma once

#include <QWidget>
#include <QDateTime>
#include <QList>

class QScrollArea;
class QLineEdit;
class QPushButton;
class ChatClient;

struct ChatMessage {
    QString sender;
    QString content;
    QDateTime timestamp;
};

class PrivateChatDialog : public QWidget {
    Q_OBJECT
public:
    explicit PrivateChatDialog(const QString &peer, ChatClient *client,
                               const QList<ChatMessage> &history,
                               QWidget *parent = nullptr);

    void addMessage(const ChatMessage &msg);
    QString peerName() const { return m_peer; }

signals:
    void messageSent(const QString &to, const QString &content);
    void dialogClosed(const QString &peer);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onSendClicked();

private:
    QWidget *buildBubble(const ChatMessage &msg);
    void scrollToBottom();
    QString avatarLetter(const QString &name) const;

    QString m_peer;
    ChatClient *m_client;
    QScrollArea *m_scroll;
    QWidget *m_bubbleContainer;
    QLineEdit *m_input;
    QPushButton *m_sendBtn;
    QString m_myUsername;
    QString m_prevDate;
};
