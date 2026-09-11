param(
    [Parameter(Mandatory=$true)]
    [string]$ProjectRoot
)

$ErrorActionPreference = "Stop"
$FolderName = "DLSS5ForUE5"
$Source = Join-Path $PSScriptRoot ("Plugins\" + $FolderName)
$DestRoot = Join-Path $ProjectRoot "Plugins"
$Dest = Join-Path $DestRoot $FolderName

$LegacyFolders = @(
    "DLSS5NeuralRendering",
    "nvngx.dll_DLSS5NeuralRendering"
)

if (!(Test-Path $Source)) { throw "Plugin source not found: $Source" }
New-Item -ItemType Directory -Force -Path $DestRoot | Out-Null

foreach ($Legacy in $LegacyFolders) {
    $LegacyPath = Join-Path $DestRoot $Legacy
    if (Test-Path $LegacyPath) {
        Write-Host "Removing legacy plugin folder: $LegacyPath"
        Remove-Item -Recurse -Force $LegacyPath
    }
}

if (Test-Path $Dest) { Remove-Item -Recurse -Force $Dest }
Copy-Item -Recurse -Force $Source $Dest

Write-Host "Installed DLSS5ForUE5 v0.5.6 to: $Dest"
Write-Host "Plugin UI: Tools > DLSS 5 for UE5"
Write-Host "Console diagnostics: DLSS5.Status"
Write-Host "Console UI shortcut: DLSS5.Open"
Write-Host "CVar namespace: r.DLSS5.*"
