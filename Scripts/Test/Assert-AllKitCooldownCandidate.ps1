<# Read-only package/source/configuration guard. Returns a bound evidence object; launches nothing. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Candidate,
    [Parameter(Mandatory)][string]$PackageResultPath,
    [Parameter(Mandatory)][string]$ConfigurationPath,
    [Parameter(Mandatory)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ExpectedExeSHA256
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
function Inside([string]$Base,[string]$Relative){
    $path=[IO.Path]::GetFullPath((Join-Path $Base $Relative))
    if(!$path.StartsWith([IO.Path]::GetFullPath($Base)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw "Path escapes evidence root: $Relative"}
    return $path
}
$packagePath=(Resolve-Path -LiteralPath $PackageResultPath).Path
$package=Get-Content -LiteralPath $packagePath -Raw | ConvertFrom-Json
if($package.status -ne 'passed' -or $package.exitCode -ne 0 -or $package.candidate -ne $Candidate){throw 'Require the matching successful package.'}
$manifestPath=(Resolve-Path -LiteralPath $package.sourceManifest).Path
if((Hash $manifestPath) -ne $package.sourceManifestSha256){throw 'Package source manifest changed.'}
$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if(!$manifest.effectiveSourceIdentity -or $manifest.effectiveSourceIdentity -ne $package.effectiveSourceIdentity -or $manifest.sourceHead -ne $package.sourceHead){throw 'Package source identity mismatch.'}
$head=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0 -or $head -ne $manifest.sourceHead){throw 'Checkout HEAD differs from package.'}
if((Hash $package.sourceSnapshot) -ne $package.sourceSnapshotSha256){throw 'Package snapshot changed.'}
$snapshot=Get-Content -LiteralPath $package.sourceSnapshot -Raw | ConvertFrom-Json
if($snapshot.sourceHead -ne $head -or $snapshot.sourceIdentity -ne $package.sourceIdentity){throw 'Snapshot identity mismatch.'}
foreach($kind in @('patch','status','untracked')){if((Hash $snapshot.files.$kind.path) -ne $snapshot.files.$kind.sha256){throw "Frozen $kind evidence changed."}}
$pattern='(?i)(\.(cpp|h|hpp|inl|cs|uasset|umap|ini|uproject|uplugin)$|(^|/)(Config|Content)/)'
$frozen=@{}
foreach($file in @($manifest.files | Where-Object path -Match $pattern)){
    if($frozen.ContainsKey($file.path)){throw 'Duplicate manifest input.'}
    if((Hash (Inside $root $file.path)) -ne $file.sha256 -or (Hash (Inside $manifest.archive $file.path)) -ne $file.sha256){throw "Changed source/archive bytes: $($file.path)"}
    $frozen[$file.path]=$file.sha256
}
$changed=@(& git -C $root diff HEAD --name-only); if($LASTEXITCODE -ne 0){throw 'Cannot inspect tracked changes.'}
$new=@(& git -C $root ls-files --others --exclude-standard); if($LASTEXITCODE -ne 0){throw 'Cannot inspect untracked changes.'}
$current=@($changed+$new | Where-Object {$_ -match $pattern} | Sort-Object -Unique)
if((@($frozen.Keys | Sort-Object) -join "`n") -ne ($current -join "`n")){throw 'Current execution inputs differ from frozen manifest inventory.'}
$native=@($manifest.files | Where-Object path -Match '\.(cpp|h|cs|uproject|uplugin)$')
$identityText=$head+"`n"+(($native | Sort-Object path | ForEach-Object {$_.path+':'+$_.sha256}) -join "`n")
$hasher=[Security.Cryptography.SHA256]::Create()
try{$nativeIdentity=[Convert]::ToHexString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($identityText)))}finally{$hasher.Dispose()}
$compiles=[Collections.Generic.List[object]]::new()
foreach($target in @('LyraEditor','LyraGame')){
    $refs=@($package.compileRecords | Where-Object target -eq $target)
    if($refs.Count -ne 1 -or (Hash $refs[0].path) -ne $refs[0].sha256){throw "Missing/changed bound $target compile record."}
    $compile=Get-Content -LiteralPath $refs[0].path -Raw | ConvertFrom-Json
    if($compile.status -ne 'passed' -or $compile.exitCode -ne 0 -or !$compile.PSObject.Properties['failureSignals'] -or @($compile.failureSignals).Count -or
       $compile.target -ne $target -or $compile.sourceHead -ne $head -or $compile.nativeSourceIdentity -ne $nativeIdentity -or
       (Hash $compile.nativeSourceManifest) -ne $compile.nativeSourceManifestSha256 -or (Hash $compile.binaryPath) -ne $compile.binarySha256){throw "Invalid bound $target compile."}
    $compiles.Add([pscustomobject]@{target=$target;path=$refs[0].path;sha256=$refs[0].sha256;binarySha256=$compile.binarySha256})
}
$configuration=(Resolve-Path -LiteralPath $ConfigurationPath).Path
$config=Get-Content -LiteralPath $configuration -Raw | ConvertFrom-Json
if($config.schema -ne 'allkit-cooldown-config-1' -or $config.status -ne 'validated-authored-config' -or $config.candidate -ne $Candidate -or
   $config.effectiveSourceIdentity -ne $manifest.effectiveSourceIdentity -or $config.sourceManifestSHA256 -ne $package.sourceManifestSha256 -or
   $config.packageResultSHA256 -ne (Hash $packagePath) -or [datetime]$config.createdUtc -lt [datetime]$package.finishedUtc -or
   $config.abilityCount -ne 16 -or @($config.abilities).Count -ne 16 -or @($config.abilities.case | Sort-Object -Unique).Count -ne 16 -or
   $config.upgradeRank -ne 1 -or $config.cooldownReduction -le 0 -or $config.cooldownReduction -ge 0.5 -or @($config.assets).Count -ne 5){throw 'Require a fresh matching authored configuration export.'}
