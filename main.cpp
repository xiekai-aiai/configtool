#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QFile>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    const QString fileName = QStringLiteral(":/styles/app.qss");
    QFile file(fileName );
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        qDebug() << "failed qss file:" << fileName;
        return 0;
    }

    const QString style = file.readAll();
    a.setStyleSheet(style);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "ConfigTool_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    a.setWindowIcon(QIcon(":/icons/app.ico"));

    MainWindow w;
    w.show();
    return a.exec();
}
