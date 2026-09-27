@echo off
rem ---------------------------------------------------------------
rem  Windows 下双击即可编译固件（自动找 Git Bash）
rem ---------------------------------------------------------------
setlocal

set SCRIPT=%~dp0build.sh

where bash >nul 2>nul
if %ERRORLEVEL%==0 (
    bash "%SCRIPT%" %*
) else if exist "C:\Program Files\Git\bin\bash.exe" (
    "C:\Program Files\Git\bin\bash.exe" "%SCRIPT%" %*
) else (
    echo [ERROR] 没有找到 bash。请安装 Git for Windows，或在 Git Bash 中执行: ./build.sh
)

echo.
pause
