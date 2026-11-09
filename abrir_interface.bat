@echo off
setlocal
cd /d "%~dp0"
if not exist APS_Interface.exe (
    echo Primeira execucao: compilando o projeto...
    call build_gui.bat
    if errorlevel 1 exit /b 1
)
if not exist aps_ordenacao.exe (
    echo O nucleo nao foi encontrado. Recompilando...
    call build_gui.bat
    if errorlevel 1 exit /b 1
)
start "" "%~dp0APS_Interface.exe"
exit /b 0
