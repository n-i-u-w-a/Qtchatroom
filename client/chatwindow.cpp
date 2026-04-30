#include "chatwindow.h"
#include "chatclient.h"
#include "privatechatdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QGraphicsDropShadowEffect>
#include <QStackedWidget>

ChatWindow::ChatWindow(QWidget *parent)
    : QWidget(parent)
    , m_client(new ChatClient(this))
    , m_host("127.0.0.1")
    , m_port(9527)
{
    setWindowTitle("Chat Room");
    buildUi();
    applyStyles();

    connect(m_client, &ChatClient::connected,
            this, &ChatWindow::onConnected);
    connect(m_client, &ChatClient::disconnected,
            this, &ChatWindow::onDisconnected);
    connect(m_client, &ChatClient::loginSuccess,
            this, &ChatWindow::onLoginSuccess);
    connect(m_client, &ChatClient::loginError,
            this, &ChatWindow::onLoginError);
    connect(m_client, &ChatClient::securityQuestionReceived,
            this, &ChatWindow::onSecurityQuestion);
    connect(m_client, &ChatClient::forgotError,
            this, &ChatWindow::onForgotError);
    connect(m_client, &ChatClient::passwordResetOk,
            this, &ChatWindow::onPasswordResetOk);
    connect(m_client, &ChatClient::resetError,
            this, &ChatWindow::onResetError);
    connect(m_client, &ChatClient::userListReceived,
            this, &ChatWindow::onUserListReceived);
    connect(m_client, &ChatClient::messageReceived,
            this, &ChatWindow::onMessageReceived);
    connect(m_client, &ChatClient::systemMessage,
            this, &ChatWindow::onSystemMessage);
    connect(m_client, &ChatClient::errorOccurred,
            this, &ChatWindow::onConnectionError);

    m_loginPanel->setVisible(true);
    m_chatPanel->setVisible(false);
    setMinimumSize(440, 420);
    resize(500, 500);
}

ChatWindow::~ChatWindow()
{
    for (auto *d : m_dialogs)
        d->close();
}

