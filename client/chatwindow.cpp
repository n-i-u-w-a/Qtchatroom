#include "chatwindow.h"
#include "chatclient.h"
#include "privatechatdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QGraphicsDropShadowEffect>
#include <QStackedWidget>
#include <QDialog>
#include <QScrollArea>
#include <QInputDialog>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QApplication>
#include <QTimer>
#include <QFileDialog>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>

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
    // Group signals
    connect(m_client, &ChatClient::groupCreated, this, &ChatWindow::onGroupCreated);
    connect(m_client, &ChatClient::groupJoined, this, &ChatWindow::onGroupJoined);
    connect(m_client, &ChatClient::groupListReceived, this, &ChatWindow::onGroupListReceived);
    connect(m_client, &ChatClient::groupError, this, &ChatWindow::onGroupError);
    // Friend signals
    connect(m_client, &ChatClient::searchResults, this, &ChatWindow::onSearchResults);
    connect(m_client, &ChatClient::friendRequestReceived, this, &ChatWindow::onFriendRequestReceived);
    connect(m_client, &ChatClient::friendAdded, this, &ChatWindow::onFriendAdded);
    connect(m_client, &ChatClient::friendListReceived, this, &ChatWindow::onFriendListReceived);
    connect(m_client, &ChatClient::friendGroupSet, this, &ChatWindow::onFriendGroupSet);
    connect(m_client, &ChatClient::friendGroupCreated, this, [this](const QString &name) {
        if (!m_existingGroups.contains(name)) m_existingGroups.append(name);
        updateSidebar();
    });
    connect(m_client, &ChatClient::friendGroupRenamed, this, &ChatWindow::onFriendGroupRenamed);
    connect(m_client, &ChatClient::friendGroupDeleted, this, &ChatWindow::onFriendGroupDeleted);
    connect(m_client, &ChatClient::friendGroupsReceived, this, &ChatWindow::onFriendGroupsReceived);
    connect(m_client, &ChatClient::pendingRequestsReceived, this, &ChatWindow::onPendingRequestsReceived);
    connect(m_client, &ChatClient::friendIgnored, this, [this](const QString &) {});
    connect(m_client, &ChatClient::friendError, this, &ChatWindow::onFriendError);
    connect(m_client, &ChatClient::typingReceived, this, &ChatWindow::onTypingReceived);
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

    // Typing timer
    m_typingTimer = new QTimer(this);
    m_typingTimer->setInterval(2000);
    m_typingTimer->setSingleShot(true);
    connect(m_typingTimer, &QTimer::timeout, this, [this]() { m_isTyping = false; });
    connect(m_input, &QLineEdit::textChanged, this, &ChatWindow::onTypingChanged);

    // System tray
    setupSystemTray();
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
        "    stop:0 #0a1628, stop:0.5 #1a3a5c, stop:1 #0d2137);"
        "}");

    // Settings button — top-left corner, overlaid on the gradient
    auto *settingsBtn = new QPushButton(QString::fromUtf8("⚙"), m_loginPanel);
    settingsBtn->setCursor(Qt::PointingHandCursor);
    settingsBtn->setFixedSize(32, 32);
    settingsBtn->move(12, 12);
    settingsBtn->setToolTip("Server settings (IP & Port)");
    settingsBtn->setStyleSheet(
        "QPushButton { color: rgba(255,255,255,0.6); font-size: 18px;"
        "  border: none; background: transparent; }"
        "QPushButton:hover { color: white; }");
    connect(settingsBtn, &QPushButton::clicked, this, [this]() {
        // Simple settings dialog
        QDialog dlg(this);
        dlg.setWindowTitle("Server Settings");
        dlg.setFixedSize(320, 170);
        dlg.setStyleSheet(
            "QDialog { background: #0a1628; }"
            "QLabel { color: #c9d1d9; font-size: 13px; }");

        auto *dlgLayout = new QVBoxLayout(&dlg);
        dlgLayout->setContentsMargins(20, 16, 20, 16);
        dlgLayout->setSpacing(10);

        QString inputStyle =
            "QLineEdit { background: #07101a; color: #c9d1d9;"
            "  border: 1px solid #30363d; border-radius: 6px;"
            "  padding: 7px 10px; font-size: 13px; }"
            "QLineEdit:focus { border-color: #2196f3; }";

        auto *hostRow = new QHBoxLayout;
        hostRow->addWidget(new QLabel("IP Address:"));
        auto *hostEdit = new QLineEdit(m_host);
        hostEdit->setStyleSheet(inputStyle);
        hostRow->addWidget(hostEdit);
        dlgLayout->addLayout(hostRow);

        auto *portRow = new QHBoxLayout;
        portRow->addWidget(new QLabel("Port:"));
        auto *portEdit = new QLineEdit(QString::number(m_port));
        portEdit->setStyleSheet(inputStyle);
        portEdit->setFixedWidth(100);
        portRow->addWidget(portEdit);
        portRow->addStretch();
        dlgLayout->addLayout(portRow);

        auto *btnRow = new QHBoxLayout;
        btnRow->addStretch();
        auto *okBtn = new QPushButton("Save");
        okBtn->setCursor(Qt::PointingHandCursor);
        okBtn->setStyleSheet(
            "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 #2196f3,stop:1 #1565c0); color: white; border: none;"
            "  border-radius: 6px; padding: 7px 22px; font-size: 13px; font-weight: bold; }"
            "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 #42a5f5,stop:1 #1976d2); }");
        connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        btnRow->addWidget(okBtn);
        dlgLayout->addLayout(btnRow);

        if (dlg.exec() == QDialog::Accepted) {
            m_host = hostEdit->text().trimmed();
            bool ok; quint16 p = portEdit->text().toUShort(&ok);
            if (ok && p > 0) m_port = p;
        }
    });

    auto *loginOuter = new QVBoxLayout(m_loginPanel);
    loginOuter->setAlignment(Qt::AlignCenter);

    // Card stack — switches between auth and forgot password
    m_cardStack = new QStackedWidget(m_loginPanel);
    m_cardStack->setFixedSize(380, 340);

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

    // ==== CHAT PANEL (QQ-style) ====
    buildChatPanel();

    // Stack panels
    auto *windowLayout = new QVBoxLayout(this);
    windowLayout->setContentsMargins(0, 0, 0, 0);
    windowLayout->addWidget(m_loginPanel);
    windowLayout->addWidget(m_chatPanel);
}

