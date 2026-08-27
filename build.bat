@echo off
rem ============================================================
rem  build.bat -- 傻瓜式构建最新固件（调用 firmware\build.sh）
rem
rem  产物: firmware\firmware.elf / .hex / .bin / .map
rem  日志: backup\build.log（构建完整输出落盘，失败可回溯）
rem  要求: 已安装 Git for Windows（提供 bash.exe，Git Bash）
rem
rem  用法: 双击运行（无需参数）
rem ============================================================

setlocal enabledelayedexpansion
set "ROOT=%~dp0"
set "BUILDLOG=%ROOT%backup\build.log"

rem ---- 定位 Git Bash（build.sh 按 /c/ 路径写法）----
set "BASH="
if exist "%ProgramFiles%\Git\bin\bash.exe" set "BASH=%ProgramFiles%\Git\bin\bash.exe"
if exist "%ProgramFiles(x86)%\Git\bin\bash.exe" set "BASH=%ProgramFiles(x86)%\Git\bin\bash.exe"
if exist "%LocalAppData%\Programs\Git\bin\bash.exe" set "BASH=%LocalAppData%\Programs\Git\bin\bash.exe"
if not defined BASH (
    for /f "tokens=2,*" %%j in ('reg query "HKLM\SOFTWARE\GitForWindows" /v InstallPath 2^>nul ^| findstr /i "InstallPath"') do if exist "%%k\bin\bash.exe" set "BASH=%%k\bin\bash.exe"
)
if not defined BASH (
    for /f "tokens=2,*" %%j in ('reg query "HKCU\Software\GitForWindows" /v InstallPath 2^>nul ^| findstr /i "InstallPath"') do if exist "%%k\bin\bash.exe" set "BASH=%%k\bin\bash.exe"
)
if not defined BASH (
    for /f "delims=" %%i in ('where git 2^>nul') do if not defined BASH (
        set "GITROOT=%%i"
        set "GITROOT=!GITROOT:\cmd\git.exe=!"
        set "GITROOT=!GITROOT:\bin\git.exe=!"
        if exist "!GITROOT!\bin\bash.exe" set "BASH=!GITROOT!\bin\bash.exe"
    )
)
if not defined BASH (
    for /f "delims=" %%i in ('where bash 2^>nul') do if not defined BASH set "BASH=%%i"
)
if not defined BASH (
    echo [错误] 找不到 Git Bash（bash.exe）。
    echo        请安装 Git for Windows：https://git-scm.com/download/win
    pause
    exit /b 1
)

rem ---- 校验 Arm GNU Toolchain（自动探测，跨机器无需改路径）----
set "GCCBIN="
for /d %%d in ("%ProgramFiles(x86)%\Arm GNU Toolchain arm-none-eabi\*") do (
    if exist "%%d\bin\arm-none-eabi-gcc.exe" set "GCCBIN=%%d\bin\arm-none-eabi-gcc.exe"
)
if not defined GCCBIN (
    for /d %%d in ("%ProgramFiles%\Arm GNU Toolchain arm-none-eabi\*") do (
        if exist "%%d\bin\arm-none-eabi-gcc.exe" set "GCCBIN=%%d\bin\arm-none-eabi-gcc.exe"
    )
)
if not defined GCCBIN (
    for /f "delims=" %%i in ('where arm-none-eabi-gcc 2^>nul') do if not defined GCCBIN set "GCCBIN=%%i"
)
if not defined GCCBIN (
    echo [错误] 找不到 Arm GNU Toolchain（arm-none-eabi-gcc）。
    echo        请先运行 install_deps.bat 自动安装（装到默认路径）。
    pause
    exit /b 1
)
echo [信息] bash      : %BASH%
echo [信息] gcc       : %GCCBIN%
echo [信息] 开始构建（约 30~60 秒，请稍候）...
echo.

rem ---- 构建（输出落盘，完成后再显示）----
for /f %%s in ('powershell -NoProfile -Command "[int](Get-Date -UFormat %%s)"') do set "D0=%%s"
pushd "%ROOT%firmware"
"%BASH%" build.sh > "%BUILDLOG%" 2>&1
set "BRC=%ERRORLEVEL%"
popd

if not "%BRC%"=="0" (
    echo [失败] 构建退出码 %BRC%。
    echo        完整日志: %BUILDLOG%
    echo.
    type "%BUILDLOG%"
    pause
    exit /b 1
)

rem ---- 成功：显示构建日志、耗时、SHA ----
type "%BUILDLOG%"

for /f %%s in ('powershell -NoProfile -Command "[int](Get-Date -UFormat %%s)"') do set "D1=%%s"
set /a "DIFF=D1-D0"

echo.
echo [完成] 构建产物：firmware\firmware.elf / .hex / .bin / .map
echo [耗时] %DIFF% 秒
echo [校验] firmware.bin SHA-256：
certutil -hashfile "%ROOT%firmware\firmware.bin" SHA256 | findstr /i "^[0-9a-f]"
echo.
echo [提示] 全量测试: python test\run_tests.py
echo        烧写固件: flash.bat
pause
