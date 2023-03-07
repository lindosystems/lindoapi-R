@echo off 
echo.
echo ---------------------------------------
echo rLindo Installation script for Win32/64
echo         Lindo Systems, Inc.
echo ---------------------------------------

setlocal enabledelayedexpansion
set RCORE_VER=3.6.2
set RCORE_EXE=R-%RCORE_VER%-win.exe
set RTOOLS_VER=Rtools35.exe
set RMIN=3.5
rem ***************************Check whether R is installed***************************
echo Checking for R...
:CHECK_R_INSTALLATION
set R_INSTALLED=false
set R_PATH=
set R_PATH_VALUE_NAME=InstallPath
set R_PATH_REG_VERSION=Current Version

rem search HKEY_CURRENT_USER 64-bit
set KEY_NAME=HKEY_CURRENT_USER\SOFTWARE\R-core
FOR /F "tokens=4 delims=\" %%A IN (' "reg query "%KEY_NAME%" /reg:64 2>nul" ') DO (
    if "%%A" == "R" set R_INSTALLED=true
    if "%%A" == "R32" set R_INSTALLED=true
    if "%%A" == "R64" set R_INSTALLED=true

    if "!R_INSTALLED!" == "true" (
        rem set R installation path
        set R_PATH_KEY_NAME=%KEY_NAME%\%%A
        FOR /F "skip=3 tokens=2*" %%C IN ('REG QUERY "!R_PATH_KEY_NAME!" /v "%R_PATH_REG_VERSION%" /reg:64 2^>nul') DO (
            if "%%D" == !RCORE_VER! (@echo Found R !RCORE_VER!) else (set R_INSTALLED=false)
        )
    )
	
    if "!R_INSTALLED!" == "true" (
        rem set R installation path
        set R_PATH_KEY_NAME=%KEY_NAME%\%%A
        FOR /F "skip=2 tokens=2*" %%C IN ('REG QUERY "!R_PATH_KEY_NAME!" /v "%R_PATH_VALUE_NAME%" /reg:64 2^>nul') DO (
            set R_PATH=%%D
            goto CHECK_R_PATH
        )
    )
)

rem search HKEY_CURRENT_USER 32-bit
set KEY_NAME=HKEY_CURRENT_USER\SOFTWARE\R-core
FOR /F "tokens=4 delims=\" %%A IN (' "reg query "%KEY_NAME%" /reg:32 2>nul" ') DO (
    if "%%A" == "R" set R_INSTALLED=true
    if "%%A" == "R32" set R_INSTALLED=true
    if "%%A" == "R64" set R_INSTALLED=true

    if "!R_INSTALLED!" == "true" (
        rem set R installation path
        set R_PATH_KEY_NAME=%KEY_NAME%\%%A
        FOR /F "skip=3 tokens=2*" %%C IN ('REG QUERY "!R_PATH_KEY_NAME!" /v "%R_PATH_REG_VERSION%" /reg:32 2^>nul') DO (
            if "%%D" == !RCORE_VER! (@echo Found R !RCORE_VER!) else (set R_INSTALLED=false)
        )
    )
	
    if "!R_INSTALLED!" == "true" (
        rem set R installation path
        set R_PATH_KEY_NAME=%KEY_NAME%\%%A
        FOR /F "skip=2 tokens=2*" %%C IN ('REG QUERY "!R_PATH_KEY_NAME!" /v "%R_PATH_VALUE_NAME%" /reg:32 2^>nul') DO (
            set R_PATH=%%D
            goto CHECK_R_PATH
        )
    )
)

