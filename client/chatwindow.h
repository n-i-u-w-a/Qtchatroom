#pragma once

#include <QWidget>
#include <QHash>
#include "privatechatdialog.h"

class QTextEdit;
class QLineEdit;
class QPushButton;
class QListWidget;
class QListWidgetItem;
class QComboBox;
class QLabel;
class QStackedWidget;
class ChatClient;

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
    void onUserDoubleClicked(QListWidgetItem *item);
    void onDialogClosed(const QString &peer);

private:
    void buildUi();
    void buildAuthCard(QWidget *card);
    void buildForgotCard(QWidget *card);
    void applyStyles();
    void openPrivateChat(const QString &peer);
    void setLoginMode(bool registerMode);
    void switchToForgotMode();

    // Auth
    QLineEdit *m_usernameInput;
    QLineEdit *m_passwordInput;
    QComboBox *m_questionCombo;
    QLineEdit *m_answerInput;
    QPushButton *m_loginBtn;
    QPushButton *m_modeToggle;
    QPushButton *m_forgotLink;
    QLabel *m_subtitle;
    bool m_isRegisterMode = false;

    // Forgot password
    QLineEdit *m_forgotUsername;
    QLabel *m_forgotQuestion;
    QLineEdit *m_forgotAnswer;
    QLineEdit *m_forgotNewPass;
    QPushButton *m_forgotBtn;
    QPushButton *m_backBtn;
    QString m_resetUsername;

    // Card switching
    QStackedWidget *m_cardStack;

    // Chat
    QTextEdit *m_chatView;
    QListWidget *m_userList;
    QComboBox *m_targetCombo;
    QLineEdit *m_input;
    QPushButton *m_sendBtn;
    QPushButton *m_disconnectBtn;

    QWidget *m_loginPanel;
    QWidget *m_chatPanel;

    ChatClient *m_client;
    QString m_host;
    quint16 m_port;
    QString m_myUsername;
    QStringList m_onlineUsers;

    QHash<QString, PrivateChatDialog *> m_dialogs;
    QHash<QString, QList<ChatMessage>> m_chatHistory;
};
