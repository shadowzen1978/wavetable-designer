@echo off
if not defined WTD_VCVARSALL exit /b 2
call "%WTD_VCVARSALL%" x64 >nul
if errorlevel 1 exit /b 1
set
