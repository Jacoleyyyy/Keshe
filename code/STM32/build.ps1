# 智能搬运机器人 — 一键编译脚本 (PowerShell)
# 用法:
#   powershell -File build.ps1           → 仅编译
#   powershell -File build.ps1 configure  → CMake配置+编译
#   powershell -File build.ps1 flash      → 烧录

$gcc    = "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\12.2 mpacbti-rel1\bin"
$cmake  = "C:\Users\16011\tools\cmake-4.0.0-windows-x86_64\bin"
$ninja  = "C:\Users\16011\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe"

$env:Path = "$gcc;$cmake;$ninja;$env:Path"
Set-Location "f:\KeShe\code\STM32"

if ($args[0] -eq "configure") {
    Write-Host "=== CMake Configure ===" -ForegroundColor Cyan
    Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
    & "$cmake\cmake.exe" -B build -G Ninja `
      -DCMAKE_TOOLCHAIN_FILE=toolchain-arm-none-eabi.cmake `
      -DCMAKE_BUILD_TYPE=Debug `
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    Copy-Item -Force build\compile_commands.json . -ErrorAction SilentlyContinue
    Write-Host "Configure done, starting build..." -ForegroundColor Green
}

Write-Host "=== Build ===" -ForegroundColor Cyan
& "$cmake\cmake.exe" --build build -j8
if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "==========================================" -ForegroundColor Green
    Write-Host "  BUILD SUCCESS" -ForegroundColor Green
    Write-Host "  build\smart_carrier.elf" -ForegroundColor Green
    Write-Host "  build\smart_carrier.hex" -ForegroundColor Green
    Write-Host "==========================================" -ForegroundColor Green
    & "$gcc\arm-none-eabi-size.exe" build\smart_carrier.elf
} else {
    Write-Host "BUILD FAILED - check errors above" -ForegroundColor Red
}

if ($args[0] -eq "flash") {
    Write-Host "=== Flash ===" -ForegroundColor Cyan
    pyocd load build\smart_carrier.hex --target stm32f407ve --frequency 4000000
}
