@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build

set FLAGS=-std=c11 -O2 -Wall -Wextra -Wconversion -pedantic -finput-charset=UTF-8 -fexec-charset=UTF-8

gcc %FLAGS% -Dmain=aps_core_main -c aps_ordenacao.c -o build\nucleo.o
if errorlevel 1 goto falha
gcc %FLAGS% -Iinclude -c src\menu.c -o build\menu.o
if errorlevel 1 goto falha
gcc %FLAGS% -Iinclude -c src\main.c -o build\main.o
if errorlevel 1 goto falha
gcc %FLAGS% build\nucleo.o build\menu.o build\main.o -o aps_ordenacao.exe
if errorlevel 1 goto falha

echo Compilacao concluida: aps_ordenacao.exe
exit /b 0

:falha
echo Falha ao compilar. Verifique se o GCC ou MinGW-w64 esta instalado.
exit /b 1