rem search HKEY_LOCAL_MACHINE 64-bit
set KEY_NAME=HKEY_LOCAL_MACHINE\SOFTWARE\R-core
FOR /F "tokens=4 delims=\" %%A IN (' "reg query "%KEY_NAME%" /reg:64 2>nul" ') DO (
    if "%%A" == "R" set R_INSTALLED=true
    if "%%A" == "R32" set R_INSTALLED=true
    if "%%A" == "R64" set R_INSTALLED=true

    if "!R_INSTALLED!" == "true" (
        rem set R installation path
        set R_PATH_KEY_NAME=%KEY_NAME%\%%A
        FOR /F "skip=3 tokens=2*" %%C IN ('REG QUERY "!R_PATH_KEY_NAME!" /v "%R_PATH_REG_VERSION%" /reg:64 2^>nul') DO (
            if "%%D" == !RCORE_VER! (@echo Found R !RCORE_VER!) else (set R_INSTALLED=false)
        )
    )
	
    if "!R_INSTALLED!" == "true" (		
        rem set R installation path
        set R_PATH_KEY_NAME=%KEY_NAME%\%%A        
        FOR /F "skip=2 tokens=2*" %%C IN ('REG QUERY "!R_PATH_KEY_NAME!" /v "%R_PATH_VALUE_NAME%" /reg:64 2^>nul') DO (
            set R_PATH=%%D
            goto CHECK_R_PATH
        )
    )
)

rem search HKEY_LOCAL_MACHINE 32-bit
set KEY_NAME=HKEY_LOCAL_MACHINE\SOFTWARE\R-core
FOR /F "tokens=4 delims=\" %%A IN (' "reg query "%KEY_NAME%" /reg:32 2>nul" ') DO (
    if "%%A" == "R" set R_INSTALLED=true
    if "%%A" == "R32" set R_INSTALLED=true
    if "%%A" == "R64" set R_INSTALLED=true

    if "!R_INSTALLED!" == "true" (
        rem set R installation path
        set R_PATH_KEY_NAME=%KEY_NAME%\%%A
        FOR /F "skip=3 tokens=2*" %%C IN ('REG QUERY "!R_PATH_KEY_NAME!" /v "%R_PATH_REG_VERSION%" /reg:32 2^>nul') DO (
            if "%%D" == !RCORE_VER! (@echo Found R !RCORE_VER!) else (set R_INSTALLED=false)
        )
    )
	
    if "!R_INSTALLED!" == "true" (
        rem set R installation path
        set R_PATH_KEY_NAME=%KEY_NAME%\%%A
        FOR /F "skip=2 tokens=2*" %%C IN ('REG QUERY "!R_PATH_KEY_NAME!" /v "%R_PATH_VALUE_NAME%" /reg:32 2^>nul') DO (
            set R_PATH=%%D
            goto CHECK_R_PATH
        )
    )
)

if not "!R_INSTALLED!"=="true" (
    set response=
    set /P response= R is not installed, would you like to download and install R (y/n^)?
    
    if "!response!"=="n" goto END
    if "!response!"=="N" goto END
    if "!response!"=="NO" goto END
    if "!response!"=="No" goto END
    if "!response!"=="no" goto END
    
    if not exist %RCORE_EXE% (
        if not exist wget.exe (
            echo ERROR: Can not find wget.exe.
            goto END
        )
        wget.exe --no-check-certificate https://cloud.r-project.org/bin/windows/base/old/%RCORE_VER%/%RCORE_EXE% -q --show-progress
        if not exist %RCORE_EXE% (
        	echo Failed to download %RCORE_EXE%
        	goto END
        )
        set /P response2= Downloading R completed, press Enter to continue...
    )
    
    %RCORE_EXE%
    if %errorlevel% neq 0 (
		echo ERROR: Installing R failed.
		goto END
    )
    
    rem check again to set R_PATH
    goto CHECK_R_INSTALLATION
)

:CHECK_R_PATH
echo R path : %R_PATH%
    set "PATH=!R_PATH!\bin;%PATH%"

:CHECK_R_VERSION
R --version 2>version.txt
set R_VERSION=
FOR /F "tokens=3 delims= " %%A IN (version.txt) DO (
    set R_VERSION=%%A
    goto CHECK_R_ARCHITETURE
)

:CHECK_R_ARCHITETURE
set R_ARCHITETURE=32bit
FOR /F "skip=2 tokens=3 delims= " %%A IN (version.txt) DO (
    set ARCH=%%A
    if not "!ARCH!" == "!ARCH:64=!" (
        set R_ARCHITETURE=64bit
        goto DELETE_VERSION_TXT
    )
)

:DELETE_VERSION_TXT
del version.txt

