@echo off
cd /d "%~dp0.."

:: clear MSYS/MinGW env vars so idf.py main() is not skipped
set "MSYSTEM="
set "MSYSTEM_CHOST="
set "MSYSTEM_PREFIX="
set "MSYS2_ENV_CONV_EXCL="

:: clean PATH: Windows system dirs + ESP-IDF toolchain
set "PATH=C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\bin;C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;%SystemRoot%\system32;%SystemRoot%"

set "IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5.4"
set "IDF_TOOLS_PATH=C:\Espressif"
set "IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.5_py3.11_env"

"C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe" "C:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py" build
