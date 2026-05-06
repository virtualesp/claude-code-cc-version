@echo off
REM Copyright 2026 Prosophor Contributors
REM SPDX-License-Identifier: Apache-2.0
REM
REM Stop llama-server by PID.
REM Usage: stop_llamacpp_server.bat <pid> [--force]
REM
REM Exit codes:
REM   0 = process stopped
REM   1 = missing PID
REM   2 = process not found
REM   3 = failed to stop

setlocal

if "%~1"=="" exit /b 1

set "PID=%~1"
set "FORCE="
if /i "%~2"=="--force" set "FORCE=/F"

taskkill %FORCE% /PID %PID% >nul 2>nul
if errorlevel 1 (
    exit /b 2
)
exit /b 0
