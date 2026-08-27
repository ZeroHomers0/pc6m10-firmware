@echo off
rem ============================================================
rem  flash.bat -- 傻瓜式 J-Link SWD 烧写最新固件
rem
rem  步骤: ①连接探测(校验驱动/接线) -> ②connect-under-reset(停核)
rem        -> ③自动备份当前板上 Flash -> ④整片擦除
rem        -> ⑤写入 firmware\firmware.bin -> ⑥全镜像校验
rem        -> ⑦复位并运行新固件
rem
rem  参数（可省）:
rem        -check    只做 J-Link 连接探测（校验驱动/接线），不烧写
rem        -preview  只生成 J-Link 脚本预览，不执行烧写
rem        -norun    烧写+校验后不复位运行（核心保持 halt）
rem
rem  前置: 市电/门极/功率负载已断开，仅控制电。
rem        板已接 J-Link，P12 排针: 6=SWDIO  8=SWCLK  7=nRESET
rem  依据: docs/w8/W8_POST_FLASH_2026-08-26.md（已验证序列）
rem ============================================================

setlocal
set "ROOT=%~dp0"
rem J-Link: 项目打包版 tools\jlink（免安装）优先；其次自动探测常见安装目录；最后 PATH
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
set "BIN=%ROOT%firmware\firmware.bin"
set "BAK=%ROOT%backup\auto_pre_flash.bin"
set "SCRIPT=%TEMP%\flash_firmware.jlink"
set "CHKSCRIPT=%TEMP%\jlink_check.jlink"
set "LOG=%ROOT%backup\flash_build.log"
set "CHKLOG=%ROOT%backup\jlink_check.log"
set "NORUN="

for %%a in (%*) do (
    if /i "%%a"=="-norun" set "NORUN=1"
)
if /i "%~1"=="-preview" goto preview
if /i "%~1"=="-check"  goto check

if not exist "%BIN%" (
    echo [错误] 找不到 %BIN%
    echo        请先运行 build.bat 构建固件。
    pause
    exit /b 1
)
if not exist "%JLINK%" (
    echo [错误] 找不到 J-Link: %JLINK%
    echo        请确认路径，或修改本脚本顶部 JLINK 变量。
    pause
    exit /b 1
)

rem ---- ① 连接探测（校验驱动/接线，失败即中止）----
call :check_link
if not defined JLINK_OK (
    echo [失败] 未检测到 J-Link 目标（找不到 SW-DP）。
    echo        日志: %CHKLOG%
    echo        可能原因:
    echo          1. J-Link USB 驱动未装 -- 请运行 install_deps.bat
    echo          2. J-Link 未插入 USB / 板未供电
    echo          3. SWD 线序错（P12: 6=SWDIO 8=SWCLK 7=nRESET）
    pause
    exit /b 1
)
echo [检查] J-Link 连接正常，芯片 SW-DP:
type "%CHKLOG%" | findstr /i "Found SW-DP"

call :gen_script

rem ---- ② 烧写前确认（整片擦除不可逆，防误烧旧版固件）----
echo.
echo [固件] 即将烧写:
for %%f in ("%BIN%") do (
    echo   文件名 : %%~nxf
    echo   构建时间: %%~tf
    echo   大小   : %%~zf 字节
)
echo [备份] 烧写前自动备份当前 Flash 到: %BAK%
echo [前置] 市电/门极/功率负载已断开？仅控制电？
echo.
choice /c YN /m "即将整片擦除并烧写，继续 Y / 取消 N"
if errorlevel 2 goto cancel

echo.
echo [烧写] 开始（约 10~30 秒，请勿断电/拔线）...
"%JLINK%" -device LPC1765 -if SWD -speed 4000 -CommanderScript "%SCRIPT%" > "%LOG%" 2>&1
set "RC=%ERRORLEVEL%"

set "VERIFIED="
findstr /i "Verify successful" "%LOG%" >nul 2>&1 && set "VERIFIED=1"

echo.
echo [结果] 关键日志：
type "%LOG%" | findstr /i "Erase Program Verify Reset Could not O.K. successful"