void ChatWindow::buildUi()
{
    // ==== LOGIN PANEL (full-screen gradient background) ====
    m_loginPanel = new QWidget(this);
    m_loginPanel->setObjectName("loginPanel");
    m_loginPanel->setStyleSheet(
        "#loginPanel {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "    stop:0 #0f0c29, stop:0.5 #302b63, stop:1 #24243e);"
        "}");

    auto *loginOuter = new QVBoxLayout(m_loginPanel);
    loginOuter->setAlignment(Qt::AlignCenter);

    // Card stack — switches between auth and forgot password
    m_cardStack = new QStackedWidget(m_loginPanel);
    m_cardStack->setFixedSize(380, 440);

    auto *authCard = new QWidget;
    authCard->setObjectName("loginCard");
    authCard->setStyleSheet("#loginCard { background: rgba(255,255,255,0.95); border-radius: 16px; }");
    auto *authShadow = new QGraphicsDropShadowEffect(authCard);
    authShadow->setBlurRadius(40);
    authShadow->setColor(QColor(0, 0, 0, 80));
    authShadow->setOffset(0, 8);
    authCard->setGraphicsEffect(authShadow);
    buildAuthCard(authCard);

    auto *forgotCard = new QWidget;
    forgotCard->setObjectName("loginCard");
    forgotCard->setStyleSheet("#loginCard { background: rgba(255,255,255,0.95); border-radius: 16px; }");
    auto *forgotShadow = new QGraphicsDropShadowEffect(forgotCard);
    forgotShadow->setBlurRadius(40);
    forgotShadow->setColor(QColor(0, 0, 0, 80));
    forgotShadow->setOffset(0, 8);
    forgotCard->setGraphicsEffect(forgotShadow);
    buildForgotCard(forgotCard);

    m_cardStack->addWidget(authCard);
    m_cardStack->addWidget(forgotCard);

    loginOuter->addWidget(m_cardStack);

    // ==== CHAT PANEL (dark theme) ====
    m_chatPanel = new QWidget(this);
    m_chatPanel->setObjectName("chatPanel");
    m_chatPanel->setStyleSheet(
        "#chatPanel { background: #1a1a2e; }");

    auto *mainLayout = new QHBoxLayout(m_chatPanel);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // -- Left: messages --
    auto *leftLayout = new QVBoxLayout;
    leftLayout->setSpacing(8);

    m_chatView = new QTextEdit(m_chatPanel);
    m_chatView->setReadOnly(true);
    m_chatView->setObjectName("chatView");
    m_chatView->setStyleSheet(
        "#chatView {"
        "  background: #0d1117; color: #e6e6e6;"
        "  border: 1px solid #30363d; border-radius: 10px;"
        "  padding: 10px; font-size: 13px;"
        "}");
    m_chatView->setPlaceholderText("Chat messages will appear here...");
    leftLayout->addWidget(m_chatView);

    // Target selector + input
    auto *sendBar = new QHBoxLayout;
    sendBar->setSpacing(6);

    auto *toLabel = new QLabel("To:", m_chatPanel);
    toLabel->setStyleSheet("color: #8b949e; font-size: 13px; font-weight: bold;");
    sendBar->addWidget(toLabel);

    m_targetCombo = new QComboBox(m_chatPanel);
    m_targetCombo->addItem("[Everyone]");
    m_targetCombo->setMinimumWidth(110);
    m_targetCombo->setStyleSheet(
        "QComboBox {"
        "  background: #161b22; color: #c9d1d9;"
        "  border: 1px solid #30363d; border-radius: 6px;"
        "  padding: 6px 10px; font-size: 13px;"
        "}"
        "QComboBox:hover { border-color: #667eea; }"
        "QComboBox QAbstractItemView {"
        "  background: #161b22; color: #c9d1d9;"
        "  selection-background-color: #30363d;"
        "  border: 1px solid #30363d;"
        "}");
    sendBar->addWidget(m_targetCombo);

    m_input = new QLineEdit(m_chatPanel);
    m_input->setPlaceholderText("Type a message...");
    m_input->setStyleSheet(
        "QLineEdit {"
        "  background: #161b22; color: #c9d1d9;"
        "  border: 1px solid #30363d; border-radius: 8px;"
        "  padding: 9px 14px; font-size: 14px;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #667eea; background: #1c2333;"
        "}");
    connect(m_input, &QLineEdit::returnPressed, this, [this]() { onSendClicked(); });
    sendBar->addWidget(m_input);

    QString purpleBtn =
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #667eea, stop:1 #764ba2);"
        "  color: white; border: none; border-radius: 8px;"
        "  padding: 9px 18px; font-size: 13px; font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #7b93f5, stop:1 #8b5fbf);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #5a6fd6, stop:1 #6a4190);"
        "}";

    m_sendBtn = new QPushButton("Send", m_chatPanel);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setStyleSheet(purpleBtn);
    connect(m_sendBtn, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
    sendBar->addWidget(m_sendBtn);

    leftLayout->addLayout(sendBar);

    auto *bottomBar = new QHBoxLayout;
    bottomBar->setSpacing(8);

    m_disconnectBtn = new QPushButton("Disconnect", m_chatPanel);
    m_disconnectBtn->setCursor(Qt::PointingHandCursor);
    m_disconnectBtn->setStyleSheet(
        "QPushButton {"
        "  background: transparent; color: #f85149;"
        "  border: 1px solid #f85149; border-radius: 6px;"
        "  padding: 6px 0; font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(248,81,73,0.15);"
        "}");
    connect(m_disconnectBtn, &QPushButton::clicked, this, [this]() {
        m_client->disconnectFromServer();
    });
    bottomBar->addWidget(m_disconnectBtn);

    auto *deleteBtn = new QPushButton("Delete Account", m_chatPanel);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setStyleSheet(
        "QPushButton {"
        "  background: rgba(248,81,73,0.1); color: #f85149;"
        "  border: 1px solid rgba(248,81,73,0.4); border-radius: 6px;"
        "  padding: 6px 12px; font-size: 12px; font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(248,81,73,0.25);"
        "}");
    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
        auto result = QMessageBox::question(
            this, "Delete Account",
            "Are you sure you want to permanently delete your account?\n"
            "This action cannot be undone.",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (result == QMessageBox::Yes)
            m_client->sendDeleteAccount();
    });
    bottomBar->addWidget(deleteBtn);

    leftLayout->addLayout(bottomBar);

    mainLayout->addLayout(leftLayout, 1);

    // -- Right: user list --
    auto *rightLayout = new QVBoxLayout;
    rightLayout->setSpacing(6);

    auto *onlineLabel = new QLabel("Online", m_chatPanel);
    onlineLabel->setStyleSheet(
        "color: #8b949e; font-size: 12px; font-weight: bold;"
        "text-transform: uppercase; letter-spacing: 1px;");
    rightLayout->addWidget(onlineLabel);

    m_userList = new QListWidget(m_chatPanel);
    m_userList->setMinimumWidth(140);
    m_userList->setMaximumWidth(170);
    m_userList->setStyleSheet(
        "QListWidget {"
        "  background: #161b22; color: #c9d1d9;"
        "  border: 1px solid #30363d; border-radius: 10px;"
        "  padding: 4px; font-size: 13px;"
        "}"
        "QListWidget::item {"
        "  padding: 6px 10px; border-radius: 6px;"
        "}"
        "QListWidget::item:hover {"
        "  background: #21262d;"
        "}"
        "QListWidget::item:selected {"
        "  background: rgba(102,126,234,0.25); color: #c9d1d9;"
        "}");
    connect(m_userList, &QListWidget::itemDoubleClicked,
            this, &ChatWindow::onUserDoubleClicked);
    connect(m_userList, &QListWidget::currentItemChanged, this, [this]() {
        auto *item = m_userList->currentItem();
        if (item) {
            int idx = m_targetCombo->findText(item->text());
            if (idx >= 0)
                m_targetCombo->setCurrentIndex(idx);
        }
    });
    rightLayout->addWidget(m_userList);

    auto *hintLabel = new QLabel("Double-click to PM", m_chatPanel);
    hintLabel->setStyleSheet("color: #484f58; font-size: 11px;");
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setWordWrap(true);
    rightLayout->addWidget(hintLabel);

    mainLayout->addLayout(rightLayout);

    // Stack panels
    auto *windowLayout = new QVBoxLayout(this);
    windowLayout->setContentsMargins(0, 0, 0, 0);
    windowLayout->addWidget(m_loginPanel);
    windowLayout->addWidget(m_chatPanel);
}