void ChatWindow::buildChatPanel()
{
    m_chatPanel = new QWidget(this);
    m_chatPanel->setObjectName("chatPanel");
    m_chatPanel->setStyleSheet("#chatPanel { background: #0a1628; }");

    auto *panelLayout = new QVBoxLayout(m_chatPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet("QSplitter::handle { background: #30363d; }");

    // ==== LEFT SIDEBAR ====
    auto *sidebarWidget = new QWidget(m_chatPanel);
    sidebarWidget->setMinimumWidth(160);
    sidebarWidget->setMaximumWidth(350);
    sidebarWidget->setStyleSheet("background: #07101a;");
    auto *sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(8, 12, 8, 8);
    sidebarLayout->setSpacing(6);

    // Search bar
    m_searchInput = new QLineEdit(sidebarWidget);
    m_searchInput->setPlaceholderText("Search users...");
    m_searchInput->setStyleSheet(
        "QLineEdit { background: #020810; color: #c9d1d9;"
        "  border: 1px solid #30363d; border-radius: 8px;"
        "  padding: 6px 10px; font-size: 12px; }"
        "QLineEdit:focus { border-color: #2196f3; }");
    connect(m_searchInput, &QLineEdit::returnPressed, this, &ChatWindow::onSearchUsers);
    sidebarLayout->addWidget(m_searchInput);

    // Sidebar buttons
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(4);
    auto *addFriendBtn = new QPushButton("+Friend", sidebarWidget);
    addFriendBtn->setToolTip("Add a friend by username");
    addFriendBtn->setCursor(Qt::PointingHandCursor);
    addFriendBtn->setStyleSheet(
        "QPushButton { background: #21262d; color: #c9d1d9; border: none;"
        "  border-radius: 4px; padding: 4px 8px; font-size: 11px; }"
        "QPushButton:hover { background: #30363d; }");
    connect(addFriendBtn, &QPushButton::clicked, this, &ChatWindow::onAddFriend);
    btnRow->addWidget(addFriendBtn);

    auto *createGrpBtn = new QPushButton("+Group", sidebarWidget);
    createGrpBtn->setToolTip("Create a new chat group");
    createGrpBtn->setCursor(Qt::PointingHandCursor);
    createGrpBtn->setStyleSheet(addFriendBtn->styleSheet());
    connect(createGrpBtn, &QPushButton::clicked, this, &ChatWindow::onCreateGroup);
    btnRow->addWidget(createGrpBtn);

    auto *joinGrpBtn = new QPushButton("Join", sidebarWidget);
    joinGrpBtn->setToolTip("Join a chat group by ID");
    joinGrpBtn->setCursor(Qt::PointingHandCursor);
    joinGrpBtn->setStyleSheet(addFriendBtn->styleSheet());
    connect(joinGrpBtn, &QPushButton::clicked, this, &ChatWindow::onJoinGroup);
    btnRow->addWidget(joinGrpBtn);

    auto *manageBtn = new QPushButton("GrpMgr", sidebarWidget);
    manageBtn->setCursor(Qt::PointingHandCursor);
    manageBtn->setToolTip("Manage friend groups");
    manageBtn->setStyleSheet(joinGrpBtn->styleSheet());
    connect(manageBtn, &QPushButton::clicked, this, &ChatWindow::onManageGroups);
    btnRow->addWidget(manageBtn);
    btnRow->addStretch();

    m_notifyBell = new QPushButton(QString::fromUtf8("🔔"), sidebarWidget);
    m_notifyBell->setCursor(Qt::PointingHandCursor);
    m_notifyBell->setToolTip("Friend requests");
    m_notifyBell->setStyleSheet(
        "QPushButton { background: transparent; color: #8b949e; border: none;"
        "  font-size: 16px; padding: 0 4px; }"
        "QPushButton:hover { color: #f0883e; }");
    m_notifyBell->setVisible(false);
    connect(m_notifyBell, &QPushButton::clicked, this, &ChatWindow::onShowFriendRequests);
    btnRow->addWidget(m_notifyBell);
    sidebarLayout->addLayout(btnRow);

    auto *sidebarTitle = new QLabel("Chats", sidebarWidget);
    sidebarTitle->setStyleSheet(
        "color: #8b949e; font-size: 11px; font-weight: bold;"
        "padding: 0 8px; letter-spacing: 1px;");
    sidebarLayout->addWidget(sidebarTitle);

    m_sidebar = new QTreeWidget(sidebarWidget);
    m_sidebar->setHeaderHidden(true);
    m_sidebar->setIndentation(16);
    m_sidebar->setAnimated(true);
    m_sidebar->setStyleSheet(
        "QTreeWidget { background: transparent; color: #c9d1d9; border: none;"
        "  font-size: 13px; outline: none; }"
        "QTreeWidget::item { padding: 6px 10px; border-radius: 6px; }"
        "QTreeWidget::item:hover { background: #21262d; }"
        "QTreeWidget::item:selected { background: rgba(33,150,243,0.25); color: #c9d1d9; }"
        "QTreeWidget::branch:has-children:!has-siblings:closed,"
        "QTreeWidget::branch:closed:has-children:has-siblings {"
        "  border-image: none; image: none; }"
        "QTreeWidget::branch:open:has-children:!has-siblings,"
        "QTreeWidget::branch:open:has-children:has-siblings {"
        "  border-image: none; image: none; }");
    connect(m_sidebar, &QTreeWidget::itemClicked,
            this, &ChatWindow::onSidebarClicked);
    connect(m_sidebar, &QTreeWidget::itemDoubleClicked,
            this, &ChatWindow::onUserDoubleClicked);
    m_sidebar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sidebar, &QTreeWidget::customContextMenuRequested,
            this, &ChatWindow::onSidebarContextMenu);
    sidebarLayout->addWidget(m_sidebar);

    auto *sidebarHint = new QLabel("Click to chat\nDouble-click to pop out", sidebarWidget);
    sidebarHint->setStyleSheet("color: #484f58; font-size: 10px; padding: 4px 8px;");
    sidebarHint->setWordWrap(true);
    sidebarLayout->addWidget(sidebarHint);

    // ==== RIGHT CHAT AREA ====
    auto *rightPanel = new QWidget(m_chatPanel);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // Chat header
    m_chatHeader = new QLabel("Public Chat", rightPanel);
    m_chatHeader->setStyleSheet(
        "color: #c9d1d9; font-size: 15px; font-weight: bold;"
        "padding: 10px 16px; background: #0a1628;"
        "border-bottom: 1px solid #30363d;");
    rightLayout->addWidget(m_chatHeader);

    // Message area (bubble scroll)
    m_bubbleScroll = new QScrollArea(rightPanel);
    m_bubbleScroll->setWidgetResizable(true);
    m_bubbleScroll->setStyleSheet(
        "QScrollArea { background: #020810; border: none; }"
        "QScrollBar:vertical { background: #020810; width: 6px; }"
        "QScrollBar::handle:vertical { background: #30363d; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    m_bubbleContainer = new QWidget;
    m_bubbleContainer->setStyleSheet("background: #020810;");
    auto *bubbleLayout = new QVBoxLayout(m_bubbleContainer);
    bubbleLayout->setContentsMargins(12, 12, 12, 12);
    bubbleLayout->setSpacing(8);
    bubbleLayout->addStretch();
    m_bubbleScroll->setWidget(m_bubbleContainer);
    rightLayout->addWidget(m_bubbleScroll);

    // Input bar
    auto *inputBar = new QWidget(rightPanel);
    inputBar->setStyleSheet("background: #0a1628; border-top: 1px solid #30363d;");
    auto *inputLayout = new QHBoxLayout(inputBar);
    inputLayout->setContentsMargins(12, 10, 12, 10);
    inputLayout->setSpacing(8);

    m_input = new QLineEdit(inputBar);
    m_input->setPlaceholderText("Type a message...");
    m_input->setStyleSheet(
        "QLineEdit { background: #07101a; color: #c9d1d9;"
        "  border: 1px solid #30363d; border-radius: 8px;"
        "  padding: 9px 14px; font-size: 14px; }"
        "QLineEdit:focus { border-color: #2196f3; background: #1c2333; }");
    connect(m_input, &QLineEdit::returnPressed, this, [this]() { onSendClicked(); });
    inputLayout->addWidget(m_input);

    m_emojiBtn = new QPushButton(QString::fromUtf8("😊"), inputBar);
    m_emojiBtn->setToolTip("Emoji picker");
    m_emojiBtn->setCursor(Qt::PointingHandCursor);
    m_emojiBtn->setFixedWidth(36);
    m_emojiBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; font-size: 18px; }"
        "QPushButton:hover { background: #21262d; border-radius: 6px; }");
    connect(m_emojiBtn, &QPushButton::clicked, this, &ChatWindow::onEmojiClicked);
    inputLayout->addWidget(m_emojiBtn);

    m_sendBtn = new QPushButton("Send", inputBar);
    m_sendBtn->setToolTip("Send message (Enter)");
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #2196f3,stop:1 #1565c0); color: white; border: none;"
        "  border-radius: 8px; padding: 9px 20px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #42a5f5,stop:1 #1976d2); }");
    connect(m_sendBtn, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
    inputLayout->addWidget(m_sendBtn);

    rightLayout->addWidget(inputBar);

    // Bottom bar
    auto *bottomBar = new QHBoxLayout;
    bottomBar->setContentsMargins(12, 6, 12, 8);
    bottomBar->setSpacing(8);

    auto *disconnectBtn = new QPushButton("Disconnect", rightPanel);
    disconnectBtn->setToolTip("Disconnect and return to login");
    disconnectBtn->setCursor(Qt::PointingHandCursor);
    disconnectBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #f85149;"
        "  border: 1px solid #f85149; border-radius: 6px; padding: 5px 14px; font-size: 11px; }"
        "QPushButton:hover { background: rgba(248,81,73,0.15); }");
    connect(disconnectBtn, &QPushButton::clicked, this, [this]() { m_client->disconnectFromServer(); });
    bottomBar->addWidget(disconnectBtn);

    auto *deleteBtn = new QPushButton("Delete Account", rightPanel);
    deleteBtn->setToolTip("Permanently delete your account");
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setStyleSheet(
        "QPushButton { background: rgba(248,81,73,0.1); color: #f85149;"
        "  border: 1px solid rgba(248,81,73,0.4); border-radius: 6px;"
        "  padding: 5px 14px; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(248,81,73,0.25); }");
    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
        auto result = QMessageBox::question(this, "Delete Account",
            "Are you sure you want to permanently delete your account?\n"
            "This action cannot be undone.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (result == QMessageBox::Yes)
            m_client->sendDeleteAccount();
    });
    bottomBar->addWidget(deleteBtn);

    m_themeBtn = new QPushButton(QString::fromUtf8("🌙"), rightPanel);
    m_themeBtn->setToolTip("Toggle dark/light theme");
    m_themeBtn->setCursor(Qt::PointingHandCursor);
    m_themeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; font-size: 14px; padding: 4px 8px; }"
        "QPushButton:hover { background: #21262d; border-radius: 4px; }");
    connect(m_themeBtn, &QPushButton::clicked, this, &ChatWindow::onToggleTheme);
    bottomBar->addWidget(m_themeBtn);

    m_exportBtn = new QPushButton("Export", rightPanel);
    m_exportBtn->setToolTip("Export chat history");
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    m_exportBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #8b949e; border: 1px solid #30363d;"
        "  border-radius: 4px; padding: 3px 10px; font-size: 11px; }"
        "QPushButton:hover { background: #21262d; color: #c9d1d9; }");
    connect(m_exportBtn, &QPushButton::clicked, this, &ChatWindow::onExportChat);
    bottomBar->addWidget(m_exportBtn);
    bottomBar->addStretch();

    rightLayout->addLayout(bottomBar);

    splitter->addWidget(sidebarWidget);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({200, 520});
    panelLayout->addWidget(splitter);
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
        "QPushButton { color: #2196f3; font-size: 13px; font-weight: bold;"
        "  border: none; padding: 0; text-align: left; }"
        "QPushButton:hover { color: #1565c0; }");
    backBtn->setVisible(false);
    connect(backBtn, &QPushButton::clicked, this, &ChatWindow::onToggleMode);
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    auto *title = new QLabel("Chat Room", card);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 26px; font-weight: bold; color: #1a3a5c;");
    layout->addWidget(title);

    m_subtitle = new QLabel("Login to your account", card);
    m_subtitle->setObjectName("loginSubtitle");
    m_subtitle->setAlignment(Qt::AlignCenter);
    m_subtitle->setStyleSheet("font-size: 13px; color: #888; margin-bottom: 2px;");
    layout->addWidget(m_subtitle);

    QString inputStyle =
        "QLineEdit { border: 1px solid #ddd; border-radius: 8px;"
        "  padding: 9px 14px; font-size: 14px; background: #f8f9fa; color: #333; }"
        "QLineEdit:focus { border-color: #2196f3; background: white; }";

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
        "你最喜欢的老师叫什么？",
        "Custom question..."
    });
    m_questionCombo->setStyleSheet(
        "QComboBox { border: 1px solid #ddd; border-radius: 8px;"
        "  padding: 8px 12px; font-size: 13px; background: #f8f9fa; color: #333; }"
        "QComboBox:hover { border-color: #2196f3; }"
        "QComboBox QAbstractItemView { background: white; selection-background-color: #e8e8ff; }");
    m_questionCombo->setVisible(false);
    layout->addWidget(m_questionCombo);

    // Custom question input (shown when "Custom..." is selected)
    m_customQuestionInput = new QLineEdit(card);
    m_customQuestionInput->setPlaceholderText("Write your own security question...");
    m_customQuestionInput->setStyleSheet(inputStyle);
    m_customQuestionInput->setVisible(false);
    layout->addWidget(m_customQuestionInput);

    connect(m_questionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                m_customQuestionInput->setVisible(
                    idx == m_questionCombo->count() - 1);
            });

    // Security answer (register only)
    m_answerInput = new QLineEdit(card);
    m_answerInput->setPlaceholderText("Answer to security question");
    m_answerInput->setStyleSheet(inputStyle);
    m_answerInput->setVisible(false);
    layout->addWidget(m_answerInput);

    // Button
    m_loginBtn = new QPushButton("Login", card);
    m_loginBtn->setToolTip("Connect and authenticate");
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    m_loginBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #2196f3,stop:1 #1565c0);"
        "  color: white; border: none; border-radius: 8px; padding: 11px 0; font-size: 15px; font-weight: bold; }"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #42a5f5,stop:1 #1976d2); }"
        "QPushButton:pressed { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #1e88e5,stop:1 #0d47a1); }"
        "QPushButton:disabled { background: #ccc; color: #999; }");
    connect(m_loginBtn, &QPushButton::clicked, this, &ChatWindow::onLoginClicked);
    layout->addWidget(m_loginBtn);

    m_modeToggle = new QPushButton("Don't have an account? Register", card);
    m_modeToggle->setToolTip("Switch between Login and Register");
    m_modeToggle->setCursor(Qt::PointingHandCursor);
    m_modeToggle->setFlat(true);
    m_modeToggle->setStyleSheet("QPushButton { color: #2196f3; font-size: 12px; border: none; }"
                                "QPushButton:hover { color: #1565c0; text-decoration: underline; }");
    connect(m_modeToggle, &QPushButton::clicked, this, &ChatWindow::onToggleMode);
    layout->addWidget(m_modeToggle);

    m_forgotLink = new QPushButton("Forgot Password?", card);
    m_forgotLink->setToolTip("Reset password via security question");
    m_forgotLink->setCursor(Qt::PointingHandCursor);
    m_forgotLink->setFlat(true);
    m_forgotLink->setStyleSheet("QPushButton { color: #999; font-size: 11px; border: none; }"
                                "QPushButton:hover { color: #2196f3; text-decoration: underline; }");
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
        "QLineEdit:focus { border-color: #2196f3; background: white; }";

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
    m_forgotBtn->setToolTip("Find your security question");
    m_forgotBtn->setCursor(Qt::PointingHandCursor);
    m_forgotBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #2196f3,stop:1 #1565c0);"
        "  color: white; border: none; border-radius: 8px; padding: 11px 0; font-size: 15px; font-weight: bold; }"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #42a5f5,stop:1 #1976d2); }"
        "QPushButton:pressed { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #1e88e5,stop:1 #0d47a1); }"
        "QPushButton:disabled { background: #ccc; color: #999; }");
    connect(m_forgotBtn, &QPushButton::clicked, this, &ChatWindow::onForgotClicked);
    layout->addWidget(m_forgotBtn);

    m_backBtn = new QPushButton("Back to Login", card);
    m_backBtn->setToolTip("Return to login page");
    m_backBtn->setCursor(Qt::PointingHandCursor);
    m_backBtn->setFlat(true);
    m_backBtn->setStyleSheet("QPushButton { color: #2196f3; font-size: 12px; border: none; }"
                             "QPushButton:hover { color: #1565c0; text-decoration: underline; }");
    connect(m_backBtn, &QPushButton::clicked, this, &ChatWindow::onBackToLogin);
    layout->addWidget(m_backBtn);

    layout->addStretch();
}

