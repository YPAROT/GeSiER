#include "mainwindow.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    bool smokeOk = false;
    const int smokeExit = qEnvironmentVariableIntValue("GESIER_SMOKE_EXIT_MS", &smokeOk);
    if (smokeOk && smokeExit >= 0)
        QTimer::singleShot(smokeExit, &a, &QCoreApplication::quit);
    return a.exec();
}
