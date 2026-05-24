@echo off
chcp 65001 >nul
REM 先编译好 ChatClient，再双击本脚本，会启动两个独立的客户端进程。

set "EXE=%~dp0build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug\ChatClient.exe"

if not exist "%EXE%" (
    echo 找不到 ChatClient.exe，请先在 Qt Creator 里构建一次 ChatClient。
    echo 期望路径：
    echo   %EXE%
    echo.
    echo 若你的构建目录名不同，用记事本打开本 bat，修改 EXE 那一行路径。
    pause
    exit /b 1
)

echo 正在启动两个 ChatClient（请确保 ChatServer 已运行）...
start "ChatClient-1" "%EXE%"
timeout /t 1 /nobreak >nul
start "ChatClient-2" "%EXE%"
echo 已启动。两个窗口请用不同账号分别登录。
pause
