# =============================================================================
# setup_windows.ps1 — 目标B 另一台电脑一键环境搭建（Windows 10/11）
# =============================================================================
# 做什么（按顺序）：
#   1. 校验/安装 Git for Windows（提供 build.sh 所需的 bash）
#   2. 校验/安装 ARM GNU Embedded Toolchain（arm-none-eabi-gcc，编译固件）
#   3. 校验/安装 Python 3.13（跑 verify_*.py 验证脚本 + W8 Modbus 测试）
#   4. pip 安装 minimalmodbus + pyserial（W8 阶段 B 用）
#   5. 定位 arm-none-eabi-gcc 实际路径，改写 firmware/build.sh 的 TC=（关键！）
#   6. 校验 build.sh 能否编译
#
# 用法（二选一）：
#   powershell -ExecutionPolicy Bypass -File tools\setup_windows.ps1
#   或双击同目录 setup_windows.bat
#
# 前置：能联网（winget 从 Microsoft 源下载）、有管理员权限（会自动提权）。
# =============================================================================

# ── 1. 自动提权（Install 需管理员）────────────────────────────────────
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "请求管理员权限以安装软件…" -ForegroundColor Yellow
    Start-Process powershell -Verb RunAs -ArgumentList "-ExecutionPolicy Bypass -File `"$PSCommandPath`"" -WindowStyle Hidden
    exit
}

$ErrorActionPreference = "Stop"
$ROOT  = Split-Path -Parent $PSScriptRoot   # 项目根 D:\...\decompiled
$FIRM  = Join-Path $ROOT "firmware"

Write-Host "=== 目标B 环境搭建开始 ===" -ForegroundColor Cyan
Write-Host "项目根 : $ROOT"

# ── 2. 确认 winget 可用 ─────────────────────────────────────────────
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Host "[错误] 找不到 winget。请先安装'应用安装程序'（可从 Microsoft Store 装，或升级 Win10/11 系统自带）。" -ForegroundColor Red
    exit 1
}

# ── 3. 安装 / 校验 Git for Windows（bash 来源）─────────────────────
Write-Host "`n== [1/5] Git for Windows（提供 bash）==" -ForegroundColor Green
winget install --id Git.Git -e --accept-package-agreements --accept-source-agreements --silent
Write-Host "  => 完成。bash 将位于 ...\Git\bin\bash.exe"

# ── 4. 安装 / 校验 ARM GCC 工具链 ─────────────────────────────────
Write-Host "`n== [2/5] ARM GNU Embedded Toolchain (arm-none-eabi-gcc) ==" -ForegroundColor Green
winget install --id Arm.GnuArmEmbeddedToolchain -e --accept-package-agreements --accept-source-agreements --silent
Write-Host "  => 完成。"

# ── 5. 安装 / 校验 Python 3.13 ────────────────────────────────────
Write-Host "`n== [3/5] Python 3.13 ==" -ForegroundColor Green
winget install --id Python.Python.3.13 -e --accept-package-agreements --accept-source-agreements --silent
Write-Host "  => 完成。"

# ── 6. 定位 arm-none-eabi-gcc，改写 build.sh 的 TC 路径（关键）──────
Write-Host "`n== [4/5] 定位工具链并改写 build.sh 路径 ==" -ForegroundColor Green

# 找 arm-none-eabi-gcc.exe（优先 PATH，再在两个常用安装目录里找，取最新版本）
$gccExe = $null
$cands = @(
    (Get-Command arm-none-eabi-gcc.exe -ErrorAction SilentlyContinue).Source,
    (Get-ChildItem "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\*\bin\arm-none-eabi-gcc.exe" -ErrorAction SilentlyContinue),
    (Get-ChildItem "C:\Program Files\Arm GNU Toolchain arm-none-eabi\*\bin\arm-none-eabi-gcc.exe" -ErrorAction SilentlyContinue)
) | Where-Object { $_ } | Select-Object -First 1
if ($cands) { $gccExe = $cands.Path }

if (-not $gccExe) {
    Write-Host "[警告] 未自动定位到 arm-none-eabi-gcc.exe。请在 Git Bash 手动确认后更新 build.sh。" -ForegroundColor Yellow
} else {
    $binDir  = Split-Path -Parent $gccExe
    # 把 Windows 绝对路径转成 Git Bash 风格 /c/Program Files/...
    $drv = $binDir.Substring(0,1).ToLower()
    $rest = $binDir.Substring(2) -replace '\\','/'
    $bashBin = "/$drv$rest"          # 例: /c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin

    $buildSh = Join-Path $FIRM "build.sh"
    if (Test-Path $buildSh) {
        $content = Get-Content $buildSh -Raw
        # 替换 TC="/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin"
        $content = $content -replace '(?m)^TC=".*"', "TC=`"$bashBin`""
        Set-Content $buildSh $content -NoNewline -Encoding UTF8
        Write-Host "  已把 build.sh 的 TC 改写为: $bashBin"
        Write-Host "  （版本号路径不同也能自动对齐，避免编译失败）"
    } else {
        Write-Host "  [警告] 找不到 $buildSh，跳过改写。" -ForegroundColor Yellow
    }
}

# ── 7. 安装 Python 包：minimalmodbus + pyserial（W8 用）────────────
Write-Host "`n== [5/5] pip 安装 minimalmodbus + pyserial ==" -ForegroundColor Green
# Python 可能刚装完 PATH 未刷新，逐个候选路径直接调用
$pyCands = @(
    "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe",
    "$env:ProgramFiles\Python313\python.exe",
    (Get-Command python.exe -ErrorAction SilentlyContinue).Source,
    (Get-Command py.exe -ErrorAction SilentlyContinue).Source
) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique

$pipOk = $false
if ($pyCands) {
    foreach ($py in $pyCands) {
        Write-Host "  尝试 $py ..."
        & $py -m pip install --upgrade minimalmodbus pyserial 2>$null
        if ($LASTEXITCODE -eq 0) { $pipOk = $true; $pyPath = $py; break }
    }
}
if ($pipOk) {
    Write-Host "  已安装 minimalmodbus + pyserial（via $pyPath）"
} else {
    Write-Host "  [警告] pip 安装失败。请重开终端后手动执行:" -ForegroundColor Yellow
    Write-Host '    python -m pip install minimalmodbus pyserial'
}

# ── 8. 验证 build.sh ──────────────────────────────────────────────
Write-Host "`n== 尝试编译验证 ==" -ForegroundColor Green
$gitBash = "$env:ProgramFiles\Git\bin\bash.exe"
if (-not (Test-Path $gitBash)) {
    $gitBash = (Get-Command bash.exe -ErrorAction SilentlyContinue).Source
}
if ($gitBash -and (Test-Path $FIRM)) {
    Push-Location $FIRM
    & $gitBash -c "bash build.sh" | ForEach-Object { Write-Host "  $_" }
    Pop-Location
} else {
    Write-Host "  未找到 Git Bash 或 firmware 目录，跳过自动编译验证。" -ForegroundColor Yellow
    Write-Host "  请手动：进入 firmware/ 目录，运行  bash build.sh"
}

Write-Host "`n=== 全部完成 ===" -ForegroundColor Cyan
Write-Host "请【重开一个终端】(或 Git Bash) 让新装的命令进入 PATH，再执行："
Write-Host "  cd D:\\code\\LPC1765FBD100\\decompiled\\firmware"
Write-Host "  bash build.sh"
Write-Host "预期最后一行：OK: firmware.elf / firmware.hex / firmware.bin"
Write-Host "`n更多说明见项目根 SETUP_WINDOWS.md"
