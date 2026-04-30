#include <QCoreApplication>
#include <iostream>
#include "chatserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    ChatServer server;
    QObject::connect(&server, &ChatServer::logMessage,
                     [](const QString &msg) {
                         std::cout << msg.toStdString() << std::endl;
                     });

    quint16 port = 9527;
    while (!server.start(port)) {
        if (++port > 9536) {
            std::cerr << "No available port found" << std::endl;
            return 1;
        }
    }

    std::cout << "Press Ctrl+C to stop..." << std::endl;
    return app.exec();
}
