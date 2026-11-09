@echo off
setlocal
cd /d "%~dp0"

where gcc >nul 2>nul
if errorlevel 1 (
    echo ERRO: GCC/MinGW-w64 nao encontrado no PATH.
    echo Instale ou configure o compilador e tente novamente.
    pause
    exit /b 1
)

echo.
echo ==========================================================
echo APS - COMPILACAO DO NUCLEO ORIGINAL EM C
echo ==========================================================
call build.bat
if errorlevel 1 goto falha

echo.
echo ==========================================================
echo APS - COMPILACAO DA INTERFACE GRAFICA WINDOWS
echo ==========================================================
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8 ^
    gui\aps_interface_win.c -o APS_Interface.exe ^
    -mwindows -static-libgcc -lgdi32 -luser32 -lcomdlg32 -lkernel32 -lm
if errorlevel 1 goto falha

if not exist gui_saida mkdir gui_saida
echo.
echo OK: aps_ordenacao.exe e APS_Interface.exe foram gerados.
echo Para abrir a interface, execute APS_Interface.exe
exit /b 0

:falha
echo.
echo ERRO: Falha na compilacao. Verifique as mensagens acima.
pause
exit /b 1