void ChatWindow::applyStyles()
{
    // No longer needed — bubble rendering replaces it
}

QString ChatWindow::avatarLetter(const QString &name) const
{
    return name.isEmpty() ? "?" : name.left(1).toUpper();
}

void ChatWindow::switchConversation(const QString &conv)
{
    m_activeConv = conv;
    m_prevDate.clear();
    m_chatHeader->setText(conv.isEmpty() ? QString::fromUtf8("📢 Public Chat")
                                         : QString("Chat with %1").arg(conv));
    m_input->setPlaceholderText(conv.isEmpty()
        ? "Broadcast to everyone..."
        : QString("Message @%1...").arg(conv));
    renderMessages();
}

void ChatWindow::renderMessages()
{
    // Clear existing bubbles
    auto *layout = qobject_cast<QVBoxLayout *>(m_bubbleContainer->layout());
    if (!layout) return;
    while (layout->count() > 1) {
        QLayoutItem *item = layout->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }
    m_prevDate.clear();

    const auto &msgs = m_chatHistory.value(m_activeConv);
    for (const auto &msg : msgs) {
        QWidget *bubble = buildBubble(msg);
        layout->insertWidget(layout->count() - 1, bubble);
    }
    scrollToBottom();
}

QWidget *ChatWindow::buildBubble(const ChatMessage &msg)
{
    bool fromMe = (msg.sender == m_myUsername);
    QString dateStr = msg.timestamp.toString("yyyy-MM-dd");
    QString timeStr = msg.timestamp.toString("hh:mm:ss");

    auto *wrapper = new QWidget;
    auto *outerLayout = new QVBoxLayout(wrapper);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(2);

    if (dateStr != m_prevDate) {
        m_prevDate = dateStr;
        auto *dateLabel = new QLabel(dateStr);
        dateLabel->setAlignment(Qt::AlignCenter);
        dateLabel->setStyleSheet("color: #8b949e; font-size: 11px; padding: 6px 0;");
        outerLayout->addWidget(dateLabel);
    }

    auto *timeLabel = new QLabel(timeStr);
    timeLabel->setAlignment(fromMe ? Qt::AlignRight : Qt::AlignLeft);
    timeLabel->setStyleSheet("color: #484f58; font-size: 10px; padding: 0 46px;");
    outerLayout->addWidget(timeLabel);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);

    auto *avatar = new QLabel(avatarLetter(msg.sender));
    avatar->setFixedSize(34, 34);
    avatar->setAlignment(Qt::AlignCenter);
    QString avatarBg = fromMe ? "#2196f3" : "#30363d";
    avatar->setStyleSheet(
        QString("background: %1; color: white; border-radius: 17px;"
                "font-size: 14px; font-weight: bold;").arg(avatarBg));

    auto *bubble = new QFrame;
    bubble->setMaximumWidth(300);
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

    // @mention highlight
    bool mentioned = !fromMe && !m_myUsername.isEmpty()
                     && msg.content.contains("@" + m_myUsername, Qt::CaseInsensitive);
    QString bubbleColor = fromMe ? "#1a3a5c" : (mentioned ? "#3d1a00" : "#21262d");
    QString border = mentioned ? "border: 2px solid #f0883e;" : "";
    bubble->setStyleSheet(
        QString("background: %1; border-radius: 10px; %2").arg(bubbleColor, border));

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