void ChatWindow::buildAuthCard(QWidget *card)
{
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(36, 16, 36, 24);
    layout->setSpacing(10);

    // Back button (visible in register mode)
    auto *backBtn = new QPushButton("← Back", card);
    backBtn->setObjectName("registerBackBtn");
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFlat(true);
    backBtn->setFixedWidth(60);
    backBtn->setStyleSheet(
        "QPushButton { color: #667eea; font-size: 13px; font-weight: bold;"
        "  border: none; padding: 0; text-align: left; }"
        "QPushButton:hover { color: #764ba2; }");
    backBtn->setVisible(false);
    connect(backBtn, &QPushButton::clicked, this, &ChatWindow::onToggleMode);
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    auto *title = new QLabel("Chat Room", card);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 26px; font-weight: bold; color: #302b63;");
    layout->addWidget(title);

    m_subtitle = new QLabel("Login to your account", card);
    m_subtitle->setObjectName("loginSubtitle");
    m_subtitle->setAlignment(Qt::AlignCenter);
    m_subtitle->setStyleSheet("font-size: 13px; color: #888; margin-bottom: 2px;");
    layout->addWidget(m_subtitle);

    QString inputStyle =
        "QLineEdit { border: 1px solid #ddd; border-radius: 8px;"
        "  padding: 9px 14px; font-size: 14px; background: #f8f9fa; color: #333; }"
        "QLineEdit:focus { border-color: #667eea; background: white; }";

    auto *hostInput = new QLineEdit(m_host, card);
    hostInput->setPlaceholderText("Server address");
    hostInput->setStyleSheet(inputStyle);
    connect(hostInput, &QLineEdit::textChanged, this, [this](const QString &t) { m_host = t; });
    layout->addWidget(hostInput);

    auto *portInput = new QLineEdit(QString::number(m_port), card);
    portInput->setPlaceholderText("Port");
    portInput->setStyleSheet(inputStyle);
    connect(portInput, &QLineEdit::textChanged, this, [this](const QString &t) {
        bool ok; quint16 p = t.toUShort(&ok); if (ok) m_port = p;
    });
    layout->addWidget(portInput);

    m_usernameInput = new QLineEdit(card);
    m_usernameInput->setPlaceholderText("Username");
    m_usernameInput->setStyleSheet(inputStyle);
    layout->addWidget(m_usernameInput);

    m_passwordInput = new QLineEdit(card);
    m_passwordInput->setPlaceholderText("Password");
    m_passwordInput->setEchoMode(QLineEdit::Password);
    m_passwordInput->setStyleSheet(inputStyle);
    connect(m_passwordInput, &QLineEdit::returnPressed, this, &ChatWindow::onLoginClicked);
    layout->addWidget(m_passwordInput);

    // Security question (register only)
    m_questionCombo = new QComboBox(card);
    m_questionCombo->addItem("Choose a security question...");
    m_questionCombo->addItems({
        "你第一只宠物叫什么名字？",
        "你母亲的名字是什么？",
        "你出生的城市是哪里？",
        "你小时候最好的朋友叫什么？",
        "你最喜欢的老师叫什么？"
    });
    m_questionCombo->setStyleSheet(
        "QComboBox { border: 1px solid #ddd; border-radius: 8px;"
        "  padding: 8px 12px; font-size: 13px; background: #f8f9fa; color: #333; }"
        "QComboBox:hover { border-color: #667eea; }"
        "QComboBox QAbstractItemView { background: white; selection-background-color: #e8e8ff; }");
    m_questionCombo->setVisible(false);
    layout->addWidget(m_questionCombo);

    // Security answer (register only)
    m_answerInput = new QLineEdit(card);
    m_answerInput->setPlaceholderText("Answer to security question");
    m_answerInput->setStyleSheet(inputStyle);
    m_answerInput->setVisible(false);
    layout->addWidget(m_answerInput);

    // Button
    m_loginBtn = new QPushButton("Login", card);
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    m_loginBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #667eea,stop:1 #764ba2);"
        "  color: white; border: none; border-radius: 8px; padding: 11px 0; font-size: 15px; font-weight: bold; }"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #7b93f5,stop:1 #8b5fbf); }"
        "QPushButton:pressed { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #5a6fd6,stop:1 #6a4190); }"
        "QPushButton:disabled { background: #ccc; color: #999; }");
    connect(m_loginBtn, &QPushButton::clicked, this, &ChatWindow::onLoginClicked);
    layout->addWidget(m_loginBtn);

    m_modeToggle = new QPushButton("Don't have an account? Register", card);
    m_modeToggle->setCursor(Qt::PointingHandCursor);
    m_modeToggle->setFlat(true);
    m_modeToggle->setStyleSheet("QPushButton { color: #667eea; font-size: 12px; border: none; }"
                                "QPushButton:hover { color: #764ba2; text-decoration: underline; }");
    connect(m_modeToggle, &QPushButton::clicked, this, &ChatWindow::onToggleMode);
    layout->addWidget(m_modeToggle);

    m_forgotLink = new QPushButton("Forgot Password?", card);
    m_forgotLink->setCursor(Qt::PointingHandCursor);
    m_forgotLink->setFlat(true);
    m_forgotLink->setStyleSheet("QPushButton { color: #999; font-size: 11px; border: none; }"
                                "QPushButton:hover { color: #667eea; text-decoration: underline; }");
    connect(m_forgotLink, &QPushButton::clicked, this, &ChatWindow::onForgotClicked);
    layout->addWidget(m_forgotLink);
}

