#include "src/AvStream.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    AvStream w;
    w.show();
    return a.exec();
}