void ChatWindow::scrollToBottom()
{
    QTimer::singleShot(50, this, [this]() {
        auto *bar = m_bubbleScroll->verticalScrollBar();
        if (bar) bar->setValue(bar->maximum());
    });
}

void ChatWindow::updateSidebar()
{
    m_sidebar->blockSignals(true);
    m_sidebar->clear();

    // Public chat
    auto *pubItem = new QTreeWidgetItem(m_sidebar);
    pubItem->setText(0, QString::fromUtf8("📢  Public Chat"));
    pubItem->setData(0, Qt::UserRole, QString());

    // Me divider
    auto *meDiv = new QTreeWidgetItem(m_sidebar);
    meDiv->setText(0, "── Me ──");
    meDiv->setFlags(Qt::NoItemFlags);
    meDiv->setForeground(0, QColor("#484f58"));
    meDiv->setTextAlignment(0, Qt::AlignCenter);

    auto *meItem = new QTreeWidgetItem(m_sidebar);
    meItem->setText(0, QString::fromUtf8("🧑  %1 (you)").arg(m_myUsername));
    meItem->setFlags(Qt::NoItemFlags);

    // Friends section
    if (!m_friends.isEmpty() || !m_existingGroups.isEmpty()) {
        auto *fdiv = new QTreeWidgetItem(m_sidebar);
        fdiv->setText(0, "── Friends ──");
        fdiv->setFlags(Qt::NoItemFlags);
        fdiv->setForeground(0, QColor("#484f58"));

        QHash<QString, QStringList> grouped;
        for (const auto &f : m_friends) {
            QString g = m_friendGroups.value(f, "Default");
            grouped[g].append(f);
        }
        for (const auto &g : m_existingGroups) {
            if (!grouped.contains(g))
                grouped[g] = {};
        }
        QStringList groupNames = grouped.keys();
        groupNames.sort();

        for (const auto &group : groupNames) {
            auto *gh = new QTreeWidgetItem(m_sidebar);
            gh->setText(0, QString::fromUtf8("📁  %1").arg(group));
            gh->setFlags(gh->flags() & ~Qt::ItemIsSelectable);
            gh->setForeground(0, QColor("#6e7681"));
            gh->setData(0, Qt::UserRole, QString("__grp_%1").arg(group)); // tag as group header

            bool hasFriends = !grouped[group].isEmpty();
            if (!hasFriends) {
                auto *empty = new QTreeWidgetItem(gh);
                empty->setText(0, "  (empty)");
                empty->setFlags(Qt::NoItemFlags);
                empty->setForeground(0, QColor("#484f58"));
            } else {
                for (const auto &u : grouped[group]) {
                    auto *item = new QTreeWidgetItem(gh);
                    bool online = m_onlineUsers.contains(u);
                    QString icon = online ? QString::fromUtf8("🟢") : QString::fromUtf8("⚫");
                    item->setText(0, QString("  %1 %2").arg(icon, u));
                    item->setData(0, Qt::UserRole, u);
                }
            }
            m_sidebar->expandItem(gh);  // default: expanded — but user wants collapsed!
        }
    }

    // Chat groups
    if (!m_groups.isEmpty()) {
        auto *gdiv = new QTreeWidgetItem(m_sidebar);
        gdiv->setText(0, "── Groups ──");
        gdiv->setFlags(Qt::NoItemFlags);
        gdiv->setForeground(0, QColor("#484f58"));

        for (const auto &g : m_groups) {
            auto *item = new QTreeWidgetItem(m_sidebar);
            QString key = QString("#%1").arg(g.id);
            item->setText(0, QString::fromUtf8("👥  %1 (%2)").arg(g.name).arg(g.memberCount));
            item->setData(0, Qt::UserRole, key);
        }
    }

    m_sidebar->blockSignals(false);

    // Collapse all friend group headers by default
    for (int i = 0; i < m_sidebar->topLevelItemCount(); ++i) {
        auto *top = m_sidebar->topLevelItem(i);
        for (int j = 0; j < top->childCount(); ++j) {
            auto *child = top->child(j);
            QString d = child->data(0, Qt::UserRole).toString();
            if (d.startsWith("__grp_"))
                m_sidebar->collapseItem(child);
        }
    }
}