void ChatWindow::buildForgotCard(QWidget *card)
{
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(10);

    auto *title = new QLabel("Reset Password", card);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #302b63;");
    layout->addWidget(title);

    auto *sub = new QLabel("Enter your username to find your\nsecurity question", card);
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("font-size: 13px; color: #888; margin-bottom: 6px;");
    layout->addWidget(sub);

    QString inputStyle =
        "QLineEdit { border: 1px solid #ddd; border-radius: 8px;"
        "  padding: 9px 14px; font-size: 14px; background: #f8f9fa; color: #333; }"
        "QLineEdit:focus { border-color: #667eea; background: white; }";

    m_forgotUsername = new QLineEdit(card);
    m_forgotUsername->setPlaceholderText("Your username");
    m_forgotUsername->setStyleSheet(inputStyle);
    layout->addWidget(m_forgotUsername);

    m_forgotQuestion = new QLabel(card);
    m_forgotQuestion->setWordWrap(true);
    m_forgotQuestion->setStyleSheet(
        "color: #333; font-size: 13px; font-weight: bold;"
        "background: #f0f0ff; border-radius: 6px; padding: 10px;");
    m_forgotQuestion->setVisible(false);
    layout->addWidget(m_forgotQuestion);

    m_forgotAnswer = new QLineEdit(card);
    m_forgotAnswer->setPlaceholderText("Your answer");
    m_forgotAnswer->setStyleSheet(inputStyle);
    m_forgotAnswer->setVisible(false);
    layout->addWidget(m_forgotAnswer);

    m_forgotNewPass = new QLineEdit(card);
    m_forgotNewPass->setPlaceholderText("New password (4+ chars)");
    m_forgotNewPass->setEchoMode(QLineEdit::Password);
    m_forgotNewPass->setStyleSheet(inputStyle);
    m_forgotNewPass->setVisible(false);
    layout->addWidget(m_forgotNewPass);

    m_forgotBtn = new QPushButton("Look Up", card);
    m_forgotBtn->setCursor(Qt::PointingHandCursor);
    m_forgotBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #667eea,stop:1 #764ba2);"
        "  color: white; border: none; border-radius: 8px; padding: 11px 0; font-size: 15px; font-weight: bold; }"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #7b93f5,stop:1 #8b5fbf); }"
        "QPushButton:pressed { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #5a6fd6,stop:1 #6a4190); }"
        "QPushButton:disabled { background: #ccc; color: #999; }");
    connect(m_forgotBtn, &QPushButton::clicked, this, &ChatWindow::onForgotClicked);
    layout->addWidget(m_forgotBtn);

    m_backBtn = new QPushButton("Back to Login", card);
    m_backBtn->setCursor(Qt::PointingHandCursor);
    m_backBtn->setFlat(true);
    m_backBtn->setStyleSheet("QPushButton { color: #667eea; font-size: 12px; border: none; }"
                             "QPushButton:hover { color: #764ba2; text-decoration: underline; }");
    connect(m_backBtn, &QPushButton::clicked, this, &ChatWindow::onBackToLogin);
    layout->addWidget(m_backBtn);

    layout->addStretch();
}

