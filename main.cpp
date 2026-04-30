#include <QApplication>
#include <QWidget>
#include <QPushButton>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Qt Demo");
    window.setFixedSize(400, 300);

    QPushButton *btn = new QPushButton("Hello Qt!", &window);
    btn->setGeometry(120, 110, 160, 60);

    QObject::connect(btn, &QPushButton::clicked, &app, &QApplication::quit);

    window.show();
    return app.exec();
}
