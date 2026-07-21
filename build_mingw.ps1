# Time Master build script (Qt6 + MinGW, no vcpkg)
$ErrorActionPreference = "Stop"

$QtDir = "D:\AICOP\requirements\QT6"
$QtVer = "6.11.1\mingw_64"
$MingwDir = "$QtDir\Tools\mingw1310_64"
$NinjaDir = "$QtDir\Tools\Ninja"
$CmakeDir = "$QtDir\Tools\CMake_64"

$env:PATH = "$MingwDir\bin;$NinjaDir;$QtDir\$QtVer\bin;$CmakeDir\bin;$env:PATH"

Write-Host "=== Configuring ===" -ForegroundColor Cyan
Remove-Item -LiteralPath build -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path build -Force | Out-Null

& "$QtDir\$QtVer\bin\qt-cmake.bat" -S . -B build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DBUILD_TESTING=OFF

if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

Write-Host "=== Building ===" -ForegroundColor Cyan
cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

Write-Host "=== Done ===" -ForegroundColor Green
Write-Host "Run: .\run.ps1"
