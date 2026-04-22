@echo off
setlocal
set ROOT=%~dp0
set INC=-I"%ROOT%include"
set SRC=%ROOT%src
set OUT=%ROOT%build
if not exist "%OUT%" mkdir "%OUT%"

gcc -std=c11 -Wall -Wextra -O2 %INC% -c "%SRC%\main.c" -o "%OUT%\main.o"
if errorlevel 1 exit /b 1
gcc -std=c11 -Wall -Wextra -O2 %INC% -c "%SRC%\lexer.c" -o "%OUT%\lexer.o"
if errorlevel 1 exit /b 1
gcc -std=c11 -Wall -Wextra -O2 %INC% -c "%SRC%\parse.c" -o "%OUT%\parse.o"
if errorlevel 1 exit /b 1
gcc -std=c11 -Wall -Wextra -O2 %INC% -c "%SRC%\ast.c" -o "%OUT%\ast.o"
if errorlevel 1 exit /b 1
gcc -std=c11 -Wall -Wextra -O2 %INC% -c "%SRC%\value.c" -o "%OUT%\value.o"
if errorlevel 1 exit /b 1
gcc -std=c11 -Wall -Wextra -O2 %INC% -c "%SRC%\interp.c" -o "%OUT%\interp.o"
if errorlevel 1 exit /b 1
gcc -std=c11 -Wall -Wextra -O2 %INC% -c "%SRC%\pathutil.c" -o "%OUT%\pathutil.o"
if errorlevel 1 exit /b 1

gcc -o "%ROOT%flow.exe" "%OUT%\main.o" "%OUT%\lexer.o" "%OUT%\parse.o" "%OUT%\ast.o" "%OUT%\value.o" "%OUT%\interp.o" "%OUT%\pathutil.o"
if errorlevel 1 exit /b 1
echo Built "%ROOT%flow.exe"
