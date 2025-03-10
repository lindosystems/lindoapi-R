REM Description: Create a static library from R.dll
REM Date: 2021-06-02
@echo off
set "MACHINE_TYPE=%1"

if /i "%MACHINE_TYPE%"=="x86" (
    set "MACHINE_FLAG=x86"
) else if /i "%MACHINE_TYPE%"=="x64" (
    set "MACHINE_FLAG=x64"
) else (
    echo No machine type specified. Defaulting to x64.
    set "MACHINE_FLAG=x64"
)

for /f "delims=" %%i in ('where R.dll 2^>nul') do set "R_DLL_PATH=%%i"
if not defined R_DLL_PATH (
    echo R.dll not found. Please ensure it is installed.
    exit /b
)
echo %R_DLL_PATH%
dumpbin /exports %R_DLL_PATH% > a
grep -E '^[[:space:]]*[0-9]+[[:space:]]+[0-9a-fA-F]+' a | grep -v "date stamp" > b
echo LIBRARY R > R.def
echo EXPORTS >> R.def
awk -F" " "{ print $4 }" b >> R.def
lib /def:R.def /out:R.lib /machine:%MACHINE_FLAG%