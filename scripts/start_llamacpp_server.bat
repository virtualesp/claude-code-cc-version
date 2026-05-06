@echo off
REM Copyright 2026 Prosophor Contributors
REM SPDX-License-Identifier: Apache-2.0
REM
REM Start llama-server in background and output PID.
REM Reads config from settings.json (local_models[0]).
REM
REM Outputs: PID on stdout on success
REM Exit codes:
REM   0 = success (PID on stdout)
REM   1 = config not found
REM   2 = server binary not found
REM   3 = model file not found
REM   4 = failed to launch

setlocal enabledelayedexpansion

set "CONFIG_PATH=%USERPROFILE%\.prosophor\settings.json"
if not exist "!CONFIG_PATH!" (
    >&2 echo Config not found: !CONFIG_PATH!
    exit /b 1
)

REM Read config via PowerShell JSON parser
for /f "usebackq tokens=*" %%i in (`
    powershell -NoProfile -Command ^
        "$c=Get-Content '!CONFIG_PATH!' | ConvertFrom-Json; $lm=$c.local_models[0]; ^
         $mp=$lm.model_path_for_win; if(-not $mp){$mp=$lm.model_path}; ^
         Write-Output ($lm.server_path + '|' + $mp + '|' + $lm.port + '|' + $lm.n_gpu_layers + '|' + $lm.n_threads)"
`) do set "CFG=%%i"

if "!CFG!"=="" (
    >&2 echo Failed to parse config: !CONFIG_PATH!
    exit /b 1
)

for /f "tokens=1-5 delims=|" %%a in ("!CFG!") do (
    set "SERVER_PATH=%%a"
    set "MODEL_PATH=%%b"
    set "PORT=%%c"
    set "NGL=%%d"
    set "THREADS=%%e"
)

REM Auto-detect server binary relative to script location if not configured
if "!SERVER_PATH!"=="" (
    set "SCRIPT_DIR=%~dp0"
    for %%I in ("!SCRIPT_DIR!..") do set "SERVER_PATH=%%~fI\llama-server.exe"
)
if not exist "!SERVER_PATH!" (
    >&2 echo Server binary not found: !SERVER_PATH!
    exit /b 2
)
if not exist "!MODEL_PATH!" (
    >&2 echo Model file not found: !MODEL_PATH!
    exit /b 3
)

REM Build llama-server args
set "PS_ARGS=-m !MODEL_PATH! --port !PORT! --host 127.0.0.1 -c 4096"
if "!NGL!"=="-1" (
    set "PS_ARGS=!PS_ARGS! -ngl 999"
) else if not "!NGL!"=="0" (
    set "PS_ARGS=!PS_ARGS! -ngl !NGL!"
)
if not "!THREADS!"=="0" (
    set "PS_ARGS=!PS_ARGS! -t !THREADS!"
)

REM Write temp PowerShell script to avoid quoting issues
set "TMPFILE=%TEMP%\prosophor_llama_start_%RANDOM%.ps1"
(
echo $p = Start-Process -FilePath "!SERVER_PATH:\=\\!" -ArgumentList "!PS_ARGS:\=\\!" -WindowStyle Hidden -PassThru;
echo Write-Output $p.Id
) > "%TMPFILE%" 2>nul

for /f "usebackq tokens=*" %%i in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%TMPFILE%" 2^>nul`) do set "PID=%%i"
del "%TMPFILE%" 2>nul

if not defined PID set PID=0
echo !PID!
exit /b 0
