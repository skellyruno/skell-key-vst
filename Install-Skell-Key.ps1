[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'High')]
param(
    [string]$PackageRoot,
    [string]$RollbackFrom,
    [string]$Vst3Directory = (Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'),
    [string]$BackupRoot = (Join-Path $env:LOCALAPPDATA 'Skell Audio\Backups\Skell-Key')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Product = 'Skell Key'
$Vendor = 'Skell Audio'
$BundleName = 'Skell Key.vst3'
$BinaryRelative = 'Contents\x86_64-win\Skell Key.vst3'
$ExpectedClassIds = @(
    'ABCDEF019182FAEB496E5263546E5263',
    'ABCDEF011234ABCD496E5263546E5263'
)

if ([string]::IsNullOrWhiteSpace($PackageRoot) -eq [string]::IsNullOrWhiteSpace($RollbackFrom)) {
    throw 'Supply exactly one of -PackageRoot or -RollbackFrom.'
}

function Get-FullPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { throw 'Empty path.' }
    [System.IO.Path]::GetFullPath($Path)
}

function Get-TreeHash([string]$Directory) {
    $root = Get-FullPath $Directory
    if (-not (Test-Path -LiteralPath $root -PathType Container)) {
        throw "Missing bundle directory: $root"
    }
    $items = @(Get-ChildItem -LiteralPath $root -Recurse -Force)
    foreach ($item in $items) {
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Reparse point rejected in bundle: $($item.FullName)"
        }
    }
    $files = @($items | Where-Object { -not $_.PSIsContainer } |
        Sort-Object { $_.FullName.Substring($root.Length).Replace('\', '/') })
    if ($files.Count -eq 0) { throw "Empty bundle: $root" }
    $lines = foreach ($file in $files) {
        $relative = $file.FullName.Substring($root.Length).TrimStart([char[]]@('\', '/')).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$relative  $hash"
    }
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $payload = [string]::Join([char]10, [string[]]$lines)
        [System.BitConverter]::ToString($sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($payload))).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose() }
}

function Read-ChecksumManifest([string]$Root, [string]$Name) {
    $rootPath = Get-FullPath $Root
    $manifestPath = Join-Path $rootPath $Name
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw "Missing checksum manifest: $Name" }
    $prefix = $rootPath.TrimEnd([char[]]@('\', '/')) + [System.IO.Path]::DirectorySeparatorChar
    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $paths = [System.Collections.Generic.List[string]]::new()
    foreach ($line in [System.IO.File]::ReadAllLines($manifestPath, [System.Text.Encoding]::ASCII)) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        if ($line -notmatch '^(?<hash>[0-9a-fA-F]{64})  (?<path>.+)$') {
            throw ('Malformed line in ' + $Name + ': ' + $line)
        }
        $expected = $Matches.hash.ToLowerInvariant()
        $relative = $Matches.path
        if ([System.IO.Path]::IsPathRooted($relative) -or $relative.Contains(':') -or
            (($relative -split '[\\/]') -contains '..') -or (($relative -split '[\\/]') -contains '.')) {
            throw ('Unsafe path in ' + $Name + ': ' + $relative)
        }
        $full = Get-FullPath (Join-Path $rootPath ($relative.Replace('/', [System.IO.Path]::DirectorySeparatorChar)))
        if (-not $full.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Path escapes package: $relative"
        }
        if (-not $seen.Add($relative)) { throw "Duplicate checksum entry: $relative" }
        if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "Missing checksummed file: $relative" }
        $actual = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $expected) { throw "Checksum mismatch: $relative" }
        $paths.Add($relative.Replace('\', '/'))
    }
    if ($paths.Count -eq 0) { throw "$Name is empty." }
    return ,$paths.ToArray()
}

function Get-BundleEvidence([string]$Bundle) {
    $root = Get-FullPath $Bundle
    $infoPath = Join-Path $root 'Contents\Resources\moduleinfo.json'
    $modulePath = Join-Path $root $BinaryRelative
    $contentsPath = Join-Path $root 'Contents'
    if (-not (Test-Path -LiteralPath $infoPath -PathType Leaf)) { throw 'Missing moduleinfo.json.' }
    if (-not (Test-Path -LiteralPath $modulePath -PathType Leaf)) { throw 'Missing Windows VST3 module.' }
    if (-not (Test-Path -LiteralPath $contentsPath -PathType Container)) { throw 'Missing VST3 Contents directory.' }

    $modules = @(Get-ChildItem -LiteralPath $contentsPath -Recurse -File -Filter '*.vst3')
    if ($modules.Count -ne 1 -or $modules[0].FullName -ne (Get-FullPath $modulePath)) {
        throw 'Expected exactly one Windows VST3 module in the bundle.'
    }

    $info = [System.IO.File]::ReadAllText($infoPath) | ConvertFrom-Json -ErrorAction Stop
    if ($null -eq $info -or $info -is [array] -or $info.Name -ne $Product -or $info.'Factory Info'.Vendor -ne $Vendor) {
        throw 'VST3 product/vendor metadata mismatch.'
    }
    $classes = @($info.Classes)
    if ($classes.Count -ne 2) { throw "Expected exactly two VST3 classes, found $($classes.Count)." }
    $ids = @($classes | ForEach-Object { [string]$_.CID } | Sort-Object)
    $expectedIds = @($ExpectedClassIds | Sort-Object)
    if ([string]::Join('|', [string[]]$ids) -ne [string]::Join('|', [string[]]$expectedIds)) {
        throw 'VST3 class IDs changed.'
    }
    $categories = @($classes | ForEach-Object { [string]$_.Category } | Sort-Object)
    if ([string]::Join('|', [string[]]$categories) -ne 'Audio Module Class|Component Controller Class') {
        throw 'VST3 processor/controller class categories changed.'
    }
    $version = [string]$info.Version
    if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "Invalid VST3 version: $version" }

    $bytes = [System.IO.File]::ReadAllBytes($modulePath)
    if ($bytes.Length -lt 128 -or $bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) { throw 'VST3 module is missing a valid MZ header.' }
    $offset = [System.BitConverter]::ToInt32($bytes, 0x3C)
    if ($offset -lt 64 -or ($offset + 26) -gt $bytes.Length) { throw 'VST3 module has an invalid PE header offset.' }
    if ($bytes[$offset] -ne 0x50 -or $bytes[$offset + 1] -ne 0x45 -or $bytes[$offset + 2] -ne 0 -or $bytes[$offset + 3] -ne 0) {
        throw 'VST3 module is missing the PE signature.'
    }
    if ([System.BitConverter]::ToUInt16($bytes, $offset + 4) -ne 0x8664 -or
        [System.BitConverter]::ToUInt16($bytes, $offset + 24) -ne 0x20B) {
        throw 'VST3 module is not AMD64 PE32+.'
    }

    [pscustomobject]@{
        version = $version
        binary_sha256 = (Get-FileHash -LiteralPath $modulePath -Algorithm SHA256).Hash.ToLowerInvariant()
        moduleinfo_sha256 = (Get-FileHash -LiteralPath $infoPath -Algorithm SHA256).Hash.ToLowerInvariant()
        tree_sha256 = Get-TreeHash $root
    }
}

function Get-VerifiedPackage([string]$Root) {
    $rootPath = Get-FullPath $Root
    $buildInfoPath = Join-Path $rootPath 'build-info.json'
    if (-not (Test-Path -LiteralPath $buildInfoPath -PathType Leaf)) { throw 'Missing build-info.json.' }
    $build = [System.IO.File]::ReadAllText($buildInfoPath) | ConvertFrom-Json -ErrorAction Stop
    if ($build.schema_version -ne 1 -or $build.product -ne $Product -or $build.format -ne 'VST3' -or
        $build.platform -ne 'Windows x86_64' -or $build.release_status -ne 'preview-only; not a commercial release') {
        throw 'Not a Skell Key Windows private-preview package.'
    }
    if ([string]$build.version -notmatch '^\d+\.\d+\.\d+$' -or
        [string]$build.source_head_sha -notmatch '^[0-9a-f]{40}$' -or
        [string]$build.tested_workflow_sha -notmatch '^[0-9a-f]{40}$' -or
        [string]$build.workflow_run_id -notmatch '^\d+$') {
        throw 'Build provenance is incomplete or malformed.'
    }

    $mainPaths = @(Read-ChecksumManifest $rootPath 'SHA256SUMS.txt')
    $toolPaths = @(Read-ChecksumManifest $rootPath 'TOOLS-SHA256SUMS.txt')
    $toolFiles = @('Install-Skell-Key.ps1', 'INSTALL-WINDOWS.txt', 'THIRD-PARTY-NOTICES.txt')
    $sortedTools = @($toolPaths | Sort-Object)
    $expectedTools = @($toolFiles | Sort-Object)
    if ([string]::Join('|', [string[]]$sortedTools) -ne [string]::Join('|', [string[]]$expectedTools)) {
        throw 'Tool checksum manifest must cover exactly the installer, install guide and third-party notice.'
    }

    $excluded = @('SHA256SUMS.txt', 'TOOLS-SHA256SUMS.txt') + $toolFiles
    $allFiles = @(Get-ChildItem -LiteralPath $rootPath -Recurse -File -Force)
    $expectedMain = @($allFiles | ForEach-Object {
        $_.FullName.Substring($rootPath.Length).TrimStart([char[]]@('\', '/')).Replace('\', '/')
    } | Where-Object { $_ -notin $excluded } | Sort-Object)
    $sortedMain = @($mainPaths | Sort-Object)
    if ([string]::Join('|', [string[]]$sortedMain) -ne [string]::Join('|', [string[]]$expectedMain)) {
        throw 'Main checksum manifest does not cover exactly every release payload file.'
    }

    $bundleDirs = @(Get-ChildItem -LiteralPath $rootPath -Directory -Filter '*.vst3' -Force)
    if ($bundleDirs.Count -ne 1 -or $bundleDirs[0].Name -ne $BundleName) {
        throw 'Package must contain exactly one top-level Skell Key.vst3 bundle.'
    }
    foreach ($required in @(
        'build-info.json',
        'PREVIEW-ONLY.txt',
        'RELEASE-NOTES.txt',
        'Skell-Key-Windows-VST3-manifest.txt',
        'Skell-Key-Windows-VST3.sha256',
        'Skell-Key-moduleinfo-verification.json',
        'Skell Key.vst3/Contents/Resources/moduleinfo.json',
        'Skell Key.vst3/Contents/x86_64-win/Skell Key.vst3'
    )) {
        if ($mainPaths -notcontains $required) { throw "Main checksum manifest omits $required." }
    }

    $bundle = Join-Path $rootPath $BundleName
    $evidence = Get-BundleEvidence $bundle
    if ($evidence.version -ne [string]$build.version) { throw 'Build and VST3 module versions differ.' }
    foreach ($file in Get-ChildItem -LiteralPath $bundle -Recurse -File -Force) {
        $relative = $BundleName + '/' + $file.FullName.Substring($bundle.Length).TrimStart([char[]]@('\', '/')).Replace('\', '/')
        if ($mainPaths -notcontains $relative) { throw "Main checksum manifest omits bundle file $relative." }
    }

    $verificationPath = Join-Path $rootPath 'Skell-Key-moduleinfo-verification.json'
    $verification = [System.IO.File]::ReadAllText($verificationPath) | ConvertFrom-Json -ErrorAction Stop
    if ($verification.product -ne $Product -or $verification.vendor -ne $Vendor -or
        $verification.version -ne $evidence.version -or $verification.staged_moduleinfo_sha256 -ne $evidence.moduleinfo_sha256) {
        throw 'Strict moduleinfo verification manifest does not match the staged bundle.'
    }
    $verifiedIds = @($verification.class_ids | ForEach-Object { [string]$_ } | Sort-Object)
    $actualIds = @($ExpectedClassIds | Sort-Object)
    if ([string]::Join('|', [string[]]$verifiedIds) -ne [string]::Join('|', [string[]]$actualIds)) {
        throw 'Moduleinfo verification manifest class IDs changed.'
    }

    $sidecarPath = Join-Path $rootPath 'Skell-Key-Windows-VST3.sha256'
    $sidecar = [System.IO.File]::ReadAllText($sidecarPath)
    if ($sidecar -notmatch '(?m)^(?<hash>[0-9a-f]{64})  Skell Key\.vst3/Contents/x86_64-win/Skell Key\.vst3\s*$' -or
        $Matches.hash -ne $evidence.binary_sha256) {
        throw 'VST3 binary SHA-256 sidecar does not match the module.'
    }

    $manifest = [System.IO.File]::ReadAllText((Join-Path $rootPath 'Skell-Key-Windows-VST3-manifest.txt'))
    foreach ($expected in @(
        "Product: $Product",
        "Version: $($evidence.version)",
        'Format: VST3',
        'Platform: Windows x86_64',
        "Source head commit: $($build.source_head_sha)",
        "Tested workflow commit: $($build.tested_workflow_sha)",
        "Workflow run ID: $($build.workflow_run_id)",
        "Binary SHA-256: $($evidence.binary_sha256)"
    )) {
        if (-not $manifest.Contains($expected)) { throw "Release manifest omits expected provenance: $expected" }
    }

    $previewText = [System.IO.File]::ReadAllText((Join-Path $rootPath 'PREVIEW-ONLY.txt'))
    if ($previewText -notmatch 'preview-only' -or $previewText -notmatch 'not a commercial release') {
        throw 'Explicit preview-only warning is missing.'
    }

    [pscustomobject]@{ root = $rootPath; bundle = $bundle; version = $evidence.version; evidence = $evidence }
}

function Get-VerifiedBackup([string]$BackupPath) {
    $root = Get-FullPath $BackupPath
    $manifestPath = Join-Path $root 'backup-manifest.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'Backup manifest is missing.' }
    $manifest = [System.IO.File]::ReadAllText($manifestPath) | ConvertFrom-Json -ErrorAction Stop
    if ($manifest.schema_version -ne 1 -or $manifest.product -ne $Product) { throw 'Unsupported or mismatched backup manifest.' }
    $bundle = Join-Path $root $BundleName
    $evidence = Get-BundleEvidence $bundle
    if ($evidence.version -ne [string]$manifest.version -or
        $evidence.binary_sha256 -ne [string]$manifest.binary_sha256 -or
        $evidence.tree_sha256 -ne [string]$manifest.bundle_tree_sha256) {
        throw 'Backup integrity verification failed.'
    }
    [pscustomobject]@{ root = $root; bundle = $bundle; version = $evidence.version; evidence = $evidence }
}

function New-VerifiedBackup([string]$Bundle, [string]$Reason, [string]$OriginalTarget) {
    $evidence = Get-BundleEvidence $Bundle
    $backupRoot = Get-FullPath $BackupRoot
    New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
    $stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')
    $suffix = [Guid]::NewGuid().ToString('N').Substring(0, 8)
    $path = Join-Path $backupRoot ("Skell-Key-" + $evidence.version + "-" + $stamp + "-" + $suffix)
    New-Item -ItemType Directory -Path $path | Out-Null
    $copy = Join-Path $path $BundleName
    Copy-Item -LiteralPath $Bundle -Destination $copy -Recurse
    $copied = Get-BundleEvidence $copy
    if ($copied.tree_sha256 -ne $evidence.tree_sha256) {
        throw "Backup verification failed; original installation remains untouched: $path"
    }
    $backupManifest = [ordered]@{
        schema_version = 1
        product = $Product
        created_utc = [DateTime]::UtcNow.ToString('o')
        version = $evidence.version
        binary_sha256 = $evidence.binary_sha256
        bundle_tree_sha256 = $evidence.tree_sha256
        reason = $Reason
        original_target = $OriginalTarget
    }
    [System.IO.File]::WriteAllText(
        (Join-Path $path 'backup-manifest.json'),
        ($backupManifest | ConvertTo-Json -Depth 6),
        [System.Text.UTF8Encoding]::new($false)
    )
    $path
}

function Replace-VerifiedBundle([string]$Source, [string]$Target, [string]$Reason) {
    $expected = Get-BundleEvidence $Source
    $targetRoot = Split-Path -Parent $Target
    $backupPath = $null
    if (Test-Path -LiteralPath $Target) {
        if (-not (Test-Path -LiteralPath $Target -PathType Container)) {
            throw "Existing target is not a VST3 directory; refusing to replace it: $Target"
        }
        $current = Get-BundleEvidence $Target
        if ($current.tree_sha256 -eq $expected.tree_sha256) {
            return [pscustomobject]@{ status = 'already-installed'; version = $current.version; target = $Target; backup_path = $null; binary_sha256 = $current.binary_sha256 }
        }
        $backupPath = New-VerifiedBackup $Target $Reason $Target
    }

    New-Item -ItemType Directory -Path $targetRoot -Force | Out-Null
    $id = [Guid]::NewGuid().ToString('N')
    $stage = Join-Path $targetRoot ('.Skell-Key-stage-' + $id + '.vst3')
    $previous = Join-Path $targetRoot ('.Skell-Key-previous-' + $id + '.vst3')
    $failed = Join-Path $targetRoot ('.Skell-Key-failed-' + $id + '.vst3')
    $previousMoved = $false
    $newMoved = $false
    try {
        Copy-Item -LiteralPath $Source -Destination $stage -Recurse
        $staged = Get-BundleEvidence $stage
        if ($staged.tree_sha256 -ne $expected.tree_sha256) { throw 'Staged VST3 bundle hash differs from the verified package.' }
        if (Test-Path -LiteralPath $Target) {
            Move-Item -LiteralPath $Target -Destination $previous
            $previousMoved = $true
        }
        Move-Item -LiteralPath $stage -Destination $Target
        $newMoved = $true
        $installed = Get-BundleEvidence $Target
        if ($installed.tree_sha256 -ne $expected.tree_sha256) { throw 'Installed VST3 bundle failed post-copy verification.' }
    }
    catch {
        if ($newMoved -and (Test-Path -LiteralPath $Target)) {
            try { Move-Item -LiteralPath $Target -Destination $failed -ErrorAction Stop } catch {}
        }
        if ($previousMoved -and (Test-Path -LiteralPath $previous) -and -not (Test-Path -LiteralPath $Target)) {
            Move-Item -LiteralPath $previous -Destination $Target -ErrorAction Stop
        }
        throw
    }
    finally {
        if (Test-Path -LiteralPath $stage) {
            Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    if (Test-Path -LiteralPath $previous) {
        Remove-Item -LiteralPath $previous -Recurse -Force -ErrorAction SilentlyContinue
    }
    [pscustomobject]@{
        status = 'installed'
        version = $installed.version
        target = $Target
        backup_path = $backupPath
        binary_sha256 = $installed.binary_sha256
        failed_stage_path = if (Test-Path -LiteralPath $failed) { $failed } else { $null }
    }
}

if (-not [string]::IsNullOrWhiteSpace($PackageRoot)) {
    $package = Get-VerifiedPackage $PackageRoot
    $targetRoot = Get-FullPath $Vst3Directory
    $target = Join-Path $targetRoot $BundleName
    if (Test-Path -LiteralPath $target -PathType Container) {
        $current = Get-BundleEvidence $target
        if ($current.tree_sha256 -eq $package.evidence.tree_sha256) {
            return [pscustomobject]@{ status = 'already-installed'; version = $current.version; target = $target; backup_path = $null; binary_sha256 = $current.binary_sha256 }
        }
    }
    $proceed = $PSCmdlet.ShouldProcess($target, "Install Skell Key $($package.version) after verified backup")
    if (-not $proceed) {
        return [pscustomobject]@{ status = 'what-if'; version = $package.version; target = $target; backup_path = $null; binary_sha256 = $package.evidence.binary_sha256 }
    }
    return Replace-VerifiedBundle $package.bundle $target 'pre-install-or-update'
}
else {
    $backup = Get-VerifiedBackup $RollbackFrom
    $targetRoot = Get-FullPath $Vst3Directory
    $target = Join-Path $targetRoot $BundleName
    if (Test-Path -LiteralPath $target -PathType Container) {
        $current = Get-BundleEvidence $target
        if ($current.tree_sha256 -eq $backup.evidence.tree_sha256) {
            return [pscustomobject]@{ status = 'already-restored'; version = $current.version; target = $target; backup_path = $null; binary_sha256 = $current.binary_sha256 }
        }
    }
    $proceed = $PSCmdlet.ShouldProcess($target, "Restore Skell Key $($backup.version) from verified backup")
    if (-not $proceed) {
        return [pscustomobject]@{ status = 'what-if'; version = $backup.version; target = $target; backup_path = $null; binary_sha256 = $backup.evidence.binary_sha256 }
    }
    $result = Replace-VerifiedBundle $backup.bundle $target 'pre-rollback'
    [pscustomobject]@{ status = 'rolled-back'; version = $result.version; target = $result.target; backup_path = $result.backup_path; binary_sha256 = $result.binary_sha256 }
}
