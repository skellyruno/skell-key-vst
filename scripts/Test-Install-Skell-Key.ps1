[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PackageRoot,
    [string]$TestRoot = (Join-Path ([System.IO.Path]::GetTempPath()) ('Skell-Key-Install-Test-' + [Guid]::NewGuid().ToString('N')))
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:assertions = 0

function New-SafeTestRoot([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { throw 'TestRoot must be a non-empty path.' }
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $pathRoot = [System.IO.Path]::GetPathRoot($fullPath)
    $trimmedPath = $fullPath.TrimEnd([char[]]@('\', '/'))
    $trimmedRoot = $pathRoot.TrimEnd([char[]]@('\', '/'))
    if ([string]::Equals($trimmedPath, $trimmedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to use a filesystem root as TestRoot: $fullPath"
    }
    if (Test-Path -LiteralPath $fullPath) {
        throw "Refusing to use an existing TestRoot; choose a new, unused directory: $fullPath"
    }

    $parent = Split-Path -Parent $fullPath
    if ([string]::IsNullOrWhiteSpace($parent)) { throw "TestRoot has no parent directory: $fullPath" }
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    New-Item -ItemType Directory -Path $fullPath -ErrorAction Stop | Out-Null

    $token = [Guid]::NewGuid().ToString('N')
    $markerPath = Join-Path $fullPath ('.skell-key-test-root-' + $token)
    [System.IO.File]::WriteAllText($markerPath, $token, [System.Text.UTF8Encoding]::new($false))
    [pscustomobject]@{ Path = $fullPath; MarkerPath = $markerPath; Token = $token }
}

function Remove-SafeTestRoot($Lease) {
    if ($null -eq $Lease) { return }
    if (-not (Test-Path -LiteralPath $Lease.Path -PathType Container)) { return }

    $rootItem = Get-Item -LiteralPath $Lease.Path -Force
    if (($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        Write-Warning "Leaving TestRoot in place because it became a reparse point: $($Lease.Path)"
        return
    }
    if (-not (Test-Path -LiteralPath $Lease.MarkerPath -PathType Leaf)) {
        Write-Warning "Leaving TestRoot in place because its ownership marker is missing: $($Lease.Path)"
        return
    }
    $markerItem = Get-Item -LiteralPath $Lease.MarkerPath -Force
    if (($markerItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        Write-Warning "Leaving TestRoot in place because its ownership marker is a reparse point: $($Lease.Path)"
        return
    }
    $actualToken = [System.IO.File]::ReadAllText($Lease.MarkerPath)
    if ($actualToken -cne $Lease.Token) {
        Write-Warning "Leaving TestRoot in place because its ownership marker changed: $($Lease.Path)"
        return
    }
    try {
        $nestedReparsePoints = @(Get-ChildItem -LiteralPath $Lease.Path -Recurse -Force -ErrorAction Stop |
            Where-Object { ($_.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0 })
    }
    catch {
        Write-Warning "Leaving TestRoot in place because its contents could not be checked safely: $($Lease.Path)"
        return
    }
    if ($nestedReparsePoints.Count -gt 0) {
        Write-Warning "Leaving TestRoot in place because it contains a reparse point: $($nestedReparsePoints[0].FullName)"
        return
    }

    Remove-Item -LiteralPath $Lease.MarkerPath -Force
    Remove-Item -LiteralPath $Lease.Path -Recurse -Force
}


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
function CopyTree([string]$Source, [string]$Destination) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    foreach ($item in Get-ChildItem -LiteralPath $Source -Force) {
        Copy-Item -LiteralPath $item.FullName -Destination $Destination -Recurse -Force
    }
}
function New-TestJunction([string]$Path, [string]$Target) {
    if (Test-Path -LiteralPath $Path) { throw "Test junction path already exists: $Path" }
    if (-not (Test-Path -LiteralPath $Target -PathType Container)) { throw "Test junction target is missing: $Target" }
    New-Item -ItemType Junction -Path $Path -Target $Target -ErrorAction Stop | Out-Null
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -eq 0) {
        Remove-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
        throw "Test path was not created as a reparse point: $Path"
    }
}
function Remove-TestJunction([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
    if ($null -eq $item) { return }
    if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -eq 0) {
        throw "Refusing to remove a non-reparse test path: $Path"
    }
    Remove-Item -LiteralPath $Path -Force -ErrorAction Stop
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

$testRootLease = New-SafeTestRoot $TestRoot
$TestRoot = $testRootLease.Path
$junctionPaths = [System.Collections.Generic.List[string]]::new()
try {
    # Existing paths must never be claimed for recursive test cleanup.
    $existingTestRoot = Join-Path $TestRoot 'ExistingRootMustRemain'
    New-Item -ItemType Directory -Path $existingTestRoot | Out-Null
    $existingSentinel = Join-Path $existingTestRoot 'sentinel.txt'
    [System.IO.File]::WriteAllText($existingSentinel, 'preserve existing user data')
    AssertThrows { New-SafeTestRoot $existingTestRoot | Out-Null } 'An existing TestRoot was accepted.'
    Assert ((Get-Content -LiteralPath $existingSentinel -Raw) -eq 'preserve existing user data') 'Existing TestRoot data was modified.'

    # Cleanup must also refuse a directory whose ownership marker no longer matches.
    $protectedLease = New-SafeTestRoot (Join-Path $TestRoot 'CleanupGuard')
    $protectedSentinel = Join-Path $protectedLease.Path 'sentinel.txt'
    [System.IO.File]::WriteAllText($protectedSentinel, 'preserve when ownership is uncertain')
    [System.IO.File]::WriteAllText($protectedLease.MarkerPath, 'wrong-token')
    Remove-SafeTestRoot $protectedLease
    Assert (Test-Path -LiteralPath $protectedSentinel -PathType Leaf) 'Cleanup removed a directory without a valid ownership marker.'
    Remove-Item -LiteralPath $protectedLease.Path -Recurse -Force

    # The package root and all nested package directories must be real paths.
    $packageJunctionTarget = Join-Path $TestRoot 'PackageJunctionTarget'
    CopyTree $root $packageJunctionTarget
    $packageJunction = Join-Path $TestRoot 'PackageJunction'
    New-TestJunction $packageJunction $packageJunctionTarget
    $junctionPaths.Add($packageJunction)
    $packageJunctionInstall = Join-Path $TestRoot 'PackageJunctionInstall'
    AssertThrows {
        & (Join-Path $packageJunction 'Install-Skell-Key.ps1') -PackageRoot $packageJunction -Vst3Directory (Join-Path $packageJunctionInstall 'VST3') -WhatIf
    } 'Package-root junction was accepted.'
    Assert (-not (Test-Path -LiteralPath $packageJunctionInstall)) 'Package-root junction created an install destination.'

    $externalPackageData = Join-Path $TestRoot 'ExternalPackageData'
    New-Item -ItemType Directory -Path $externalPackageData | Out-Null
    $nestedPackageJunction = Join-Path $packageJunctionTarget 'ExternalData'
    New-TestJunction $nestedPackageJunction $externalPackageData
    $junctionPaths.Add($nestedPackageJunction)
    AssertThrows {
        & $installer -PackageRoot $packageJunctionTarget -WhatIf
    } 'A nested package junction was accepted.'
    Remove-TestJunction $nestedPackageJunction
    $junctionPaths.Remove($nestedPackageJunction) | Out-Null

    # Destination roots and pre-existing bundle roots must not be junctions.
    $destinationTarget = Join-Path $TestRoot 'DestinationJunctionTarget'
    New-Item -ItemType Directory -Path $destinationTarget | Out-Null
    $destinationJunction = Join-Path $TestRoot 'DestinationJunction'
    New-TestJunction $destinationJunction $destinationTarget
    $junctionPaths.Add($destinationJunction)
    AssertThrows {
        & $installer -PackageRoot $root -Vst3Directory $destinationJunction -WhatIf
    } 'A VST3 destination-root junction was accepted.'
    Assert ((@(Get-ChildItem -LiteralPath $destinationTarget -Force)).Count -eq 0) 'Destination-root junction target was modified.'

    $bundleLinkRoot = Join-Path $TestRoot 'BundleJunctionInstall\VST3'
    New-Item -ItemType Directory -Path $bundleLinkRoot -Force | Out-Null
    $bundleJunction = Join-Path $bundleLinkRoot 'Skell Key.vst3'
    New-TestJunction $bundleJunction (Join-Path $root 'Skell Key.vst3')
    $junctionPaths.Add($bundleJunction)
    AssertThrows {
        & $installer -PackageRoot $root -Vst3Directory $bundleLinkRoot -Confirm:$false
    } 'A pre-existing VST3 bundle junction was accepted.'
    Assert (Test-Path -LiteralPath $bundleJunction -PathType Container) 'Rejected bundle junction was removed or replaced.'

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
    CopyTree $root $upgradeRoot
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

    $backupJunction = Join-Path $TestRoot 'BackupJunction'
    New-TestJunction $backupJunction $updated.backup_path
    $junctionPaths.Add($backupJunction)
    AssertThrows {
        & $installer -RollbackFrom $backupJunction -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    } 'A backup-root junction was accepted.'
    Assert (Test-Path -LiteralPath (Join-Path $updated.backup_path 'backup-manifest.json')) 'Rejected backup junction altered the original backup.'

    $badBackup = Join-Path $TestRoot 'TamperedBackup'
    CopyTree $updated.backup_path $badBackup
    Add-Content -LiteralPath (Join-Path $badBackup 'Skell Key.vst3\Contents\Resources\moduleinfo.json') -Value 'tampered'
    AssertThrows {
        & $installer -RollbackFrom $badBackup -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    } 'Tampered backup was accepted.'

    $rollback = & $installer -RollbackFrom $updated.backup_path -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    Assert ($rollback.status -eq 'rolled-back' -and $rollback.version -eq '0.1.0') 'Rollback did not restore the previous version.'
    Assert (-not [string]::IsNullOrWhiteSpace([string]$rollback.backup_path)) 'Rollback did not preserve the replaced version.'

    $tamperedPackage = Join-Path $TestRoot 'TamperedPackage'
    CopyTree $root $tamperedPackage
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
    foreach ($junctionPath in $junctionPaths) {
        try { Remove-TestJunction $junctionPath }
        catch { Write-Warning ("Could not remove test junction safely: " + $junctionPath + " — " + $_.Exception.Message) }
    }
    Remove-SafeTestRoot $testRootLease
}
