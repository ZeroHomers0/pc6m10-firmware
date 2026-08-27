@echo off
setlocal
rem ============================================================
rem  reset.bat -- 双击即复位重启固件（无需重新上电）
rem
rem  通过 J-Link connect-under-reset：SetRESET 拉低 P12-7 nRESET
rem  停核并连接 -> ClrRESET 放行 -> go 运行，固件从 Reset_Handler
rem  重新启动，等效重新上电。
rem
rem  用于：烧写后想重新跑固件 / 固件卡死想重启 / 面板改了参数想重载。
rem  仅复位，不写 Flash / EEPROM，安全。
rem
rem  前置：控制电已上；J-Link 已连 P12（6=SWDIO  8=SWCLK  7=nRESET）。
rem  参考: docs/w8/W8_POST_FLASH_2026-08-26.md（连接模式同 flash.bat）
rem ============================================================

set "ROOT=%~dp0"
rem J-Link: 优先仓库打包版 tools\jlink，其次 SEGGER 安装目录，再 PATH
set "JLINK=%ROOT%tools\jlink\JLink.exe"
if not exist "%JLINK%" (
    set "JLINK="
    for /d %%d in ("%ProgramFiles%\SEGGER\JLink_V*" "%ProgramFiles(x86)%\SEGGER\JLink_V*") do (
        if exist "%%d\JLink.exe" set "JLINK=%%d\JLink.exe"
    )
    if not defined JLINK (
        for /f "delims=" %%i in ('where JLink.exe 2^>nul') do if not defined JLINK set "JLINK=%%i"
    )
)
if not defined JLINK (
    echo [错误] 找不到 J-Link（tools\jlink\JLink.exe 或 SEGGER 安装版）。
    echo        请先运行 install_deps.bat 安装驱动。
    pause
    exit /b 1
)

set "SCRIPT=%TEMP%\jlink_reset.jlink"
set "LOG=%TEMP%\jlink_reset.log"

rem ---- 生成复位脚本（connect-under-reset：先拉复位停核再连接）----
>  "%SCRIPT%" echo device LPC1765
>> "%SCRIPT%" echo if SWD
>> "%SCRIPT%" echo speed 4000
>> "%SCRIPT%" echo SetRESET
>> "%SCRIPT%" echo sleep 200
>> "%SCRIPT%" echo connect
>> "%SCRIPT%" echo ClrRESET
>> "%SCRIPT%" echo sleep 200
>> "%SCRIPT%" echo go
>> "%SCRIPT%" echo sleep 500
>> "%SCRIPT%" echo exit

echo [复位] 通过 P12-7 nRESET 复位并重启固件（无需重新上电）...
"%JLINK%" -device LPC1765 -if SWD -speed 4000 -CommanderScript "%SCRIPT%" > "%LOG%" 2>&1
set "RC=%ERRORLEVEL%"

rem ---- 关键日志 ----
type "%LOG%" | findstr /i "Found SW-DP Reset Could not failed"

findstr /i "Found SW-DP" "%LOG%" >nul 2>&1
if errorlevel 1 (
    echo.
    echo [失败] 未检测到目标（找不到 SW-DP），固件可能未复位。
    echo        日志: %LOG%
    echo        检查: 1) 控制电已上  2) J-Link 连 P12 6=SWDIO 8=SWCLK 7=nRESET
    echo              3) 接线松动  4) 上次烧写后固件处于 halt
    pause
    exit /b 1
)

echo.
echo [成功] 固件已复位并从 Reset_Handler 重新启动（等效重新上电）。
pause
exit /b 0