if "%RC%"=="0" if defined VERIFIED goto ok

echo [失败] 烧写未成功（退出码 %RC%）。
echo        日志: %LOG%
echo        常见原因: 未接 J-Link / 未供电 / SWD 线序错（P12: 6=SWDIO 8=SWCLK 7=nRESET）/ 接线松
pause
exit /b 1

:ok
echo [成功] 烧写完成，固件已复位运行。
echo        本次烧写前 Flash 备份: %BAK%
pause
exit /b 0

:cancel
echo [取消] 未烧写，板上固件保持不变。
pause
exit /b 0

:check
if not exist "%JLINK%" (
    echo [错误] 找不到 J-Link: %JLINK%
    pause
    exit /b 1
)
echo [检查] 探测 J-Link 连接（校验驱动/接线，不烧写）...
call :check_link
if defined JLINK_OK (
    echo [OK] J-Link 已识别，驱动与接线正常。芯片 SW-DP:
    type "%CHKLOG%" | findstr /i "Found SW-DP"
) else (
    echo [失败] 未检测到 J-Link。
    echo        日志: %CHKLOG%
    echo        可能原因: 驱动未装（install_deps.bat）/ 未插 / 未供电 / 线序错
)
pause
exit /b 0

:preview
echo [预览] 生成 J-Link 脚本（不执行烧写）
call :gen_script
echo.
type "%SCRIPT%"
echo.
echo [预览] 脚本已生成: %SCRIPT%
pause
exit /b 0

rem ---- 生成 J-Link Commander 烧写脚本（验证过的序列）----
rem  参考 backup/jlink_flash_disp_sel_fix.jlink（双连接）
rem       backup/jlink_backup_pre_adc_fix.jlink（w4 MEMMAP=1 + savebin）
rem       backup/jlink_run_fixed.jlink（复位运行）
:gen_script
>  "%SCRIPT%" echo device LPC1765
>> "%SCRIPT%" echo if SWD
>> "%SCRIPT%" echo speed 4000
>> "%SCRIPT%" echo SetRESET
>> "%SCRIPT%" echo sleep 200
>> "%SCRIPT%" echo connect
>> "%SCRIPT%" echo ClrRESET
>> "%SCRIPT%" echo sleep 200
>> "%SCRIPT%" echo connect
>> "%SCRIPT%" echo w4 0x400FC040, 1
>> "%SCRIPT%" echo savebin %BAK%, 0x0, 0x40000
>> "%SCRIPT%" echo erase
>> "%SCRIPT%" echo loadbin %BIN%, 0x0
>> "%SCRIPT%" echo verifybin %BIN%, 0x0
if defined NORUN (
    >> "%SCRIPT%" echo exit
) else (
    >> "%SCRIPT%" echo SetRESET
    >> "%SCRIPT%" echo sleep 200
    >> "%SCRIPT%" echo ClrRESET
    >> "%SCRIPT%" echo sleep 200
    >> "%SCRIPT%" echo go
    >> "%SCRIPT%" echo sleep 500
    >> "%SCRIPT%" echo exit
)
exit /b 0

rem ---- 连接探测：JLink.exe 连一下，找 SW-DP ----
:check_link
set "JLINK_OK="
>  "%CHKSCRIPT%" echo device LPC1765
>> "%CHKSCRIPT%" echo if SWD
>> "%CHKSCRIPT%" echo speed 4000
>> "%CHKSCRIPT%" echo SetRESET
>> "%CHKSCRIPT%" echo sleep 200
>> "%CHKSCRIPT%" echo connect
>> "%CHKSCRIPT%" echo ClrRESET
>> "%CHKSCRIPT%" echo sleep 200
>> "%CHKSCRIPT%" echo exit
"%JLINK%" -device LPC1765 -if SWD -speed 4000 -CommanderScript "%CHKSCRIPT%" > "%CHKLOG%" 2>&1
findstr /i "Found SW-DP" "%CHKLOG%" >nul 2>&1 && set "JLINK_OK=1"
exit /b 0
