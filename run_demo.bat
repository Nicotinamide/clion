@echo off
rem 使用UTF-8编码，解决中文显示问题
chcp 65001
echo 正在设置UTF-8编码...
echo 当前代码页查询返回值 (0为成功): %errorlevel%

rem Set up OpenCASCADE environment
echo 正在设置OpenCASCADE环境...
call E:\CodesE\OCCT\INSTALL\env.bat
echo OCCT env.bat 调用返回值 (0为成功): %errorlevel%

rem Add VTK bin directory to PATH
echo 正在添加VTK到PATH...
set PATH=E:\CodesE\VTK\bin;%PATH%

echo 环境设置脚本执行完毕！