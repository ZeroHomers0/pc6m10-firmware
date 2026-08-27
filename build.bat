@echo off
rem ============================================================
rem  build.bat -- 傻瓜式构建最新固件（等价 bash firmware/build.sh）
rem
rem  产出: firmware\firmware.elf / .hex / .bin / .map
rem  要求: 已安装 Git for Windows（提供 bash.exe，Git Bash）
rem
rem  用法: 双击运行，或命令行 build.bat
rem ============================================================

setlocal
set "ROOT=%~dp0"

rem ---- 定位 Git Bash（build.sh 依赖 bash 的 /c/ 路径写法）----
set "BASH="
if exist "%ProgramFiles%\Git\bin\bash.exe" set "BASH=%ProgramFiles%\Git\bin\bash.exe"
if exist "%ProgramFiles(x86)%\Git\bin\bash.exe" set "BASH=%ProgramFiles(x86)%\Git\bin\bash.exe"
if exist "%LocalAppData%\Programs\Git\bin\bash.exe" set "BASH=%LocalAppData%\Programs\Git\bin\bash.exe"
if not defined BASH (
    for /f "delims=" %%i in ('where bash 2^>nul') do if not defined BASH set "BASH=%%i"
)
if not defined BASH (
    echo [错误] 找不到 Git Bash（bash.exe）。
    echo        请安装 Git for Windows：https://git-scm.com/download/win
    pause
    exit /b 1
)

echo [构建] bash      : %BASH%
echo [构建] 工具链    : Arm GNU Toolchain 14.2.Rel1（经 firmware\build.sh 调用）
echo [构建] 开始构建...
echo.

pushd "%ROOT%firmware"
"%BASH%" build.sh
set "BRC=%ERRORLEVEL%"
popd

if not "%BRC%"=="0" (
    echo.
    echo [失败] 构建出错（退出码 %BRC%），见上方日志。
    pause
    exit /b 1
)

echo.
echo [完成] 构建产物：firmware\firmware.elf / .hex / .bin / .map
echo [校验] firmware.bin SHA-256：
certutil -hashfile "%ROOT%firmware\firmware.bin" SHA256 | findstr /i "^[0-9a-f]"
echo.
echo [提示] 全量测试: python test\run_tests.py
echo        烧写固件: flash.bat
pause