void ChatWindow::applyStyles()
{
    m_chatView->document()->setDefaultStyleSheet(
        "body { font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;"
        "  font-size: 13px; background: #0d1117; }"
        ".broadcast { color: #e6e6e6; } "
        ".private { color: #7eb8ff; } "
        ".group { color: #7ee08f; } "
        ".system { color: #8b949e; font-style: italic; }");
}

void ChatWindow::onLoginClicked()
{
    QString name = m_usernameInput->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Error", "Username cannot be empty.");
        return;
    }
    QString pass = m_passwordInput->text();
    if (pass.length() < 4) {
        QMessageBox::warning(this, "Error", "Password must be at least 4 characters.");
        return;
    }

    if (m_isRegisterMode) {
        if (m_questionCombo->currentIndex() == 0) {
            QMessageBox::warning(this, "Error", "Please choose a security question.");
            return;
        }
        if (m_answerInput->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Error", "Please answer the security question.");
            return;
        }
    }

    m_loginBtn->setEnabled(false);
    m_modeToggle->setEnabled(false);
    m_forgotLink->setEnabled(false);
    m_myUsername = name;
    m_client->connectToServer(m_host, m_port);
}

void ChatWindow::onForgotClicked()
{
    // If in auth card, switch to forgot card
    if (m_cardStack->currentIndex() == 0) {
        m_forgotUsername->setText(m_usernameInput->text().trimmed());
        switchToForgotMode();
        return;
    }

    // Already in forgot card — step 1: look up username
    if (m_forgotBtn->text() == "Look Up") {
        QString name = m_forgotUsername->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, "Error", "Please enter your username.");
            return;
        }
        m_forgotBtn->setEnabled(false);
        m_forgotBtn->setText("Looking up...");
        m_client->connectToServer(m_host, m_port);
        m_resetUsername = name;
        m_myUsername = ""; // Don't auto-login
        return;
    }

    // Step 2: reset password
    QString answer = m_forgotAnswer->text().trimmed();
    QString newPass = m_forgotNewPass->text();
    if (answer.isEmpty() || newPass.length() < 4) {
        QMessageBox::warning(this, "Error", "Please fill in answer and new password (4+ chars).");
        return;
    }
    m_forgotBtn->setEnabled(false);
    m_client->sendResetPassword(m_resetUsername, answer, newPass);
}

