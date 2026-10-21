# Offline parser/barrier controls. No runner top-level or process functions execute.
param([Parameter(Mandatory)][string]$OutputJson)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$source=Join-Path $projectRoot 'Saved/run-slide-trajectory.ps1'
$tokens=$null;$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($source,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw ($parseErrors.Message -join '; ')}
foreach($name in @('Add-TrajectoryLine','Assert-TransportCompletion','Assert-SlideArtifacts','Get-SlidePackageIdentity')){
    $function=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$true)|Where-Object Name -CEQ $name)
    if($function.Count -ne 1){throw "Missing unique function $name"}
    . ([scriptblock]::Create($function[0].Extent.Text))
}
$controls=[Collections.Generic.List[object]]::new()
function Reset-Fixture([int]$Cases){
    $script:caseCount=$Cases
    $script:record=[ordered]@{firstCausalError=$null;errors=@();warnings=@();hostReady=$false;hostComplete=$false;completeCount=0}
    $script:records=[Collections.Generic.List[object]]::new()
    foreach($index in 0..3){
        $role=if($index -eq 0){'host'}else{"client$index"}
        $peer=[pscustomobject]@{role=$role;failure=$null;passCount=0;fixturePass=$false;reportedPeer=$null;selectedPlayer=$null;startupConnected=$false}
        $script:records.Add($peer)
        $kind=if($index -eq 0){'Host'}else{'Client'}
        Add-TrajectoryLine $peer 'LogTemp: Display: BwaySlidePackaged: StartupConnected humans=4'
        Add-TrajectoryLine $peer "LogTemp: Display: BwaySlidePackaged: $kind PASS peer=$index player=257 cases=$Cases cleanup=1 physicalInput=0 trajectoryParity=0"
    }
    Add-TrajectoryLine $script:records[0] 'LogNet: Display: GameNetDriver listening on port 7777'
    Add-TrajectoryLine $script:records[0] "LogTemp: Display: BwaySlidePackaged: COMPLETE passed=1 player=257 cases=$Cases peers=4 cleanupAcks=4 finalAcks=4 authorityTransformRestored=1"
}
function Check-Control([string]$Name,[int]$Cases,[scriptblock]$Mutate,[bool]$ExpectedFailure){
    Reset-Fixture $Cases
    & $Mutate
    $failure=$null
    try{Assert-TransportCompletion}catch{$failure=$_.Exception.Message}
    $controls.Add([pscustomobject]@{name=$Name;passed=([bool]$failure -eq $ExpectedFailure);observedError=$failure})
}
Check-Control 'legacy-four-cases' 4 {} $false
Check-Control 'review-two-cases' 2 {} $false
Check-Control 'missing-peer' 2 {$records[3].fixturePass=$false} $true
Check-Control 'duplicate-peer-identity' 2 {$records[3].reportedPeer=1} $true
Check-Control 'wrong-source-player' 2 {$records[2].selectedPlayer=258} $true
Check-Control 'duplicate-native-complete' 2 {Add-TrajectoryLine $records[0] 'LogTemp: Display: BwaySlidePackaged: COMPLETE passed=1 player=257 cases=2 peers=4 cleanupAcks=4 finalAcks=4 authorityTransformRestored=1'} $true
Check-Control 'duplicate-native-pass' 2 {Add-TrajectoryLine $records[1] 'LogTemp: Display: BwaySlidePackaged: Client PASS peer=1 player=257 cases=2 cleanup=1'} $true
Check-Control 'old-case-count-cannot-pass-review' 2 {$records[0].fixturePass=$false;Add-TrajectoryLine $records[0] 'LogTemp: Display: BwaySlidePackaged: Host PASS peer=0 player=257 cases=4 cleanup=1'} $true
$destination=[IO.Path]::GetFullPath($OutputJson)
if(Test-Path -LiteralPath $destination){throw 'Preserve prior controls; select a fresh output.'}
$fixtureDirectory=Join-Path (Split-Path -Parent $destination) ([IO.Path]::GetFileNameWithoutExtension($destination)+'-inputs')
if(Test-Path -LiteralPath $fixtureDirectory){throw 'Preserve prior controls inputs; select a fresh output.'}
$script:root=Join-Path $fixtureDirectory 'source'
$script:stage=Join-Path $fixtureDirectory 'candidate'
$script:exe=Join-Path $stage 'Windows/ProjectB/Binaries/Win64/LyraGame.exe'
$fixtureConfig=Join-Path $stage 'Windows/ProjectB/Config/DefaultEngine.ini'
$artifactManifest=Join-Path $stage 'sha256.csv'
foreach($path in @($exe,$fixtureConfig)){
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))
    [IO.File]::WriteAllText($path,'synthetic artifact bytes only')
}
$artifactRows=@($exe,$fixtureConfig|ForEach-Object {[pscustomobject]@{Path=$_.Substring($stage.Length+1);Bytes=(Get-Item -LiteralPath $_).Length;SHA256=(Get-FileHash -LiteralPath $_).Hash}})
$artifactRows|Export-Csv -LiteralPath $artifactManifest -NoTypeInformation
function Artifact-Control([string]$Name,[object[]]$Rows,[bool]$ExpectedFailure){
    $failure=$null
    try{Assert-SlideArtifacts $Rows $artifactManifest}catch{$failure=$_.Exception.Message}
    $controls.Add([pscustomobject]@{name=$Name;passed=([bool]$failure -eq $ExpectedFailure);observedError=$failure})
}
Artifact-Control 'all-artifact-bytes' $artifactRows $false
Artifact-Control 'config-omitted' @($artifactRows[0]) $true
Artifact-Control 'duplicate-artifact-path' @($artifactRows[0],$artifactRows[1],$artifactRows[0]) $true
$escaped=[pscustomobject]@{Path='../escape';Bytes=0;SHA256='NONE'}
Artifact-Control 'escaped-artifact-path' @($artifactRows[0],$escaped) $true
$oldHash=$artifactRows[1].SHA256;$artifactRows[1].SHA256='BAD'
Artifact-Control 'changed-config-hash' $artifactRows $true
$artifactRows[1].SHA256=$oldHash
# Isolated fake source/compile/package records use real hash/filesystem validation.
# This local Git stub is used only by the AST-extracted package function.
function git {
    param([Parameter(ValueFromRemainingArguments)][object[]]$Arguments)
    $global:LASTEXITCODE=0
    if($Arguments -contains 'rev-parse'){return 'synthetic-head'}
    if($Arguments -contains 'diff'){return @('Source/Fixture.cpp')+$script:extraInput}
    if($Arguments -contains 'ls-files'){return @()}
    throw 'Unexpected synthetic Git operation.'
}
$script:extraInput=@();$script:Candidate='LAN-20261016-01'
$archive=Join-Path $fixtureDirectory 'archive';$sourceRelative='Source/Fixture.cpp'
foreach($base in @($root,$archive)){
    $file=Join-Path $base $sourceRelative
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($file));[IO.File]::WriteAllText($file,'synthetic source bytes')
}
$inputFile=Join-Path $root $sourceRelative
$snapshotPath=Join-Path $fixtureDirectory 'snapshot.json'
$snapshot=[ordered]@{sourceHead='synthetic-head';sourceIdentity='synthetic-snapshot';files=[ordered]@{}}
foreach($kind in @('patch','status','untracked')){
    $path=Join-Path $fixtureDirectory "$kind.txt";[IO.File]::WriteAllText($path,"synthetic-$kind")
    $snapshot.files[$kind]=[ordered]@{path=$path;sha256=(Get-FileHash -LiteralPath $path).Hash}
}
$snapshot|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $snapshotPath -Encoding UTF8
$sourceManifestPath=Join-Path $fixtureDirectory 'source-manifest.json'
$manifest=[ordered]@{sourceHead='synthetic-head';effectiveSourceIdentity='synthetic-effective';archive=$archive;files=@([ordered]@{path=$sourceRelative;bytes=(Get-Item -LiteralPath $inputFile).Length;sha256=(Get-FileHash -LiteralPath $inputFile).Hash})}
$manifest|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $sourceManifestPath -Encoding UTF8
$nativeText=$manifest.sourceHead+"`n"+$sourceRelative+':'+$manifest.files[0].sha256
$hasher=[Security.Cryptography.SHA256]::Create()
try{$nativeIdentity=([BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($nativeText)))).Replace('-','')}finally{$hasher.Dispose()}
$compiled=@(foreach($target in @('LyraGame','LyraEditor')){
    $path=Join-Path $fixtureDirectory "$target.json"
    $original="$path.original"
    $compile=[ordered]@{status='passed';exitCode=0;target=$target;sourceHead='synthetic-head';failureSignals=@()}
    $compile|ConvertTo-Json|Set-Content -LiteralPath $original -Encoding UTF8
    $compile.nativeSourceIdentity=$nativeIdentity;$compile.nativeSourceManifest=$sourceManifestPath;$compile.nativeSourceManifestSha256=(Get-FileHash -LiteralPath $sourceManifestPath).Hash
    $compile.originalCompileRecord=$original;$compile.originalCompileRecordSha256=(Get-FileHash -LiteralPath $original).Hash;$compile.binarySha256=(Get-FileHash -LiteralPath $exe).Hash
    $compile|ConvertTo-Json|Set-Content -LiteralPath $path -Encoding UTF8
    [pscustomobject]@{path=$path;sha256=(Get-FileHash -LiteralPath $path).Hash;target=$target}
})
$packagePath=Join-Path $fixtureDirectory 'package.json'
$package=[ordered]@{status='passed';exitCode=0;candidate=$Candidate;moduleRestored=$true;stage=$stage;sourceManifest=$sourceManifestPath;sourceManifestSha256=(Get-FileHash -LiteralPath $sourceManifestPath).Hash;sourceHead='synthetic-head';effectiveSourceIdentity='synthetic-effective';sourceSnapshot=$snapshotPath;sourceIdentity='synthetic-snapshot';compileRecords=$compiled;hashManifest=$artifactManifest;executableSha256=(Get-FileHash -LiteralPath $exe).Hash}
function Package-Control([string]$Name,[bool]$ExpectedFailure){
    $package|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $packagePath -Encoding UTF8
    $failure=$null
    try{$null=Get-SlidePackageIdentity $packagePath}catch{$failure=$_.Exception.Message}
    $controls.Add([pscustomobject]@{name=$Name;passed=([bool]$failure -eq $ExpectedFailure);observedError=$failure})
}
Package-Control 'new-package-schema' $false
$package.sourceIdentity='wrong';Package-Control 'snapshot-identity-mismatch' $true;$package.sourceIdentity='synthetic-snapshot'
$package.sourceManifestSha256='BAD';Package-Control 'source-manifest-changed' $true;$package.sourceManifestSha256=(Get-FileHash -LiteralPath $sourceManifestPath).Hash
$package.moduleRestored=$false;Package-Control 'cook-module-not-restored' $true;$package.moduleRestored=$true
$script:extraInput=@('Source/NewInput.h');Package-Control 'new-unfrozen-build-input' $true;$script:extraInput=@()
$package.compileRecords=$compiled[0];Package-Control 'missing-compile-target' $true;$package.compileRecords=$compiled
$boundText=[IO.File]::ReadAllText($compiled[0].path)
$changedCompile=$boundText|ConvertFrom-Json;$changedCompile.nativeSourceIdentity='wrong'
$changedCompile|ConvertTo-Json|Set-Content -LiteralPath $compiled[0].path -Encoding UTF8
$compiled[0].sha256=(Get-FileHash -LiteralPath $compiled[0].path).Hash
Package-Control 'compile-source-binding-mismatch' $true
$changedCompile=$boundText|ConvertFrom-Json;$changedCompile.binarySha256='wrong'
$changedCompile|ConvertTo-Json|Set-Content -LiteralPath $compiled[0].path -Encoding UTF8
$compiled[0].sha256=(Get-FileHash -LiteralPath $compiled[0].path).Hash
Package-Control 'compile-output-binding-mismatch' $true
[IO.File]::WriteAllText($compiled[0].path,$boundText)
$compiled[0].sha256=(Get-FileHash -LiteralPath $compiled[0].path).Hash
[IO.File]::WriteAllText((Join-Path $archive $sourceRelative),'changed archived source')
Package-Control 'archived-source-changed' $true
$result=[ordered]@{passed=(@($controls|Where-Object {!$_.passed}).Count -eq 0);count=$controls.Count;scope='Offline runner log/barrier controls only; no process launch or gameplay evidence';controls=@($controls.ToArray())}
[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
$result|ConvertTo-Json -Depth 7|Set-Content -LiteralPath $destination -Encoding UTF8
[pscustomobject]@{passed=$result.passed;count=$result.count;failed=@($controls|Where-Object {!$_.passed});output=$destination}|ConvertTo-Json -Depth 6 -Compress
if(!$result.passed){exit 1}
