# Post-Grape - make the release zip.
# Result: dist\Post-Grape-v<version>-win64.zip with ONE folder "Post-Grape"
# that contains ONE file "Post-Grape.vst3" (single-file VST3).
# Usage: powershell -ExecutionPolicy Bypass -File tools\package.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$cmake = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
if ($cmake -notmatch 'project\(PostGrape VERSION ([0-9.]+)') { throw "Version not found in CMakeLists.txt" }
$version = $Matches[1]

$plugin = Join-Path $root "bin\Post-Grape\Post-Grape.vst3"
if (-not (Test-Path $plugin -PathType Leaf)) { throw "Build the plugin first: $plugin is missing" }

$stage = Join-Path $root "dist\stage"
$dist = Join-Path $root "dist"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $stage "Post-Grape") | Out-Null
Copy-Item $plugin (Join-Path $stage "Post-Grape\Post-Grape.vst3")

Get-ChildItem $dist -Filter "*.zip" | Remove-Item -Force
$zip = Join-Path $dist "Post-Grape-v$version-win64.zip"
Compress-Archive -Path (Join-Path $stage "Post-Grape") -DestinationPath $zip -Force
Remove-Item $stage -Recurse -Force
Write-Output "Created $zip"