void ChatWindow::onSidebarClicked(QTreeWidgetItem *item, int)
{
    if (!item || !(item->flags() & Qt::ItemIsSelectable)) return;
    QString conv = item->data(0, Qt::UserRole).toString();
    if (conv.isEmpty() || conv.startsWith("__grp_")) return;
    switchConversation(conv);
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
        // Check custom question if selected
        if (m_questionCombo->currentIndex() == m_questionCombo->count() - 1
            && m_customQuestionInput->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Error", "Please write your custom security question.");
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
    m_cardStack->setFixedSize(380, 340);
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
    m_cardStack->setFixedSize(380, m_isRegisterMode ? 420 : 340);
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
    m_cardStack->setFixedSize(380, 420);
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
    m_customQuestionInput->setVisible(
        registerMode && m_questionCombo->currentIndex() == m_questionCombo->count() - 1);
    m_answerInput->setVisible(registerMode);

    auto *backBtn = m_cardStack->findChild<QPushButton *>("registerBackBtn");
    if (backBtn) backBtn->setVisible(registerMode);

    m_cardStack->setFixedSize(380, registerMode ? 420 : 340);

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

    if (m_activeConv.isEmpty()) {
        m_client->sendBroadcast(text);
    } else {
        m_client->sendPrivate(m_activeConv, text);
    }

    m_input->clear();
    m_input->setFocus();
}

void ChatWindow::onMessageReceived(const QString &sender, const QString &content,
                                    const QString &timestamp, const QString &scope,
                                    const QString &to)
{
    bool fromMe = (sender == m_myUsername);

    ChatMessage msg{sender, content, QDateTime::currentDateTime()};

    if (scope == "private") {
        QString peer = fromMe ? to : sender;

        // Tray notification if window is hidden
        if (!fromMe && (isMinimized() || !isVisible()))
            showTrayNotification(sender, content);

        // Store in persistent history
        m_chatHistory[peer].append(msg);

        // Forward to private dialog if open
        if (m_dialogs.contains(peer))
            m_dialogs[peer]->addMessage(msg);

        // Render if currently viewing this conversation
        if (m_activeConv == peer)
            renderMessages();
    } else {
        // Broadcast or group — store under empty key (public)
        m_chatHistory[QString()].append(msg);
        if (m_activeConv.isEmpty())
            renderMessages();
    }
}

void ChatWindow::onSystemMessage(const QString &content)
{
    ChatMessage msg{"System", content, QDateTime::currentDateTime()};
    m_chatHistory[QString()].append(msg);
    if (m_activeConv.isEmpty())
        renderMessages();
}

void ChatWindow::onConnected()
{
    // Forgot password flow
    if (m_cardStack->currentIndex() == 1) {
        m_client->sendForgotPassword(m_resetUsername);
        return;
    }

    if (m_isRegisterMode) {
        QString question;
        if (m_questionCombo->currentIndex() == m_questionCombo->count() - 1)
            question = m_customQuestionInput->text().trimmed();
        else
            question = m_questionCombo->currentText();
        m_client->sendRegister(m_myUsername, m_passwordInput->text(),
                               question, m_answerInput->text().trimmed());
    }
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
    ChatMessage sysMsg{"System", "Disconnected from server", QDateTime::currentDateTime()};
    m_chatHistory[QString()].append(sysMsg);

    for (auto *d : m_dialogs)
        d->close();
    m_dialogs.clear();

    setMinimumSize(440, 420);
    resize(500, 500);
}

void ChatWindow::onLoginSuccess(const QString &username)
{
    m_myUsername = username;
    setWindowTitle(QString("Chat Room — %1").arg(username));

    m_loginPanel->setVisible(false);
    m_chatPanel->setVisible(true);
    m_loginBtn->setEnabled(true);

    // Init sidebar with public chat selected
    m_activeConv = QString();
    updateSidebar();
    switchConversation(QString());
    m_client->sendFriendList();
    m_client->sendListGroups();
    m_client->sendListPendingRequests();
    m_client->sendListFriendGroups();

    setMinimumSize(680, 480);
    resize(720, 520);
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
    updateSidebar();
}

void ChatWindow::onConnectionError(const QString &error)
{
    m_loginBtn->setEnabled(true);
    m_modeToggle->setEnabled(true);
    m_loginPanel->setVisible(true);
    m_chatPanel->setVisible(false);
    QMessageBox::warning(this, "Connection Error", error);
}

void ChatWindow::onUserDoubleClicked(QTreeWidgetItem *item, int)
{
    if (!item) return;
    QString user = item->data(0, Qt::UserRole).toString();
    if (user.isEmpty() || user == m_myUsername || user.startsWith("__grp_")) return;
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

// ========== Group Chat ==========

void ChatWindow::onCreateGroup()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Create Group", "Group name:",
                                          QLineEdit::Normal, "", &ok);
    if (ok && !name.trimmed().isEmpty())
        m_client->sendCreateGroup(name.trimmed());
}

void ChatWindow::onJoinGroup()
{
    bool ok;
    int gid = QInputDialog::getInt(this, "Join Group", "Group ID:", 1, 1, 9999, 1, &ok);
    if (ok) m_client->sendJoinGroup(gid);
}

