#include "ui/MainWindow.h"

#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("wjcCN");
#ifdef QT_DEBUG
    app.setApplicationName("MusicPlayer");
#else
    app.setApplicationName("MusicPlayerRelease");
#endif
    app.setApplicationDisplayName("MusicPlayer");

    MainWindow window;
    window.show();
    return app.exec();
}