$expectedAssets=@('Plugins/GameFeatures/BreakawayCore/Content/Economy/DA_BW_MatchUpgrades.uasset',
    'Plugins/GameFeatures/Heroes/Spartacus/Content/Kit/DA_BW_ArgusKitConfig.uasset',
    'Plugins/GameFeatures/Heroes/Rawlins/Content/Kit/DA_BW_RawlinsKitConfig.uasset',
    'Plugins/GameFeatures/Heroes/Alona/Content/Kit/DA_BW_AlonaKitConfig.uasset',
    'Plugins/GameFeatures/Heroes/Morgan/Content/Kit/DA_BW_KorrynKitConfig.uasset')
if((@($config.assets.path | Sort-Object) -join "`n") -ne (@($expectedAssets | Sort-Object) -join "`n") -or
   (@($config.abilities.case | Sort-Object {[int]$_}) -join ',') -ne ((0..15) -join ',')){throw 'Configuration export asset/case inventory differs.'}
# A changed-file archive does not contain unchanged source. Explicitly bind the
# unchanged fixture and every exported asset to their canonical HEAD blob (LFS
# filters included), alongside the actual working-byte SHA256.
$required=@('Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/Development/BwayAllKitCooldownProbe.cpp',
            'Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Development/BwayAllKitCooldownProbe.h')+@($config.assets.path)
$bindings=[Collections.Generic.List[object]]::new()
foreach($relative in $required){
    $path=Inside $root $relative; $sha=Hash $path
    $asset=@($config.assets | Where-Object path -eq $relative)
    if($asset.Count -and ($asset.Count -ne 1 -or $asset[0].sha256 -ne $sha)){throw "Configuration asset changed: $relative"}
    $blob=$null; $basis='changed-manifest'
    if(!$frozen.ContainsKey($relative)){
        $basis='HEAD-blob'; $blob=(& git -C $root rev-parse "${head}:$relative").Trim()
        if($LASTEXITCODE -ne 0){throw "Unbound unchanged input: $relative"}
        $workingBlob=(& git -C $root hash-object "--path=$relative" $path).Trim()
        if($LASTEXITCODE -ne 0 -or $workingBlob -ne $blob){throw "Unchanged input differs from HEAD: $relative"}
    }
    $bindings.Add([pscustomobject]@{path=$relative;sha256=$sha;basis=$basis;headBlob=$blob})
}
$candidateRoot=Inside $root "Saved/StagedBuilds/$Candidate"
$artifactManifest=if($package.hashManifest){$package.hashManifest}else{$package.manifest}
$artifacts=@(Import-Csv -LiteralPath $artifactManifest)
if(!$artifacts.Count -or $artifacts.Count -ne $package.artifactCount){throw 'Artifact manifest count mismatch.'}
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($entry in $artifacts){
    $path=Inside $candidateRoot $entry.path
    if(!$seen.Add($path)){throw 'Duplicate artifact path.'}
    $item=Get-Item -LiteralPath $path
    if($item.Length -ne [long]$entry.bytes -or (Hash $path) -ne $entry.sha256){throw "Packaged artifact changed: $($entry.path)"}
    $node=$item.Directory
    if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Artifact is a reparse point.'}
    while($node -and $node.FullName.StartsWith($candidateRoot,[StringComparison]::OrdinalIgnoreCase)){
        if($node.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Artifact directory is a reparse point.'}; $node=$node.Parent
    }
}
$exe=Inside $candidateRoot 'Windows/ProjectB/Binaries/Win64/LyraGame.exe'
$exeHash=Hash $exe
$exeRows=@($artifacts | Where-Object {($_.path -replace '\\','/') -eq 'Windows/ProjectB/Binaries/Win64/LyraGame.exe'})
$game=@($compiles | Where-Object target -eq 'LyraGame')[0]
if($exeRows.Count -ne 1 -or $exeRows[0].sha256 -ne $exeHash -or $exeHash -ne $ExpectedExeSHA256 -or $exeHash -ne $package.executableSha256 -or $exeHash -ne $game.binarySha256){throw 'Compiled/staged/expected executable mismatch.'}
[pscustomobject]@{schema='allkit-cooldown-provenance-1';passed=$true;candidate=$Candidate;fileCount=$artifacts.Count;stagedHash=$exeHash;compiledHash=$game.binarySha256;
    changedBuildInputs=@();sourceHead=$head;effectiveSourceIdentity=$manifest.effectiveSourceIdentity;nativeSourceIdentity=$nativeIdentity;bindings=@($bindings);compileRecords=@($compiles);
    executable=$exe;packageResult=$packagePath;packageResultSHA256=(Hash $packagePath);sourceManifest=$manifestPath;sourceManifestSHA256=(Hash $manifestPath);
    artifactManifest=$artifactManifest;artifactManifestSHA256=(Hash $artifactManifest);configuration=$configuration;configurationSHA256=(Hash $configuration);
    guardSHA256=(Hash $PSCommandPath);verifiedUtc=[datetime]::UtcNow.ToString('o')}