void ChatWindow::onGroupCreated(int groupId, const QString &name)
{
    m_groups.append({groupId, name, m_myUsername, 1});
    updateSidebar();
    QMessageBox::information(this, "Group Created",
        QString("Group '%1' created! Share the ID (%2) for others to join.").arg(name).arg(groupId));
}

void ChatWindow::onGroupJoined(int groupId, const QString &name)
{
    m_client->sendListGroups();
    switchConversation(QString("#%1").arg(groupId));
    QMessageBox::information(this, "Joined", QString("You joined '%1'").arg(name));
}

void ChatWindow::onGroupListReceived(const QJsonArray &groups)
{
    m_groups.clear();
    for (int i = 0; i < groups.size(); ++i) {
        QJsonObject g = groups[i].toObject();
        GroupInfo info;
        info.id = g["id"].toInt();
        info.name = g["name"].toString();
        info.owner = g["owner"].toString();
        info.memberCount = g["members"].toInt();
        m_groups.append(info);
    }
    updateSidebar();
}

void ChatWindow::onGroupError(const QString &error)
{
    QMessageBox::warning(this, "Group Error", error);
}

// ========== Friend System ==========

void ChatWindow::onSearchUsers()
{
    QString q = m_searchInput->text().trimmed();
    if (q.isEmpty()) return;
    m_client->sendSearchUsers(q);
}

void ChatWindow::onSearchResults(const QString &query, const QStringList &users)
{
    if (users.isEmpty()) {
        QMessageBox::information(this, "Search", QString("No users matching '%1'").arg(query));
        return;
    }
    QStringList items;
    for (const auto &u : users) items << u;
    bool ok;
    QString sel = QInputDialog::getItem(this, "Search Results",
        QString("Users matching '%1':").arg(query), items, 0, false, &ok);
    if (ok && !sel.isEmpty())
        m_client->sendFriendRequest(sel);
}

void ChatWindow::onAddFriend()
{
    QString name = QInputDialog::getText(this, "Add Friend", "Username:");
    if (!name.isEmpty()) m_client->sendFriendRequest(name);
}

void ChatWindow::onFriendRequestReceived(const QString &from)
{
    if (!m_pendingFriends.contains(from)) {
        m_pendingFriends.append(from);
        updateNotifyBell();
        showTrayNotification("Friend Request",
            QString("%1 wants to be your friend").arg(from));
    }
}

void ChatWindow::onPendingRequestsReceived(const QStringList &requests)
{
    m_pendingFriends = requests;
    updateNotifyBell();
}

void ChatWindow::onShowFriendRequests()
{
    if (m_pendingFriends.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Friend Requests");
    dlg.setMinimumWidth(400);
    dlg.setStyleSheet(
        "QDialog { background: #0a1628; }"
        "QLabel { color: #c9d1d9; font-size: 13px; }");

    auto *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(10);

    QLabel *title = new QLabel(QString("You have %1 pending friend request(s)").arg(m_pendingFriends.size()));
    title->setStyleSheet("font-size: 15px; font-weight: bold; color: #c9d1d9; padding: 8px 0;");
    layout->addWidget(title);

    // Build request list
    QStringList pending = m_pendingFriends; // copy since we modify during loop
    for (const auto &user : pending) {
        auto *row = new QHBoxLayout;
        auto *label = new QLabel(QString::fromUtf8("👤  %1").arg(user));
        label->setMinimumWidth(160);
        label->setStyleSheet("color: #e6e6e6; font-size: 14px;");
        row->addWidget(label);

        auto *acceptBtn = new QPushButton("Accept");
        acceptBtn->setStyleSheet(
            "QPushButton { background: #238636; color: white; border: none; border-radius: 6px;"
            "  padding: 6px 16px; font-size: 12px; font-weight: bold; }"
            "QPushButton:hover { background: #2ea043; }");
        QString userCopy = user;
        connect(acceptBtn, &QPushButton::clicked, &dlg, [this, &dlg, userCopy]() {
            m_client->sendFriendResponse(userCopy, "accept");
            m_pendingFriends.removeAll(userCopy);
            updateNotifyBell();
            dlg.accept();
        });
        row->addWidget(acceptBtn);

        auto *ignoreBtn = new QPushButton("Ignore");
        ignoreBtn->setStyleSheet(
            "QPushButton { background: #30363d; color: #c9d1d9; border: 1px solid #484f58;"
            "  border-radius: 6px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { background: #484f58; }");
        connect(ignoreBtn, &QPushButton::clicked, &dlg, [this, &dlg, userCopy]() {
            m_client->sendFriendResponse(userCopy, "ignore");
            m_pendingFriends.removeAll(userCopy);
            updateNotifyBell();
            dlg.accept();
        });
        row->addWidget(ignoreBtn);

        auto *rejectBtn = new QPushButton("Reject");
        rejectBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #f85149; border: 1px solid #f85149;"
            "  border-radius: 6px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { background: rgba(248,81,73,0.15); }");
        connect(rejectBtn, &QPushButton::clicked, &dlg, [this, &dlg, userCopy]() {
            m_client->sendFriendResponse(userCopy, "reject");
            m_pendingFriends.removeAll(userCopy);
            updateNotifyBell();
            dlg.accept();
        });
        row->addWidget(rejectBtn);

        layout->addLayout(row);
    }

    dlg.exec();
}

void ChatWindow::updateNotifyBell()
{
    int count = m_pendingFriends.size();
    m_notifyBell->setVisible(count > 0);
    if (count > 0)
        m_notifyBell->setText(QString::fromUtf8("🔔 %1").arg(count));
}

void ChatWindow::onFriendAdded(const QString &friendName)
{
    if (!m_friends.contains(friendName)) {
        m_friends.append(friendName);
        m_friends.sort();
        updateSidebar();
    }
}

void ChatWindow::onFriendListReceived(const QJsonArray &friends)
{
    m_friends.clear();
    m_friendGroups.clear();
    for (int i = 0; i < friends.size(); ++i) {
        QJsonObject f = friends[i].toObject();
        QString name = f["name"].toString();
        m_friends.append(name);
        m_friendGroups[name] = f["group"].toString();
    }
    m_friends.sort();
    updateSidebar();
}

void ChatWindow::onFriendGroupSet(const QString &friendName, const QString &group)
{
    m_friendGroups[friendName] = group;
    if (!group.isEmpty() && !m_existingGroups.contains(group))
        m_existingGroups.append(group);
    updateSidebar();
}

void ChatWindow::onFriendGroupsReceived(const QJsonArray &groups)
{
    // Merge server groups with local ones (preserve empty groups)
    for (int i = 0; i < groups.size(); ++i) {
        QString name = groups[i].toObject()["name"].toString();
        if (!name.isEmpty() && !m_existingGroups.contains(name))
            m_existingGroups.append(name);
    }
}

void ChatWindow::onFriendGroupRenamed(const QString &oldName, const QString &newName)
{
    m_existingGroups.removeAll(oldName);
    if (!newName.isEmpty() && !m_existingGroups.contains(newName))
        m_existingGroups.append(newName);
    // Update all friends in renamed group
    for (auto it = m_friendGroups.begin(); it != m_friendGroups.end(); ++it)
        if (it.value() == oldName) it.value() = newName;
    updateSidebar();
}