rem ***************************Check whether Rtools is installed***************************
echo Checking for Rtools...
:CHECK_RTOOLS_INSTALLATION
set RTOOLS_INSTALLED=false
set RTOOLS_PATH=
set RTOOLS_PATH_VALUE_NAME=InstallPath

rem search HKEY_CURRENT_USER 64-bit
set KEY_NAME=HKEY_CURRENT_USER\SOFTWARE\R-core
FOR /F "tokens=4 delims=\" %%A IN (' "reg query "%KEY_NAME%" /reg:64 2>nul" ') DO (
    if "%%A" == "Rtools" set RTOOLS_INSTALLED=true

    if "!RTOOLS_INSTALLED!" == "true" (
        rem set Rtools installation path
        set RTOOLS_PATH_KEY_NAME=%KEY_NAME%\%%A
        FOR /F "skip=2 tokens=2*" %%C IN ('REG QUERY "!RTOOLS_PATH_KEY_NAME!" /v "%RTOOLS_PATH_VALUE_NAME%" /reg:64 2^>nul') DO (
            set RTOOLS_PATH=%%D
            goto CHECK_RTOOLS_PATH
        )
    )
)

rem search HKEY_CURRENT_USER 32-bit
set KEY_NAME=HKEY_CURRENT_USER\SOFTWARE\R-core
FOR /F "tokens=4 delims=\" %%A IN (' "reg query "%KEY_NAME%" /reg:32 2>nul" ') DO (
    if "%%A" == "Rtools" set RTOOLS_INSTALLED=true

    if "!RTOOLS_INSTALLED!" == "true" (
        rem set Rtools installation path
        set RTOOLS_PATH_KEY_NAME=%KEY_NAME%\%%A
        FOR /F "skip=2 tokens=2*" %%C IN ('REG QUERY "!RTOOLS_PATH_KEY_NAME!" /v "%RTOOLS_PATH_VALUE_NAME%" /reg:32 2^>nul') DO (
            set RTOOLS_PATH=%%D
            goto CHECK_RTOOLS_PATH
        )
    )
)

rem search HKEY_LOCAL_MACHINE 64-bit
set KEY_NAME=HKEY_LOCAL_MACHINE\SOFTWARE\R-core
FOR /F "tokens=4 delims=\" %%A IN (' "reg query "%KEY_NAME%" /reg:64 2>nul" ') DO (
    if "%%A" == "Rtools" set RTOOLS_INSTALLED=true

    if "!RTOOLS_INSTALLED!" == "true" (
        rem set Rtools installation path
        set RTOOLS_PATH_KEY_NAME=%KEY_NAME%\%%A
        FOR /F "skip=2 tokens=2*" %%C IN ('REG QUERY "!RTOOLS_PATH_KEY_NAME!" /v "%RTOOLS_PATH_VALUE_NAME%" /reg:64 2^>nul') DO (
            set RTOOLS_PATH=%%D
            goto CHECK_RTOOLS_PATH
        )
    )
)

rem search HKEY_LOCAL_MACHINE 32-bit
set KEY_NAME=HKEY_LOCAL_MACHINE\SOFTWARE\R-core
FOR /F "tokens=4 delims=\" %%A IN (' "reg query "%KEY_NAME%" /reg:32 2>nul" ') DO (
    if "%%A" == "Rtools" set RTOOLS_INSTALLED=true

    if "!RTOOLS_INSTALLED!" == "true" (
        rem set Rtools installation path
        set RTOOLS_PATH_KEY_NAME=%KEY_NAME%\%%A
        FOR /F "skip=2 tokens=2*" %%C IN ('REG QUERY "!RTOOLS_PATH_KEY_NAME!" /v "%RTOOLS_PATH_VALUE_NAME%" /reg:32 2^>nul') DO (
            set RTOOLS_PATH=%%D
            goto CHECK_RTOOLS_PATH
        )
    )
)