void ChatWindow::onResetClicked()
{
    onForgotClicked();
}

void ChatWindow::switchToForgotMode()
{
    m_cardStack->setCurrentIndex(1);
    m_cardStack->setFixedSize(380, 440);
    m_forgotBtn->setText("Look Up");
    m_forgotBtn->setEnabled(true);
    m_forgotQuestion->setVisible(false);
    m_forgotAnswer->setVisible(false);
    m_forgotNewPass->setVisible(false);
    m_forgotAnswer->clear();
    m_forgotNewPass->clear();
}

void ChatWindow::onBackToLogin()
{
    m_cardStack->setCurrentIndex(0);
    m_cardStack->setFixedSize(380, m_isRegisterMode ? 500 : 440);
    if (m_client->isConnected())
        m_client->disconnectFromServer();
}

void ChatWindow::onSecurityQuestion(const QString &username, const QString &question)
{
    Q_UNUSED(username)
    m_forgotQuestion->setText(QString("Security question: %1").arg(question));
    m_forgotQuestion->setVisible(true);
    m_forgotAnswer->setVisible(true);
    m_forgotNewPass->setVisible(true);
    m_forgotBtn->setText("Reset Password");
    m_forgotBtn->setEnabled(true);
    m_cardStack->setFixedSize(380, 520);
}

void ChatWindow::onForgotError(const QString &error)
{
    m_forgotBtn->setText("Look Up");
    m_forgotBtn->setEnabled(true);
    if (m_client->isConnected())
        m_client->disconnectFromServer();
    QMessageBox::warning(this, "Lookup Failed", error);
}

void ChatWindow::onPasswordResetOk(const QString &message)
{
    QMessageBox::information(this, "Success", message);
    m_client->disconnectFromServer();
    onBackToLogin();
}

void ChatWindow::onResetError(const QString &error)
{
    m_forgotBtn->setEnabled(true);
    QMessageBox::warning(this, "Reset Failed", error);
}

void ChatWindow::onToggleMode()
{
    m_isRegisterMode = !m_isRegisterMode;
    setLoginMode(m_isRegisterMode);
}

void ChatWindow::setLoginMode(bool registerMode)
{
    m_isRegisterMode = registerMode;
    m_questionCombo->setVisible(registerMode);
    m_answerInput->setVisible(registerMode);

    auto *backBtn = m_cardStack->findChild<QPushButton *>("registerBackBtn");
    if (backBtn) backBtn->setVisible(registerMode);

    m_cardStack->setFixedSize(380, registerMode ? 500 : 440);

    if (registerMode) {
        m_loginBtn->setText("Register");
        m_modeToggle->setText("Already have an account? Login");
        m_subtitle->setText("Create a new account");
    } else {
        m_loginBtn->setText("Login");
        m_modeToggle->setText("Don't have an account? Register");
        m_subtitle->setText("Login to your account");
    }
}

void ChatWindow::onSendClicked()
{
    QString text = m_input->text().trimmed();
    if (text.isEmpty())
        return;

    QString target = m_targetCombo->currentText();

    if (target == "[Everyone]") {
        m_client->sendBroadcast(text);
    } else {
        m_client->sendPrivate(target, text);
    }

    m_input->clear();
    m_input->setFocus();
}

void ChatWindow::onMessageReceived(const QString &sender, const QString &content,
                                    const QString &timestamp, const QString &scope,
                                    const QString &to)
{
    bool fromMe = (sender == m_myUsername);

    if (scope == "private") {
        QString peer = fromMe ? to : sender;

        // Store in persistent history
        ChatMessage msg{sender, content, QDateTime::currentDateTime()};
        m_chatHistory[peer].append(msg);

        // Forward to private dialog if open
        if (m_dialogs.contains(peer))
            m_dialogs[peer]->addMessage(msg);

        // Show in main view
        if (fromMe) {
            m_chatView->append(
                QString("<span class='private'><b>[%1] You → %2:</b> %3</span>")
                    .arg(timestamp, peer.toHtmlEscaped(), content.toHtmlEscaped()));
        } else {
            m_chatView->append(
                QString("<span class='private'><b>[%1] %2 → You:</b> %3</span>")
                    .arg(timestamp, sender.toHtmlEscaped(), content.toHtmlEscaped()));
        }
    } else if (scope == "group") {
        m_chatView->append(
            QString("<span class='group'><b>[%1] %2 [Group]:</b> %3</span>")
                .arg(timestamp, sender.toHtmlEscaped(), content.toHtmlEscaped()));
    } else {
        if (!fromMe) {
            m_chatView->append(
                QString("<span class='broadcast'><b>[%1] %2:</b> %3</span>")
                    .arg(timestamp, sender.toHtmlEscaped(), content.toHtmlEscaped()));
        }
    }
}

