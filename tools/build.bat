@echo off
:: 一键编译烧录 — 双击即可
cd /d "%~dp0.."
set "IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5.4"
set "IDF_TOOLS_PATH=C:\Espressif"
set "IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.5_py3.11_env"
set "PATH=C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\bin;%IDF_PYTHON_ENV_PATH%\Scripts;%PATH%"
"C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe" "C:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py" build flash monitor
pause
