@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0Importer ROM.ps1" %*
