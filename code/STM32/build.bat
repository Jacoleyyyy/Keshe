@echo off
REM ================================================================
REM  智能搬运机器人 — 一键编译脚本 (Windows CMD)
REM  首次运行: build.bat configure
REM  日常编译: build.bat
REM  烧录:     build.bat flash
REM ================================================================
setlocal enabledelayedexpansion

REM 工具路径
set "GCC_PATH=C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\12.2 mpacbti-rel1\bin"
set "CMAKE_PATH=C:\Users\16011\tools\cmake-4.0.0-windows-x86_64\bin"
set "NINJA_PATH=C:\Users\16011\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe"

REM 加到 PATH
set "PATH=%GCC_PATH%;%CMAKE_PATH%;%NINJA_PATH%;%PATH%"

cd /d "f:\KeShe\code\STM32"

if "%1"=="configure" goto configure
if "%1"=="flash" goto flash
goto build

:configure
  echo === CMake 配置 ===
  rmdir /s /q build 2>nul
  cmake -B build -G Ninja ^
    -DCMAKE_TOOLCHAIN_FILE=toolchain-arm-none-eabi.cmake ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  if %ERRORLEVEL% EQU 0 (
      copy /y build\compile_commands.json . >nul
      echo === 配置完成, 开始编译 ===
  ) else (
      echo === 配置失败 ===
      exit /b 1
  )
  goto build

:build
  echo === 编译 ===
  cmake --build build -j8
  if %ERRORLEVEL% EQU 0 (
      echo.
      echo ==========================================
      echo   编译成功! build/smart_carrier.elf
      echo ==========================================
  ) else (
      echo 编译失败, 请检查上方错误信息
      exit /b 1
  )
  arm-none-eabi-size build\smart_carrier.elf 2>nul
  goto end

:flash
  echo === 烧录 ===
  pyocd load build\smart_carrier.hex --target stm32f407ve --frequency 4000000
  if %ERRORLEVEL% EQU 0 (
      echo 烧录成功!
  )
  goto end

:end
endlocal
