@echo off
setlocal
chcp 65001 >nul
set "app=%~dp0输出结果\WindowPin.exe"
if not exist "%app%" (
    echo 未找到 WindowPin.exe，请确认它位于项目的“输出结果”文件夹中。
    pause
    exit /b 1
)
start "" "%app%"
exit /b 0
