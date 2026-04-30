#include "privatechatdialog.h"
#include "chatclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QTimer>

PrivateChatDialog::PrivateChatDialog(const QString &peer, ChatClient *client,
                                     const QList<ChatMessage> &history,
                                     QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_peer(peer)
    , m_client(client)
    , m_myUsername(client->username())
{
    setWindowTitle(QString("Chat with %1").arg(peer));
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumSize(380, 380);
    resize(440, 480);

    setStyleSheet("PrivateChatDialog { background: #1a1a2e; }");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Scrollable message area
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setStyleSheet(
        "QScrollArea { background: #1a1a2e; border: none; }"
        "QScrollBar:vertical {"
        "  background: #1a1a2e; width: 6px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #30363d; border-radius: 3px; min-height: 30px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    m_bubbleContainer = new QWidget;
    m_bubbleContainer->setStyleSheet("background: #1a1a2e;");
    auto *bubbleLayout = new QVBoxLayout(m_bubbleContainer);
    bubbleLayout->setContentsMargins(10, 10, 10, 10);
    bubbleLayout->setSpacing(8);
    bubbleLayout->addStretch();
    m_scroll->setWidget(m_bubbleContainer);

    // Load history
    for (const auto &msg : history)
        addMessage(msg);

    layout->addWidget(m_scroll);

    // Input bar
    auto *inputBar = new QHBoxLayout;
    inputBar->setContentsMargins(10, 8, 10, 10);
    inputBar->setSpacing(6);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(QString("Message @%1...").arg(peer));
    m_input->setStyleSheet(
        "QLineEdit {"
        "  background: #161b22; color: #c9d1d9;"
        "  border: 1px solid #30363d; border-radius: 8px;"
        "  padding: 9px 14px; font-size: 14px;"
        "}"
        "QLineEdit:focus { border-color: #667eea; background: #1c2333; }");
    connect(m_input, &QLineEdit::returnPressed, this, &PrivateChatDialog::onSendClicked);
    inputBar->addWidget(m_input);

    m_sendBtn = new QPushButton("Send", this);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #667eea,stop:1 #764ba2);"
        "  color: white; border: none; border-radius: 8px;"
        "  padding: 9px 18px; font-size: 13px; font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #7b93f5,stop:1 #8b5fbf);"
        "}");
    connect(m_sendBtn, &QPushButton::clicked, this, &PrivateChatDialog::onSendClicked);
    inputBar->addWidget(m_sendBtn);

    layout->addLayout(inputBar);

    scrollToBottom();
}

QString PrivateChatDialog::avatarLetter(const QString &name) const
{
    return name.isEmpty() ? "?" : name.left(1).toUpper();
}

QWidget *PrivateChatDialog::buildBubble(const ChatMessage &msg)
{
    bool fromMe = (msg.sender == m_myUsername);
    QString dateStr = msg.timestamp.toString("yyyy-MM-dd");
    QString timeStr = msg.timestamp.toString("hh:mm:ss");

    auto *wrapper = new QWidget;
    auto *outerLayout = new QVBoxLayout(wrapper);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(2);

    // Date separator
    if (dateStr != m_prevDate) {
        m_prevDate = dateStr;
        auto *dateLabel = new QLabel(dateStr);
        dateLabel->setAlignment(Qt::AlignCenter);
        dateLabel->setStyleSheet(
            "color: #8b949e; font-size: 11px; padding: 6px 0;");
        outerLayout->addWidget(dateLabel);
    }

    // Time label
    auto *timeLabel = new QLabel(timeStr);
    timeLabel->setAlignment(fromMe ? Qt::AlignRight : Qt::AlignLeft);
    timeLabel->setStyleSheet("color: #484f58; font-size: 10px; padding: 0 46px;");
    outerLayout->addWidget(timeLabel);

    // Bubble row: [avatar] [bubble] or [bubble] [avatar]
    auto *row = new QHBoxLayout;
    row->setSpacing(8);

    // Avatar
    auto *avatar = new QLabel(avatarLetter(msg.sender));
    avatar->setFixedSize(34, 34);
    avatar->setAlignment(Qt::AlignCenter);
    QString avatarBg = fromMe ? "#667eea" : "#30363d";
    avatar->setStyleSheet(
        QString("background: %1; color: white; border-radius: 17px;"
                "font-size: 14px; font-weight: bold;").arg(avatarBg));

    // Bubble
    auto *bubble = new QFrame;
    bubble->setMaximumWidth(280);
    auto *bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(12, 8, 12, 8);

    auto *nameLabel = new QLabel(msg.sender);
    nameLabel->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold;")
            .arg(fromMe ? "#7eb8ff" : "#f0883e"));

    auto *contentLabel = new QLabel(msg.content);
    contentLabel->setWordWrap(true);
    contentLabel->setStyleSheet("color: #e6e6e6; font-size: 13px;");

    bubbleLayout->addWidget(nameLabel);
    bubbleLayout->addWidget(contentLabel);

    QString bubbleColor = fromMe ? "#1a3a5c" : "#21262d";
    bubble->setStyleSheet(
        QString("background: %1; border-radius: 10px;").arg(bubbleColor));

    if (fromMe) {
        row->addStretch();
        row->addWidget(bubble);
        row->addWidget(avatar);
    } else {
        row->addWidget(avatar);
        row->addWidget(bubble);
        row->addStretch();
    }

    outerLayout->addLayout(row);
    return wrapper;
}

void PrivateChatDialog::addMessage(const ChatMessage &msg)
{
    // Insert before the stretch item at the end
    auto *layout = qobject_cast<QVBoxLayout *>(m_bubbleContainer->layout());
    if (!layout) return;

    QWidget *bubble = buildBubble(msg);
    layout->insertWidget(layout->count() - 1, bubble);
    scrollToBottom();
}

void PrivateChatDialog::scrollToBottom()
{
    QTimer::singleShot(50, this, [this]() {
        auto *bar = m_scroll->verticalScrollBar();
        if (bar)
            bar->setValue(bar->maximum());
    });
}

void PrivateChatDialog::onSendClicked()
{
    QString text = m_input->text().trimmed();
    if (text.isEmpty())
        return;

    m_client->sendPrivate(m_peer, text);
    emit messageSent(m_peer, text);
    m_input->clear();
    m_input->setFocus();
}

void PrivateChatDialog::closeEvent(QCloseEvent *event)
{
    emit dialogClosed(m_peer);
    QWidget::closeEvent(event);
}

void PrivateChatDialog::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    scrollToBottom();
}
