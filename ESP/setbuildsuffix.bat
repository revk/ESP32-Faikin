@echo off

setlocal

IF NOT DEFINED POSIX_ENV_PATH (
  REM some commonly used defaults
  IF EXIST "c:\msys64\usr\bin" (
    set POSIX_ENV_PATH=c:\msys64\usr\bin
  ) ELSE IF EXIST "c:\cygwin64\usr\bin" (
    set POSIX_ENV_PATH=c:\cygwin64\usr\bin
  ) ELSE IF EXIST "c:\cygwin\usr\bin" (
    set POSIX_ENV_PATH=c:\cygwin\usr\bin
  ) ELSE (
    echo Cygwin or MSYS not found! Install or provide POSIX_ENV_PATH environment variable
    goto end
  )
)

set "PATH=%PATH%;%POSIX_ENV_PATH%"

csh components/ESP32-RevK/setbuildsuffix %1

:end
