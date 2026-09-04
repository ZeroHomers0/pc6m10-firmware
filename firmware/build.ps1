<#
  firmware/build.ps1 — Windows 原生构建入口

  与 build.sh 使用同一工具链、编译选项和链接布局；适用于没有 Git Bash
  或 make 的开发机。构建产物仍为 firmware.elf/.hex/.bin/.map。
#>
$ErrorActionPreference = 'Stop'
$firmwareRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $firmwareRoot

$toolchainCandidates = @(
    'C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin',
    'C:\Program Files\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin'
)
$toolchain = $toolchainCandidates | Where-Object { Test-Path (Join-Path $_ 'arm-none-eabi-gcc.exe') } | Select-Object -First 1
if (-not $toolchain) {
    $gcc = Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue
    if ($gcc) { $toolchain = Split-Path -Parent $gcc.Source }
}
if (-not $toolchain) {
    throw "找不到 arm-none-eabi-gcc，请安装 Arm GNU Toolchain 14.2.Rel1。"
}

$gcc = Join-Path $toolchain 'arm-none-eabi-gcc.exe'
$objcopy = Join-Path $toolchain 'arm-none-eabi-objcopy.exe'
$size = Join-Path $toolchain 'arm-none-eabi-size.exe'
$commonFlags = ([IO.File]::ReadAllText((Join-Path $firmwareRoot 'compiler_flags.txt')) -split '\s+' |
    Where-Object { $_ })
$cpu = @($commonFlags | Where-Object {
    $_ -like '-mcpu=*' -or $_ -eq '-mthumb' -or $_ -like '-mfloat-abi=*'
})
$cflags = $commonFlags

$generated = @('startup.o', 'data_image.o', 'firmware.elf',
    'firmware.hex', 'firmware.bin', 'firmware.map', 'firmware.lst')
$generated += Get-ChildItem -LiteralPath src -Filter '*.o' -File -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }
foreach ($path in $generated) {
    Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
}

Write-Host '== 编译 ==' -ForegroundColor Cyan
function Invoke-Tool {
    param(
        [string]$Label,
        [string]$Executable,
        [object[]]$Arguments
    )
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Label 失败（退出码 $LASTEXITCODE）"
    }
}

Invoke-Tool '编译 startup.s' $gcc ($cflags + @('-c', '-o', 'startup.o', 'startup.s'))
Invoke-Tool '编译 data_image.s' $gcc ($cflags + @('-c', '-o', 'data_image.o', 'data_image.s'))
foreach ($source in (Get-ChildItem -LiteralPath src -Filter '*.c' -File | Sort-Object Name)) {
    Write-Host $source.Name
    Invoke-Tool "编译 $($source.Name)" $gcc ($cflags + @('-c', '-o', (Join-Path src ($source.BaseName + '.o')), $source.FullName))
}

Write-Host '== 链接 ==' -ForegroundColor Cyan
$objects = @('startup.o', 'data_image.o')
$objects += Get-ChildItem -LiteralPath src -Filter '*.o' -File | Sort-Object Name | ForEach-Object { $_.FullName }
Invoke-Tool '链接 firmware.elf' $gcc ($cpu + @('-T', 'lpc1765.ld', '-nostdlib', '-o', 'firmware.elf') + $objects + @('-lgcc', '-Wl,--gc-sections', '-Wl,-Map,firmware.map'))
Invoke-Tool '生成 firmware.hex' $objcopy @('-O', 'ihex', 'firmware.elf', 'firmware.hex')
Invoke-Tool '生成 firmware.bin' $objcopy @('--gap-fill=0xFF', '-O', 'binary',
    '--only-section=.isr_vector', '--only-section=.crp', '--only-section=.text', '--only-section=.fw_image',
    'firmware.elf', 'firmware.bin')
Invoke-Tool '读取固件大小' $size @('firmware.elf')
Write-Host 'OK: firmware.elf / firmware.hex / firmware.bin / firmware.map' -ForegroundColor Green