void ChatWindow::onFriendGroupDeleted(const QString &groupName)
{
    m_existingGroups.removeAll(groupName);
    for (auto it = m_friendGroups.begin(); it != m_friendGroups.end(); ++it)
        if (it.value() == groupName) it.value().clear();
    updateSidebar();
}

void ChatWindow::onManageGroups()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Manage Friend Groups");
    dlg.setMinimumSize(450, 400);
    dlg.setStyleSheet("QDialog { background: #0a1628; }");

    auto *mainL = new QVBoxLayout(&dlg);
    mainL->setSpacing(10);

    // Top: create new group
    auto *createRow = new QHBoxLayout;
    auto *newGrpInput = new QLineEdit;
    newGrpInput->setPlaceholderText("New group name");
    newGrpInput->setStyleSheet(
        "QLineEdit { background: #07101a; color: #c9d1d9; border: 1px solid #30363d;"
        "  border-radius: 6px; padding: 6px 10px; }");
    createRow->addWidget(newGrpInput);

    auto *createBtn = new QPushButton("Create Group");
    createBtn->setStyleSheet("QPushButton { background:#238636; color:white; border:none;"
        "  border-radius:6px; padding:6px 14px; font-weight:bold; } QPushButton:hover { background:#2ea043; }");
    createRow->addWidget(createBtn);
    mainL->addLayout(createRow);

    // Group list (refreshed on create)
    QLabel *listLabel = new QLabel("Your groups:");
    listLabel->setStyleSheet("color:#8b949e; font-size:12px; font-weight:bold;");
    mainL->addWidget(listLabel);

    // We'll use a scroll area with group cards
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    auto *grpContainer = new QWidget;
    auto *grpLayout = new QVBoxLayout(grpContainer);
    grpLayout->setSpacing(6);
    grpLayout->addStretch();

    auto refreshGroups = [&]() {
        // Clear existing
        while (grpLayout->count() > 1) {
            auto *item = grpLayout->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        for (const auto &group : m_existingGroups) {
            // Count friends in this group
            int cnt = 0;
            for (const auto &f : m_friends)
                if (m_friendGroups.value(f) == group) cnt++;

            auto *card = new QWidget;
            card->setStyleSheet("background:#07101a; border-radius:8px; padding:8px;");
            auto *cl = new QVBoxLayout(card);
            cl->setSpacing(4);
            auto *header = new QHBoxLayout;
            auto *nameLabel = new QLabel(QString::fromUtf8("📁 %1  (%2 friends)").arg(group).arg(cnt));
            nameLabel->setStyleSheet("color:#c9d1d9; font-weight:bold;");
            header->addWidget(nameLabel);
            header->addStretch();

            auto *renBtn = new QPushButton("Rename");
            renBtn->setStyleSheet("QPushButton { background:#21262d; color:#c9d1d9; border:none;"
                "  border-radius:4px; padding:3px 10px; font-size:11px; } QPushButton:hover { background:#30363d; }");
            header->addWidget(renBtn);

            auto *delBtn = new QPushButton("Delete");
            delBtn->setStyleSheet("QPushButton { background:transparent; color:#f85149; border:1px solid #f85149;"
                "  border-radius:4px; padding:3px 10px; font-size:11px; } QPushButton:hover { background:rgba(248,81,73,0.15); }");
            header->addWidget(delBtn);
            cl->addLayout(header);

            // Add friend to group
            auto *addRow = new QHBoxLayout;
            QStringList notInGroup;
            for (const auto &f : m_friends)
                if (m_friendGroups.value(f) != group) notInGroup << f;
            if (!notInGroup.isEmpty()) {
                QComboBox *friendCombo = new QComboBox;
                friendCombo->setStyleSheet(
                    "QComboBox { background:#020810; color:#c9d1d9; border:1px solid #30363d;"
                    "  border-radius:4px; padding:4px; font-size:12px; }");
                friendCombo->addItem("Add friend...");
                for (const auto &f : notInGroup) friendCombo->addItem(f);
                addRow->addWidget(friendCombo);

                auto *addBtn = new QPushButton("Add");
                addBtn->setStyleSheet("QPushButton { background:#238636; color:white; border:none;"
                    "  border-radius:4px; padding:4px 12px; font-size:11px; } QPushButton:hover { background:#2ea043; }");
                addRow->addWidget(addBtn);
                addRow->addStretch();

                QString grpCopy = group;
                connect(addBtn, &QPushButton::clicked, &dlg, [this, friendCombo, grpCopy]() {
                    QString f = friendCombo->currentText();
                    if (f != "Add friend...") m_client->sendSetFriendGroup(f, grpCopy);
                });
                cl->addLayout(addRow);
            }

            // Rename action
            QString oldGrp = group;
            connect(renBtn, &QPushButton::clicked, &dlg, [&dlg, oldGrp, this]() {
                bool ok;
                QString nn = QInputDialog::getText(&dlg, "Rename Group",
                    "New name:", QLineEdit::Normal, oldGrp, &ok);
                if (ok && !nn.trimmed().isEmpty() && nn.trimmed() != oldGrp)
                    m_client->sendRenameFriendGroup(oldGrp, nn.trimmed());
            });
            // Delete action
            connect(delBtn, &QPushButton::clicked, &dlg, [&dlg, oldGrp, this]() {
                if (QMessageBox::question(&dlg, "Delete Group",
                        QString("Delete '%1'? Friends return to default.").arg(oldGrp)) == QMessageBox::Yes)
                    m_client->sendDeleteFriendGroup(oldGrp);
            });

            grpLayout->insertWidget(grpLayout->count() - 1, card);
        }
    };

    refreshGroups();
    connect(createBtn, &QPushButton::clicked, &dlg, [&, newGrpInput]() {
        QString name = newGrpInput->text().trimmed();
        if (!name.isEmpty()) {
            m_client->sendCreateFriendGroup(name);
            if (!m_existingGroups.contains(name))
                m_existingGroups.append(name);
            newGrpInput->clear();
            refreshGroups();
            newGrpInput->setFocus();
        }
    });

    scroll->setWidget(grpContainer);
    mainL->addWidget(scroll);
    dlg.exec();
    updateSidebar();
}

