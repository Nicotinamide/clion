@echo off
rem 使用UTF-8编码，解决中文显示问题
chcp 65001
echo 正在设置UTF-8编码...
echo 当前代码页: %errorlevel%

rem Set up OpenCASCADE environment
echo 正在设置OpenCASCADE环境...
call E:\CodesE\OCCT\INSTALL\env.bat

rem Add VTK bin directory to PATH
echo 正在添加VTK到PATH...
set PATH=E:\CodesE\VTK\bin;%PATH%

rem Start the application
echo 正在启动应用程序...
start "" "%~dp0cmake-build-Release\SprayR.exe"

echo 应用程序已启动！