if not "!RTOOLS_INSTALLED!"=="true" (
    set response=
    set /P response= Rtools is not installed, would you like to download and install Rtools (y/n^)?
    
    if "!response!"=="n" goto END
    if "!response!"=="N" goto END
    if "!response!"=="NO" goto END
    if "!response!"=="No" goto END
    if "!response!"=="no" goto END
    
    if not exist %RTOOLS_VER% (
        if not exist wget.exe (
            echo ERROR: Can not find wget.exe.
            goto END
        )
        wget.exe --no-check-certificate https://cran.r-project.org/bin/windows/Rtools/%RTOOLS_VER% -q --show-progress
        set /P response2= Downloading Rtools completed, press Enter to continue...
    )
    
    %RTOOLS_VER%
    if %errorlevel% neq 0 (
		echo ERROR: Installing Rtools failed.
		goto END
    )
    
    rem check again to set RTOOLS_PATH
    goto CHECK_RTOOLS_INSTALLATION
)

:CHECK_RTOOLS_PATH
echo RTOOLS path : %RTOOLS_PATH%

rem add RTOOLS_PATH to PATH
if "!R_ARCHITETURE!" == "64bit" (
    if not exist !RTOOLS_PATH!\mingw_64\bin\gcc.exe (
        echo ERROR: R is 64-bit, but can not find 64-bit gcc compiler. ^
Please reinstall Rtools, and make sure to check "64 bit toolchain" when installing.
        goto END
    )
    set "PATH=!RTOOLS_PATH!\bin;!RTOOLS_PATH!\mingw_64\bin;%PATH%"
) else (
    if not exist !RTOOLS_PATH!\mingw_32\bin\gcc.exe (
        echo ERROR: R is 32-bit, but can not find 32-bit gcc compiler. ^
Please reinstall Rtools, and make sure to check "32 bit toolchain" when installing.
        
        goto END
    )
    set "PATH=!RTOOLS_PATH!\bin;!RTOOLS_PATH!\mingw_32\bin;%PATH%"
)
rem echo %PATH%
:check for LINDO API
if "%LINDOAPI_HOME%" == "" (
	echo ERROR: Environment variable LINDOAPI_HOME is not defined to your installation ^
path of LINDO API.
	goto END
)
if not exist %LINDOAPI_HOME%/lib. (
	echo ERROR: Environment variable LINDOAPI_HOME is not set properly
	echo %LINDOAPI_HOME%/lib is not a valid library path
	goto END
)

if not exist %LINDOAPI_HOME%\include\lsversion.sh goto NOVERSION

for /f "delims=" %%x in (%LINDOAPI_HOME%\include\lsversion.sh) do (set "%%x")

echo Checking for LINDO API %LS_MAJOR%.%LS_MINOR% ...
if !R_ARCHITETURE! == "64bit" (
    if not exist %LINDOAPI_HOME%\bin\win64\. (
        echo ERROR: Can not find 64-bit LINDO API %LS_MAJOR%.%LS_MINOR%.
        goto END
    )
)

if !R_ARCHITETURE! == "32bit" (
    if not exist %LINDOAPI_HOME%\bin\win32\. (
        echo ERROR: Can not find 32-bit LINDO API %LS_MAJOR%.%LS_MINOR%.
        goto END
    )
)

for /F "tokens=1,2 delims=." %%a in (^"%R_VERSION:"=.%^") do set R_VER=%%a.%%b
rem echo %R_VER%
if "!R_VER!" LSS "%RMIN%" goto R_NOSUPPORT

echo Checking source package rLindo_%LS_MAJOR%.%LS_MINOR%.tar.gz
if exist rLindo_%LS_MAJOR%.%LS_MINOR%.tar.gz (
    echo Ok.. Installing...
) else (
    echo Error: source package rLindo_%LS_MAJOR%.%LS_MINOR%.tar.gz not found
    goto END
)

rem FOR /F "tokens=" %%i in (' "R --version 2> nul" ') do SET Routput=%%i
rem if "%Routput%"=="" goto NO_R

R CMD INSTALL rLindo_%LS_MAJOR%.%LS_MINOR%.tar.gz --no-multiarch
echo Finished
goto END

: R_NOSUPPORT
echo Error: Minimum R version required is %RMIN%, yours is %R_VER%.
goto END

:NOVERSION
echo Error: '%LINDOAPI_HOME%/include/lsversion.sh' file was not found
echo.
goto END


:END
