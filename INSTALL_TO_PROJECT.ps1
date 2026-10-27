<#
.SYNOPSIS
    Installs the DLSS5ForUE5 source folder into an Unreal Engine project.
.DESCRIPTION
    Copies the repository's source-only plugin folder. Existing installations
    are backed up under Project/Saved/DLSS5ForUE5Backups, never deleted.
    Source installations still need compiling and an authorized NR runtime.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot
)

$ErrorActionPreference = 'Stop'
$FolderName = 'DLSS5ForUE5'
$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)
$Source = Join-Path $PSScriptRoot "Plugins\$FolderName"
$DestRoot = Join-Path $ProjectRoot 'Plugins'
$Dest = Join-Path $DestRoot $FolderName

if (-not (Test-Path -LiteralPath $ProjectRoot -PathType Container)) {
    throw "Project folder does not exist: $ProjectRoot"
}
if (-not (Get-ChildItem -LiteralPath $ProjectRoot -Filter '*.uproject' -File | Select-Object -First 1)) {
    throw "No .uproject file found at project root: $ProjectRoot"
}
if (-not (Test-Path -LiteralPath (Join-Path $Source "$FolderName.uplugin") -PathType Leaf)) {
    throw "Plugin source not found: $Source"
}
if ([System.IO.Path]::GetFullPath($Source).TrimEnd('\') -ieq
    [System.IO.Path]::GetFullPath($Dest).TrimEnd('\')) {
    throw 'Source and destination are the same folder. No installation necessary.'
}

New-Item -ItemType Directory -Force -Path $DestRoot | Out-Null
$Stage = Join-Path $DestRoot (".$FolderName.staging.$([guid]::NewGuid().ToString('N'))")
$Backup = $null

try {
    Copy-Item -LiteralPath $Source -Destination $Stage -Recurse -Force
    if (-not (Test-Path -LiteralPath (Join-Path $Stage "$FolderName.uplugin") -PathType Leaf)) {
        throw 'Staging copy is missing the plugin descriptor.'
    }

    if (Test-Path -LiteralPath $Dest) {
        $BackupRoot = Join-Path $ProjectRoot 'Saved\DLSS5ForUE5Backups'
        New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
        $Backup = Join-Path $BackupRoot ("$FolderName.$((Get-Date).ToString('yyyyMMdd-HHmmss')).$([guid]::NewGuid().ToString('N').Substring(0,8))")
        Move-Item -LiteralPath $Dest -Destination $Backup
        Write-Host "Previous installation backed up to: $Backup"
    }

    try {
        Move-Item -LiteralPath $Stage -Destination $Dest
    }
    catch {
        if ($Backup -and -not (Test-Path -LiteralPath $Dest) -and (Test-Path -LiteralPath $Backup)) {
            Move-Item -LiteralPath $Backup -Destination $Dest
            Write-Warning 'Previous installation was restored after the copy failed.'
        }
        throw
    }
}
finally {
    if (Test-Path -LiteralPath $Stage) {
        Remove-Item -LiteralPath $Stage -Recurse -Force
    }
}

$LegacyFolders = @('DLSS5NeuralRendering', 'nvngx.dll_DLSS5NeuralRendering')
foreach ($Legacy in $LegacyFolders) {
    $LegacyPath = Join-Path $DestRoot $Legacy
    if (Test-Path -LiteralPath $LegacyPath) {
        Write-Warning "Legacy plugin folder detected and preserved: $LegacyPath. Review manually to avoid duplicate plugins."
    }
}

$Version = (Get-Content -LiteralPath (Join-Path $Dest "$FolderName.uplugin") -Raw | ConvertFrom-Json).VersionName
Write-Host "Installed $FolderName $Version to: $Dest"
Write-Host 'This is a source installation. Compile for your UE version and supply any required runtime under its applicable license.'
Write-Host 'Plugin UI: Tools > DLSS 5 for UE5'
Write-Host 'Console diagnostics: DLSS5.Status'
