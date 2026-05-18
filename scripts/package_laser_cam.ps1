param(
    [string]$ProjectRoot = "E:\haohaohao",
    [string]$BuildDir = "E:\haohaohao\build",
    [string]$PackageDir = "E:\laser_cam",
    [string]$Config = "Release",
    [string]$ShortcutName = "Laser CAM",
    [switch]$SkipBuild,
    [switch]$NoDesktopShortcut
)

$ErrorActionPreference = "Stop"

function Write-Step([string]$message) {
    Write-Host "[package] $message" -ForegroundColor Cyan
}

function Ensure-Dir([string]$path) {
    if (!(Test-Path $path)) {
        New-Item -ItemType Directory -Path $path | Out-Null
    }
}

function Copy-IfExists([string]$src, [string]$dst) {
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $dst -Recurse -Force
    }
}

function New-Shortcut([string]$shortcutPath, [string]$targetPath, [string]$workDir) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = $targetPath
    $shortcut.WorkingDirectory = $workDir
    $shortcut.IconLocation = "$targetPath,0"
    $shortcut.Save()
}

$sourceBin = Join-Path $BuildDir ("bin\" + $Config)
$mainExe = Join-Path $sourceBin "laser_cam_qt.exe"

if (-not $SkipBuild) {
    Write-Step "Build $Config"
    cmake --build $BuildDir --config $Config --target laser_cam_qt -j 8
}

if (!(Test-Path $mainExe)) {
    throw "Cannot find executable: $mainExe"
}

Write-Step "Prepare package directory: $PackageDir"
if (Test-Path $PackageDir) {
    Remove-Item -Path $PackageDir -Recurse -Force
}
Ensure-Dir $PackageDir

Write-Step "Copy executable and runtime files"
Copy-Item -Path (Join-Path $sourceBin "laser_cam_qt.exe") -Destination $PackageDir -Force
Copy-IfExists (Join-Path $sourceBin "qt.conf") $PackageDir
Copy-Item -Path (Join-Path $sourceBin "*.dll") -Destination $PackageDir -Force

$runtimeDirs = @(
    "platforms",
    "styles",
    "imageformats",
    "iconengines",
    "networkinformation",
    "tls",
    "translations",
    "generic"
)
foreach ($dirName in $runtimeDirs) {
    Copy-IfExists (Join-Path $sourceBin $dirName) (Join-Path $PackageDir $dirName)
}

Write-Step "Create extra directories"
Ensure-Dir (Join-Path $PackageDir "log")

Write-Step "Create package shortcut"
$pkgShortcut = Join-Path $PackageDir ($ShortcutName + ".lnk")
New-Shortcut -shortcutPath $pkgShortcut -targetPath (Join-Path $PackageDir "laser_cam_qt.exe") -workDir $PackageDir

if (-not $NoDesktopShortcut) {
    $desktopDir = [Environment]::GetFolderPath("Desktop")
    if ($desktopDir -and (Test-Path $desktopDir)) {
        Write-Step "Create desktop shortcut"
        $desktopShortcut = Join-Path $desktopDir ($ShortcutName + ".lnk")
        New-Shortcut -shortcutPath $desktopShortcut -targetPath (Join-Path $PackageDir "laser_cam_qt.exe") -workDir $PackageDir
    }
}

Write-Step "Done"
Write-Host "Package: $PackageDir" -ForegroundColor Green
Write-Host "Run: $pkgShortcut" -ForegroundColor Green