void ChatWindow::onSidebarContextMenu(const QPoint &pos)
{
    auto *item = m_sidebar->itemAt(pos);
    if (!item) return;
    QString user = item->data(0, Qt::UserRole).toString();
    if (user.isEmpty() || user == m_myUsername || user.startsWith("__grp_")) return;
    if (!m_friends.contains(user)) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background: #07101a; color: #c9d1d9; border: 1px solid #30363d; }"
        "QMenu::item { padding: 6px 30px 6px 12px; }"
        "QMenu::item:selected { background: #21262d; }"
        "QMenu::separator { background: #30363d; height: 1px; }");

    QAction *groupAct = menu.addAction("Move to group...");
    menu.addSeparator();
    QAction *removeAct = menu.addAction("Remove friend");

    QAction *chosen = menu.exec(m_sidebar->mapToGlobal(pos));
    if (chosen == groupAct) {
        // Build dialog with combo box of existing groups + custom option
        QDialog dlg(this);
        dlg.setWindowTitle("Move to Group");
        dlg.setMinimumWidth(280);
        dlg.setStyleSheet("QDialog { background: #0a1628; } QLabel { color: #c9d1d9; }");
        auto *dl = new QVBoxLayout(&dlg);
        dl->addWidget(new QLabel(QString("Choose group for %1:").arg(user)));

        QComboBox *combo = new QComboBox(&dlg);
        combo->setEditable(true);
        combo->setStyleSheet(
            "QComboBox { background: #07101a; color: #c9d1d9; border: 1px solid #30363d;"
            "  border-radius: 6px; padding: 6px; }");
        combo->addItem(""); // default (no group)
        for (const auto &g : m_existingGroups)
            if (!g.isEmpty()) combo->addItem(g);
        // Set current if already in a group
        QString cur = m_friendGroups.value(user);
        int idx = combo->findText(cur);
        if (idx >= 0) combo->setCurrentIndex(idx);
        else if (!cur.isEmpty()) { combo->addItem(cur); combo->setCurrentText(cur); }
        dl->addWidget(combo);

        auto *bb = new QHBoxLayout;
        bb->addStretch();
        auto *okBtn = new QPushButton("OK");
        okBtn->setStyleSheet("QPushButton { background: #238636; color: white; border: none;"
            "  border-radius: 6px; padding: 6px 20px; } QPushButton:hover { background: #2ea043; }");
        connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        bb->addWidget(okBtn);
        dl->addLayout(bb);

        if (dlg.exec() == QDialog::Accepted) {
            QString grp = combo->currentText().trimmed();
            m_client->sendSetFriendGroup(user, grp);
        }
    } else if (chosen == removeAct) {
        auto r = QMessageBox::question(this, "Remove Friend",
            QString("Remove %1 from friends?").arg(user));
        if (r == QMessageBox::Yes) {
            m_friends.removeAll(user);
            m_friendGroups.remove(user);
            updateSidebar();
        }
    }
}

void ChatWindow::onFriendError(const QString &error)
{
    QMessageBox::warning(this, "Friend Error", error);
}

// ========== New Features ==========

void ChatWindow::setupSystemTray()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    m_trayIcon->setToolTip("Chat Room");

    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction("Show", this, [this]() { show(); raise(); activateWindow(); });
    m_trayMenu->addAction("Quit", this, []() { QApplication::quit(); });
    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &ChatWindow::onTrayActivated);
    connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this, [this]() {
        show(); raise(); activateWindow();
    });

    m_trayIcon->show();
}

void ChatWindow::showTrayNotification(const QString &title, const QString &msg)
{
    if (m_trayIcon && m_trayIcon->isVisible())
        m_trayIcon->showMessage(title, msg, QSystemTrayIcon::Information, 5000);
}

void ChatWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        show(); raise(); activateWindow();
    }
}

// ========== Emoji ==========

void ChatWindow::onEmojiClicked()
{
    QStringList emojis = {
        "😀","😂","🤣","😊","😍","🤩","😎","🥳","😢","😡",
        "👍","👎","👏","🙌","💪","🤝","❤️","💔","🔥","⭐",
        "🎉","🎂","🍕","☕","🎵","📷","💻","🚀","💡","✅"
    };
    QDialog dlg(this);
    dlg.setWindowTitle("Emoji");
    dlg.setFixedSize(300, 200);
    dlg.setStyleSheet("QDialog { background: #0a1628; }");
    auto *grid = new QGridLayout(&dlg);
    for (int i = 0; i < emojis.size(); ++i) {
        auto *btn = new QPushButton(emojis[i]);
        btn->setFixedSize(36, 36);
        btn->setStyleSheet("QPushButton { font-size: 20px; border: none; background: transparent; }"
                           "QPushButton:hover { background: #21262d; border-radius: 6px; }");
        QString emoji = emojis[i];
        connect(btn, &QPushButton::clicked, &dlg, [this, &dlg, emoji]() {
            m_input->insert(emoji);
            dlg.accept();
        });
        grid->addWidget(btn, i / 10, i % 10);
    }
    dlg.exec();
}

// ========== Typing Indicator ==========

void ChatWindow::onTypingChanged()
{
    if (!m_isTyping && !m_activeConv.isEmpty() && !m_input->text().isEmpty()) {
        m_isTyping = true;
        m_client->sendTyping(m_activeConv);
    }
    m_typingTimer->start();
}

void ChatWindow::onTypingReceived(const QString &from)
{
    if (m_activeConv == from) {
        QString old = m_chatHeader->text();
        if (!old.contains("typing...")) {
            m_chatHeader->setText(QString::fromUtf8("Chat with %1  ✏️ typing...").arg(from));
            QTimer::singleShot(3000, this, [this, old]() {
                m_chatHeader->setText(old);
            });
        }
    }
}

// ========== Theme Toggle ==========

void ChatWindow::onToggleTheme()
{
    m_darkTheme = !m_darkTheme;
    m_themeBtn->setText(m_darkTheme ? QString::fromUtf8("🌙") : QString::fromUtf8("☀️"));
    m_themeBtn->setToolTip(m_darkTheme ? "Switch to light theme" : "Switch to dark theme");
    applyTheme();
}

void ChatWindow::applyTheme()
{
    if (m_darkTheme) {
        m_chatPanel->setStyleSheet("#chatPanel { background: #0a1628; }");
        m_bubbleContainer->setStyleSheet("background: #020810;");
        // etc. — already the default
    } else {
        m_chatPanel->setStyleSheet("#chatPanel { background: #f0f2f5; }");
        m_bubbleContainer->setStyleSheet("background: #ffffff;");
        m_chatHeader->setStyleSheet(
            "color: #333; font-size: 15px; font-weight: bold;"
            "padding: 10px 16px; background: #f0f2f5;"
            "border-bottom: 1px solid #e0e0e0;");
        m_input->setStyleSheet(
            "QLineEdit { background: white; color: #333;"
            "  border: 1px solid #e0e0e0; border-radius: 8px;"
            "  padding: 9px 14px; font-size: 14px; }");
    }
    renderMessages();
}

// ========== Chat Export ==========

void ChatWindow::onExportChat()
{
    QString filename = QFileDialog::getSaveFileName(this, "Export Chat",
        QString("chat_%1.txt").arg(m_activeConv.isEmpty() ? "public" : m_activeConv),
        "Text files (*.txt);;HTML files (*.html)");
    if (filename.isEmpty()) return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);

    bool html = filename.endsWith(".html");
    if (html) out << "<html><head><meta charset='utf-8'><title>Chat Export</title></head><body>\n";

    const auto &msgs = m_chatHistory.value(m_activeConv);
    for (const auto &msg : msgs) {
        QString ts = msg.timestamp.toString("yyyy-MM-dd hh:mm:ss");
        if (html)
            out << "<p><b>[" << ts << "] " << msg.sender.toHtmlEscaped() << ":</b> "
                << msg.content.toHtmlEscaped() << "</p>\n";
        else
            out << "[" << ts << "] " << msg.sender << ": " << msg.content << "\n";
    }
    if (html) out << "</body></html>\n";
    file.close();

    showTrayNotification("Export Complete", QString("Saved to %1").arg(filename));
}

void ChatWindow::onDialogClosed(const QString &peer)
{
    m_dialogs.remove(peer);
}
