#include "ui/MainWindow.h"

#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("wjcCN");
    app.setApplicationName("MusicPlayer");

    MainWindow window;
    window.show();
//AGENTS.md
    return app.exec();
}
