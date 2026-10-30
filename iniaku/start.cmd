@echo off
cd /d "%~dp0"
xmrig.exe -c config.json %*
pause
