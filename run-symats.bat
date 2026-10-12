@echo off
title Symats Symbolic Mathematics Software
echo ====================================================
echo                 Symats Software
echo ====================================================
echo.
echo Launching Symats Notebook in your browser...
if exist "app\dist\index.html" (
    start "" "app\dist\index.html"
) else (
    echo Note: App bundle not found in app\dist\.
)
echo.
echo Starting Symats Interactive REPL Engine...
if exist "symats-cli.exe" (
    "symats-cli.exe"
) else if exist "build\cli\symats-cli.exe" (
    "build\cli\symats-cli.exe"
) else (
    echo symats-cli.exe not found.
    pause
)
