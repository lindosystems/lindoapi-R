@echo off
for /f "delims=" %%i in ('where R.dll 2^>nul') do set "R_DLL_PATH=%%i"
if not defined R_DLL_PATH (
    echo R.dll not found. Please ensure it is installed.
    exit /b
)
echo  %R_DLL_PATH%
dumpbin /exports %R_DLL_PATH% > a
grep -E '^[[:space:]]*[0-9]+[[:space:]]+[0-9]+' a | grep -v "date stamp" > b
echo LIBRARY R > R.def
echo EXPORTS >> R.def
awk -F" " '{ print $4 }' b >> R.def
lib /def:R.def /out:R.lib /machine:x64



