$ErrorActionPreference = 'Stop'
if (!(Get-Command cl.exe -ErrorAction SilentlyContinue) -or !(Get-Command rc.exe -ErrorAction SilentlyContinue)) {
    throw 'Run from Developer PowerShell for VS with C++ tools and Windows SDK installed.'
}
$sourceDir = Join-Path $PSScriptRoot '项目文件'
$outputDir = Join-Path $PSScriptRoot '输出结果'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
Push-Location $sourceDir
try {
    & rc.exe /nologo /fo "$outputDir\WindowPin.res" WindowPin.rc
    if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }
    & cl.exe /nologo /O2 /MT /W4 /utf-8 /EHsc WindowPin.cpp "/Fo:$outputDir\WindowPin.obj" "/Fe:$outputDir\WindowPin.exe" /link /SUBSYSTEM:WINDOWS "$outputDir\WindowPin.res" user32.lib shell32.lib advapi32.lib ole32.lib uuid.lib dwmapi.lib gdi32.lib d2d1.lib
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed. Close Window Pin before rebuilding.' }
} finally { Pop-Location }

