@echo off
rem ============================================================
rem  install_deps.bat -- 一键安装构建/烧写依赖
rem
rem  1. Git for Windows（build.bat 依赖 bash）   -- 若缺则提示手动装
rem  2. Arm GNU Toolchain 14.2.Rel1（gcc）       -- 若缺则下载官方 exe 静默安装
rem  3. J-Link USB 驱动（tools\jlink 打包驱动）   -- 若 J-Link 无法识别则安装
rem
rem  用法: 双击运行（自动请求管理员权限）
rem        -skip-toolchain   跳过工具链
rem        -skip-driver      跳过 J-Link 驱动
rem ============================================================

setlocal enabledelayedexpansion
set "ROOT=%~dp0"
set "SKIP_TC="
set "SKIP_DRV="
for %%a in (%*) do (
    if /i "%%a"=="-skip-toolchain" set "SKIP_TC=1"
    if /i "%%a"=="-skip-driver"    set "SKIP_DRV=1"
)

rem ---- 提权（装 Program Files / USB 驱动需管理员）----
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [提权] 需要管理员权限，正在重新以管理员身份运行...
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo ============================================================
echo   依赖安装脚本
echo ============================================================

rem ---- 1/3 Git for Windows ----
echo.
echo [1/3] Git for Windows（build.bat 需要 bash）...
set "BASH="
if exist "%ProgramFiles%\Git\bin\bash.exe" set "BASH=%ProgramFiles%\Git\bin\bash.exe"
if exist "%ProgramFiles(x86)%\Git\bin\bash.exe" set "BASH=%ProgramFiles(x86)%\Git\bin\bash.exe"
if exist "%LocalAppData%\Programs\Git\bin\bash.exe" set "BASH=%LocalAppData%\Programs\Git\bin\bash.exe"
if not defined BASH (
    for /f "delims=" %%i in ('where bash 2^>nul') do if not defined BASH set "BASH=%%i"
)
if defined BASH (
    echo       [OK] Git Bash 已安装: !BASH!
) else (
    echo       [提示] 未检测到 Git Bash。
    echo              请下载安装 Git for Windows：
    echo              https://git-scm.com/download/win
    echo              （安装完成后本脚本其余部分仍可继续）
)

rem ---- 2/3 Arm GNU Toolchain ----
if defined SKIP_TC goto driver
echo.
echo [2/3] Arm GNU Toolchain 14.2.Rel1（arm-none-eabi-gcc）...
set "GCCBIN=%ProgramFiles(x86)%\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-gcc.exe"
if exist "!GCCBIN!" (
    echo       [OK] 工具链已安装: !GCCBIN!
) else (
    echo       [安装] 未检测到工具链，下载官方安装包（约 250MB，静默安装）...
    set "URL=https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-x86_64-arm-none-eabi.exe"
    set "SETUP=%TEMP%\arm-gnu-toolchain-14.2.rel1.exe"
    curl -L -o "%SETUP%" "%URL%"
    if not exist "%SETUP%" (
        echo       [错误] 下载失败。
        echo              请手动从 https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
        echo              下载 64 位 Windows 安装包并安装（装到默认路径）。
        goto driver
    )
    echo       [安装] 静默安装中...
    "%SETUP%" /S /P /R
    if exist "!GCCBIN!" (
        echo       [OK] 工具链安装成功。
    ) else (
        echo       [警告] 安装完成但未检测到 gcc。
        echo              请确认安装到默认路径，或手动运行安装器。
        echo              期望: !GCCBIN!
    )
    del "%SETUP%" 2>nul
)

:driver
rem ---- 3/3 J-Link USB 驱动 ----
if defined SKIP_DRV goto summary
echo.
echo [3/3] J-Link USB 驱动（tools\jlink 打包驱动）...
set "JLINKEXE=%ROOT%tools\jlink\JLink.exe"
if not exist "%JLINKEXE%" set "JLINKEXE=D:\software\SEGGER\JLink_V970\JLink.exe"
set "CHK=%TEMP%\jlink_check.jlink"
set "CHKLOG=%ROOT%backup\jlink_check.log"
>  "%CHK%" echo device LPC1765
>> "%CHK%" echo if SWD
>> "%CHK%" echo speed 4000
>> "%CHK%" echo SetRESET
>> "%CHK%" echo sleep 200
>> "%CHK%" echo connect
>> "%CHK%" echo ClrRESET
>> "%CHK%" echo sleep 200
>> "%CHK%" echo exit
"%JLINKEXE%" -device LPC1765 -if SWD -speed 4000 -CommanderScript "%CHK%" > "%CHKLOG%" 2>&1
findstr /i "Found SW-DP" "%CHKLOG%" >nul 2>&1
if not errorlevel 1 (
    echo       [OK] J-Link 已识别（驱动与接线正常）。
) else (
    echo       [安装] 未识别到 J-Link，尝试安装 USB 驱动...
    pushd "%ROOT%tools\jlink\USBDriver\x64"
    dpinst_x64.exe /S /SA
    popd
    echo       [提示] 驱动已尝试安装。请确认：
    echo              - J-Link 已插入 USB
    echo              - 板已接 J-Link（P12: 6=SWDIO 8=SWCLK 7=nRESET）并供电
    echo              - 仍失败请换 USB 线/端口，或重插 J-Link
    echo              可重跑本脚本验证，或用 flash.bat -check 复测
)

:summary
echo.
echo ============================================================
echo   完成。后续:
echo    build.bat   构建固件
echo    flash.bat   烧写固件（flash.bat -check 可先测连接）
echo ============================================================
pause