void ChatWindow::onSystemMessage(const QString &content)
{
    m_chatView->append(
        QString("<span class='system'>%1</span>").arg(content.toHtmlEscaped()));
}

void ChatWindow::onConnected()
{
    // Forgot password flow
    if (m_cardStack->currentIndex() == 1) {
        m_client->sendForgotPassword(m_resetUsername);
        return;
    }

    if (m_isRegisterMode)
        m_client->sendRegister(m_myUsername, m_passwordInput->text(),
                               m_questionCombo->currentText(),
                               m_answerInput->text().trimmed());
    else
        m_client->sendLogin(m_myUsername, m_passwordInput->text());
}

void ChatWindow::onDisconnected()
{
    m_loginPanel->setVisible(true);
    m_chatPanel->setVisible(false);
    m_loginBtn->setEnabled(true);
    m_modeToggle->setEnabled(true);
    m_passwordInput->clear();
    m_chatView->append("<span class='system'>Disconnected from server</span>");

    for (auto *d : m_dialogs)
        d->close();
    m_dialogs.clear();

    setMinimumSize(440, 420);
    resize(500, 500);
}

void ChatWindow::onLoginSuccess(const QString &username)
{
    m_myUsername = username;
    setWindowTitle(QString("Chat Client — %1").arg(username));

    m_loginPanel->setVisible(false);
    m_chatPanel->setVisible(true);
    m_loginBtn->setEnabled(true);

    m_chatView->clear();
    m_chatView->append("<span class='system'>Connected & logged in</span>");

    setMinimumSize(620, 450);
    resize(620, 450);
    m_input->setFocus();
}

void ChatWindow::onLoginError(const QString &error)
{
    m_loginBtn->setEnabled(true);
    m_modeToggle->setEnabled(true);
    m_forgotLink->setEnabled(true);
    m_client->disconnectFromServer();
    QMessageBox::warning(this, "Login Failed", error);
}

void ChatWindow::onUserListReceived(const QStringList &users)
{
    m_onlineUsers = users;

    m_userList->blockSignals(true);
    m_userList->clear();
    for (const auto &u : users)
        m_userList->addItem(u);
    m_userList->blockSignals(false);

    QString current = m_targetCombo->currentText();
    m_targetCombo->clear();
    m_targetCombo->addItem("[Everyone]");
    for (const auto &u : users) {
        if (u != m_myUsername)
            m_targetCombo->addItem(u);
    }

    int idx = m_targetCombo->findText(current);
    if (idx >= 0)
        m_targetCombo->setCurrentIndex(idx);
}

void ChatWindow::onConnectionError(const QString &error)
{
    m_loginBtn->setEnabled(true);
    m_modeToggle->setEnabled(true);
    m_loginPanel->setVisible(true);
    m_chatPanel->setVisible(false);
    QMessageBox::warning(this, "Connection Error", error);
}

void ChatWindow::onUserDoubleClicked(QListWidgetItem *item)
{
    if (!item)
        return;

    QString user = item->text();
    if (user == m_myUsername)
        return;

    openPrivateChat(user);
}

void ChatWindow::openPrivateChat(const QString &peer)
{
    if (m_dialogs.contains(peer)) {
        m_dialogs[peer]->show();
        m_dialogs[peer]->raise();
        m_dialogs[peer]->activateWindow();
        return;
    }

    auto *dialog = new PrivateChatDialog(peer, m_client, m_chatHistory.value(peer));
    connect(dialog, &PrivateChatDialog::dialogClosed,
            this, &ChatWindow::onDialogClosed);

    m_dialogs[peer] = dialog;
    dialog->show();
}

void ChatWindow::onDialogClosed(const QString &peer)
{
    m_dialogs.remove(peer);
}
