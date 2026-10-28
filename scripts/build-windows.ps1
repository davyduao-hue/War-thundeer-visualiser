$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
cmake -S . -B build -A x64
if ($LASTEXITCODE) { throw 'Configuration failed' }
cmake --build build --config Release --parallel 2
if ($LASTEXITCODE) { throw 'Build failed' }
ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'Tests failed' }
cmake --install build --config Release --prefix package/WTArmourVision
if ($LASTEXITCODE) { throw 'Install failed' }
Compress-Archive package/WTArmourVision/* WTArmourVision-Windows-x64.zip -Force
