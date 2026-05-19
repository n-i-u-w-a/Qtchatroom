#pragma once

#include <QWidget>
#include <QHash>
#include "privatechatdialog.h"

class QTextEdit;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QComboBox;
class QLabel;
class QStackedWidget;
class QScrollArea;
#include <QSystemTrayIcon>
class QMenu;
class QTimer;
class ChatClient;

struct GroupInfo {
    int id;
    QString name;
    QString owner;
    int memberCount;
};

class ChatWindow : public QWidget {
    Q_OBJECT
public:
    explicit ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow() override;

private slots:
    void onSendClicked();
    void onLoginClicked();
    void onToggleMode();
    void onForgotClicked();
    void onResetClicked();
    void onBackToLogin();
    void onMessageReceived(const QString &sender, const QString &content,
                           const QString &timestamp, const QString &scope,
                           const QString &to);
    void onSystemMessage(const QString &content);
    void onConnected();
    void onDisconnected();
    void onLoginSuccess(const QString &username);
    void onLoginError(const QString &error);
    void onSecurityQuestion(const QString &username, const QString &question);
    void onForgotError(const QString &error);
    void onPasswordResetOk(const QString &message);
    void onResetError(const QString &error);
    void onUserListReceived(const QStringList &users);
    void onConnectionError(const QString &error);
    void onSidebarClicked(QTreeWidgetItem *item, int column);
    void onUserDoubleClicked(QTreeWidgetItem *item, int column);
    void onDialogClosed(const QString &peer);

    // Group
    void onCreateGroup(); void onJoinGroup();
    void onGroupCreated(int groupId, const QString &name);
    void onGroupJoined(int groupId, const QString &name);
    void onGroupListReceived(const QJsonArray &groups);
    void onGroupError(const QString &error);

    // Friend
    void onSearchUsers();
    void onSearchResults(const QString &query, const QStringList &users);
    void onAddFriend();
    void onFriendRequestReceived(const QString &from);
    void onPendingRequestsReceived(const QStringList &requests);
    void onShowFriendRequests();
    void onFriendAdded(const QString &friendName);
    void onFriendListReceived(const QJsonArray &friends);
    void onFriendGroupSet(const QString &f, const QString &g);
    void onFriendGroupRenamed(const QString &o, const QString &n);
    void onFriendGroupDeleted(const QString &groupName);
    void onFriendGroupsReceived(const QJsonArray &groups);
    void onManageGroups();
    void onFriendError(const QString &error);
    void onSidebarContextMenu(const QPoint &pos);

    // New features
    void onEmojiClicked();
    void onTypingChanged();
    void onTypingReceived(const QString &from);
    void onToggleTheme();
    void onExportChat();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void buildUi();
    void buildAuthCard(QWidget *card);
    void buildForgotCard(QWidget *card);
    void buildChatPanel();
    void applyStyles();
    void applyTheme();
    void openPrivateChat(const QString &peer);
    void setLoginMode(bool registerMode);
    void switchToForgotMode();
    void switchConversation(const QString &conv);
    void renderMessages();
    void updateNotifyBell();
    QWidget *buildBubble(const ChatMessage &msg);
    void scrollToBottom();
    QString avatarLetter(const QString &name) const;
    void updateSidebar();
    void showTrayNotification(const QString &title, const QString &msg);
    void setupSystemTray();

    // Auth
    QLineEdit *m_usernameInput;
    QLineEdit *m_passwordInput;
    QComboBox *m_questionCombo;
    QLineEdit *m_customQuestionInput;
    QLineEdit *m_answerInput;
    QPushButton *m_loginBtn;
    QPushButton *m_modeToggle;
    QPushButton *m_forgotLink;
    QLabel *m_subtitle;
    bool m_isRegisterMode = false;

    // Forgot
    QLineEdit *m_forgotUsername;
    QLabel *m_forgotQuestion;
    QLineEdit *m_forgotAnswer;
    QLineEdit *m_forgotNewPass;
    QPushButton *m_forgotBtn;
    QPushButton *m_backBtn;
    QString m_resetUsername;
    QStackedWidget *m_cardStack;

    // Chat
    QLineEdit *m_searchInput;
    QTreeWidget *m_sidebar;
    QLabel *m_chatHeader;
    QScrollArea *m_bubbleScroll;
    QWidget *m_bubbleContainer;
    QLineEdit *m_input;
    QPushButton *m_sendBtn;
    QPushButton *m_emojiBtn;
    QPushButton *m_themeBtn;
    QPushButton *m_exportBtn;

    QWidget *m_loginPanel;
    QWidget *m_chatPanel;

    ChatClient *m_client;
    QString m_host;
    quint16 m_port;
    QString m_myUsername;
    QStringList m_onlineUsers;
    QStringList m_friends;
    QHash<QString, QString> m_friendGroups;
    QList<GroupInfo> m_groups;
    QStringList m_pendingFriends;
    QStringList m_existingGroups;
    QPushButton *m_notifyBell;
    QString m_activeConv;
    QString m_prevDate;
    bool m_darkTheme = true;
    QTimer *m_typingTimer;
    bool m_isTyping = false;

    QHash<QString, PrivateChatDialog *> m_dialogs;
    QHash<QString, QList<ChatMessage>> m_chatHistory;

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
};
