@echo off
setlocal

set PROJECT_ROOT=%~dp0..
set BUILD_DIR=%PROJECT_ROOT%\build_release

cmake -B "%BUILD_DIR%" -S "%PROJECT_ROOT%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBIMEUP_BUILD_TESTS=OFF

cmake --build "%BUILD_DIR%" --config Release -j %NUMBER_OF_PROCESSORS%

echo Release build complete. Binary: %BUILD_DIR%\bin\Release\bimeup.exe
