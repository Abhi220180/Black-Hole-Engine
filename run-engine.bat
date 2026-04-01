@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "BOOTSTRAP=%SCRIPT_DIR%bootstrap.ps1"

if not exist "%BOOTSTRAP%" (
    echo ERROR: bootstrap.ps1 not found at "%BOOTSTRAP%"
    exit /b 1
)

:: Default launch path: CUDA + Release.
:: Any extra args are forwarded, so you can override, e.g.:
::   run-engine.bat -Mode cpu -Config Debug
powershell -NoProfile -ExecutionPolicy Bypass -File "%BOOTSTRAP%" -Mode cuda -Config Release %*
set "RC=%ERRORLEVEL%"
exit /b %RC%
