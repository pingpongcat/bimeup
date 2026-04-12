@echo off
setlocal

set PROJECT_ROOT=%~dp0..
set BUILD_DIR=%PROJECT_ROOT%\build_debug

cmake -B "%BUILD_DIR%" -S "%PROJECT_ROOT%" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DBIMEUP_BUILD_TESTS=ON

cmake --build "%BUILD_DIR%" --config Debug -j %NUMBER_OF_PROCESSORS%

echo Debug build complete. Binary: %BUILD_DIR%\bin\Debug\bimeup.exe
