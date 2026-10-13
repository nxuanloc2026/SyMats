@echo off
setlocal
title Symats Symbolic Mathematics Software
if exist "%~dp0symats-cli.exe" goto packaged
echo symats-cli.exe was not found beside this launcher.
echo Extract the entire Windows package before running it.
exit /b 1
:packaged
"%~dp0symats-cli.exe" %*
exit /b %errorlevel%
