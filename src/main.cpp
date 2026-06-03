#include <QApplication>
#include "ui.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    Importer w;
    w.show();
    return a.exec();
}
