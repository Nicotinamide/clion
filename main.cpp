#include <QApplication>
#include "SprayR_GUI.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
    #ifdef _WIN32
    // 设置控制台输出编码为UTF-8，解决中文乱码问题
    SetConsoleOutputCP(CP_UTF8);
    #endif

    QApplication app(argc, argv);
    Spray_GUI mainWindow;
    mainWindow.show();
    return app.exec();
}

