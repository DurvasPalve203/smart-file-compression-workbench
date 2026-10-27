@echo off
setlocal
echo [Compiling SFCW C++ Compression Engine]
if not exist "bin" mkdir bin
g++ -std=c++17 -O3 -I include src/container.cpp src/metrics.cpp src/trace.cpp src/huffman.cpp src/lzw.cpp src/rle.cpp src/main.cpp -o bin/sfcw_engine.exe
if %ERRORLEVEL% EQU 0 (
    echo [Build Successful: bin\sfcw_engine.exe]
) else (
    echo [Build Failed with code %ERRORLEVEL%]
    exit /b %ERRORLEVEL%
)
