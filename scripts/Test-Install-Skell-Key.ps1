[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PackageRoot,
    [string]$TestRoot = (Join-Path ([System.IO.Path]::GetTempPath()) ('Skell-Key-Install-Test-' + [Guid]::NewGuid().ToString('N')))
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:assertions = 0

function Assert([bool]$Condition, [string]$Message) {
    $script:assertions++
    if (-not $Condition) { throw $Message }
}
function AssertThrows([scriptblock]$Action, [string]$Message) {
    $script:assertions++
    $didThrow = $false
    try { & $Action | Out-Null } catch { $didThrow = $true }
    if (-not $didThrow) { throw $Message }
}
function WriteJson([string]$Path, [object]$Value) {
    [System.IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 30), [System.Text.UTF8Encoding]::new($false))
}
function RebuildMainManifest([string]$Root) {
    $rootPath = [System.IO.Path]::GetFullPath($Root)
    $excluded = @('SHA256SUMS.txt','TOOLS-SHA256SUMS.txt','Install-Skell-Key.ps1','INSTALL-WINDOWS.txt','THIRD-PARTY-NOTICES.txt')
    $lines = foreach ($file in Get-ChildItem -LiteralPath $rootPath -Recurse -File -Force | Sort-Object FullName) {
        $relative = $file.FullName.Substring($rootPath.Length).TrimStart([char[]]@('\','/')).Replace('\','/')
        if ($relative -notin $excluded) {
            $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            "$hash  $relative"
        }
    }
    [System.IO.File]::WriteAllLines((Join-Path $rootPath 'SHA256SUMS.txt'), [string[]]$lines, [System.Text.Encoding]::ASCII)
}

$root = [System.IO.Path]::GetFullPath($PackageRoot)
$installer = Join-Path $root 'Install-Skell-Key.ps1'
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) { throw 'Packaged installer is missing.' }
if (-not (Test-Path -LiteralPath (Join-Path $root 'SHA256SUMS.txt') -PathType Leaf) -or
    -not (Test-Path -LiteralPath (Join-Path $root 'TOOLS-SHA256SUMS.txt') -PathType Leaf)) {
    throw 'Package checksum manifests are missing.'
}

New-Item -ItemType Directory -Path $TestRoot -Force | Out-Null
try {
    $whatIfTarget = Join-Path $TestRoot 'WhatIf\VST3'
    $whatIfBackups = Join-Path $TestRoot 'WhatIf\Backups'
    $dry = & $installer -PackageRoot $root -Vst3Directory $whatIfTarget -BackupRoot $whatIfBackups -WhatIf
    Assert ($dry.status -eq 'what-if') 'WhatIf did not return a dry-run status.'
    Assert (-not (Test-Path -LiteralPath $whatIfTarget)) 'WhatIf created the install directory.'
    Assert (-not (Test-Path -LiteralPath $whatIfBackups)) 'WhatIf created the backup directory.'

    $targetRoot = Join-Path $TestRoot 'Installed\VST3'
    $backupRoot = Join-Path $TestRoot 'Installed\Backups'
    $fresh = & $installer -PackageRoot $root -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    Assert ($fresh.status -eq 'installed') 'Fresh install failed.'
    Assert ($fresh.version -eq '0.1.0') 'Fresh install reported the wrong version.'
    Assert ($null -eq $fresh.backup_path) 'Fresh install unexpectedly created a backup.'
    Assert (Test-Path -LiteralPath (Join-Path $targetRoot 'Skell Key.vst3\Contents\x86_64-win\Skell Key.vst3')) 'Installed Windows VST3 module is missing.'
    Assert (-not (Test-Path -LiteralPath $backupRoot)) 'Fresh install created an unnecessary backup.'

    $same = & $installer -PackageRoot $root -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    Assert ($same.status -eq 'already-installed') 'Repeat install was not idempotent.'
    Assert ($null -eq $same.backup_path) 'Idempotent install unexpectedly returned a backup.'

    $upgradeRoot = Join-Path $TestRoot 'UpgradePackage'
    Copy-Item -LiteralPath $root -Destination $upgradeRoot -Recurse
    $moduleInfoPath = Join-Path $upgradeRoot 'Skell Key.vst3\Contents\Resources\moduleinfo.json'
    $info = [System.IO.File]::ReadAllText($moduleInfoPath) | ConvertFrom-Json
    $info.Version = '0.1.1'
    foreach ($class in $info.Classes) { $class.Version = '0.1.1' }
    WriteJson $moduleInfoPath $info
    $moduleInfoHash = (Get-FileHash -LiteralPath $moduleInfoPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $moduleInfoBytes = (Get-Item -LiteralPath $moduleInfoPath).Length

    $verificationPath = Join-Path $upgradeRoot 'Skell-Key-moduleinfo-verification.json'
    $verification = [System.IO.File]::ReadAllText($verificationPath) | ConvertFrom-Json
    $verification.version = '0.1.1'
    $verification.source_moduleinfo_sha256 = $moduleInfoHash
    $verification.staged_moduleinfo_sha256 = $moduleInfoHash
    $verification.staged_moduleinfo_bytes = $moduleInfoBytes
    WriteJson $verificationPath $verification

    $buildInfoPath = Join-Path $upgradeRoot 'build-info.json'
    $buildInfo = [System.IO.File]::ReadAllText($buildInfoPath) | ConvertFrom-Json
    $buildInfo.version = '0.1.1'
    WriteJson $buildInfoPath $buildInfo

    $manifestPath = Join-Path $upgradeRoot 'Skell-Key-Windows-VST3-manifest.txt'
    $manifest = [System.IO.File]::ReadAllText($manifestPath)
    $manifest = [regex]::Replace($manifest, '(?m)^Version: .+$', 'Version: 0.1.1')
    [System.IO.File]::WriteAllText($manifestPath, $manifest, [System.Text.UTF8Encoding]::new($false))
    RebuildMainManifest $upgradeRoot

    $updated = & (Join-Path $upgradeRoot 'Install-Skell-Key.ps1') -PackageRoot $upgradeRoot -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    Assert ($updated.status -eq 'installed' -and $updated.version -eq '0.1.1') 'Upgrade did not install the new version.'
    Assert (-not [string]::IsNullOrWhiteSpace([string]$updated.backup_path)) 'Upgrade did not create a verified backup.'
    Assert (Test-Path -LiteralPath (Join-Path $updated.backup_path 'backup-manifest.json')) 'Backup manifest is missing.'
    Assert (Test-Path -LiteralPath (Join-Path $updated.backup_path 'Skell Key.vst3')) 'Backup bundle is missing.'

    $badBackup = Join-Path $TestRoot 'TamperedBackup'
    Copy-Item -LiteralPath $updated.backup_path -Destination $badBackup -Recurse
    Add-Content -LiteralPath (Join-Path $badBackup 'Skell Key.vst3\Contents\Resources\moduleinfo.json') -Value 'tampered'
    AssertThrows {
        & $installer -RollbackFrom $badBackup -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    } 'Tampered backup was accepted.'

    $rollback = & $installer -RollbackFrom $updated.backup_path -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    Assert ($rollback.status -eq 'rolled-back' -and $rollback.version -eq '0.1.0') 'Rollback did not restore the previous version.'
    Assert (-not [string]::IsNullOrWhiteSpace([string]$rollback.backup_path)) 'Rollback did not preserve the replaced version.'

    $tamperedPackage = Join-Path $TestRoot 'TamperedPackage'
    Copy-Item -LiteralPath $root -Destination $tamperedPackage -Recurse
    Add-Content -LiteralPath (Join-Path $tamperedPackage 'PREVIEW-ONLY.txt') -Value 'tampered'
    $tamperedTarget = Join-Path $TestRoot 'TamperedInstall\VST3'
    AssertThrows {
        & (Join-Path $tamperedPackage 'Install-Skell-Key.ps1') -PackageRoot $tamperedPackage -Vst3Directory $tamperedTarget -BackupRoot (Join-Path $TestRoot 'TamperedInstall\Backups') -Confirm:$false
    } 'Tampered package was accepted.'
    Assert (-not (Test-Path -LiteralPath $tamperedTarget)) 'Tampered package changed the install destination.'

    $invalidTargetRoot = Join-Path $TestRoot 'InvalidTarget\VST3'
    $invalidBundle = Join-Path $invalidTargetRoot 'Skell Key.vst3'
    New-Item -ItemType Directory -Path $invalidBundle -Force | Out-Null
    $sentinel = Join-Path $invalidBundle 'do-not-overwrite.txt'
    [System.IO.File]::WriteAllText($sentinel, 'preserve this unrelated directory')
    AssertThrows {
        & $installer -PackageRoot $root -Vst3Directory $invalidTargetRoot -BackupRoot (Join-Path $TestRoot 'InvalidTarget\Backups') -Confirm:$false
    } 'An invalid existing target was replaced.'
    Assert ((Get-Content -LiteralPath $sentinel -Raw) -eq 'preserve this unrelated directory') 'Invalid target contents were modified.'

    Write-Host "Skell Key preview install/update/rollback tests: $script:assertions assertions passed."
}
finally {
    if (Test-Path -LiteralPath $TestRoot) {
        Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
