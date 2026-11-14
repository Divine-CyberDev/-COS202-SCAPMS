#include "scapms/gui/MainWindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    application.setApplicationName("SCAPMS");
    application.setOrganizationName("COS202 Student Project");
    application.setStyle(QStyleFactory::create("Fusion"));

    scapms::MainWindow window;
    window.show();
    return application.exec();
}
