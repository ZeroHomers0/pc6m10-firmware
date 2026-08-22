@echo off
REM =============================================================
REM setup_windows.bat — 双击即可运行 setup_windows.ps1 的一键安装入口
REM 用途：在另一台 Windows 电脑上安装编译固件 + W8 所需的全部环境
REM =============================================================
REM 切换到本脚本所在目录（tools/），再从项目根找 ps1
cd /d "%~dp0"
echo 安装目标B环境（Git + ARM GCC + Python + minimalmodbus/pyserial）...
echo 即将调用 PowerShell（可能需要管理员/UAC 弹窗，请点"是"）
pause
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup_windows.ps1"
echo.
echo 脚本执行完毕（返回码 %errorlevel%）。若上面提示"请重开终端"，请照做。
pause
