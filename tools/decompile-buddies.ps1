param([Parameter(Mandatory=$true)][string]$Targets, [Parameter(Mandatory=$true)][string]$Output,
      [string]$AnalysisRoot='D:\mgsv-analysis\mgsv-analysis')
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$env:JAVA_HOME=Join-Path $repoRoot '.build-tools\jdk\jdk-25.0.5+8'
$env:GHIDRA_HEADLESS_MAXMEM='2048M'
$env:GHIDRA_HEADLESS_JAVA_OPTIONS='-XX:ActiveProcessorCount=2 -Dcpu.core.limit=2'
$headless=Join-Path $AnalysisRoot 'third_party\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat'
$project=Join-Path $AnalysisRoot 'coop-investigation\native\tpp\project'
$arguments=@($project,'Coop_tpp','-process','mgsvtpp.exe','-readOnly','-noanalysis','-max-cpu','2',
    '-scriptPath',(Join-Path $AnalysisRoot 'tools'),'-postScript','TargetedMgsvDecompile.java',
    ([IO.Path]::GetFullPath($Targets)),([IO.Path]::GetFullPath($Output)),'10',
    '-log',([IO.Path]::GetFullPath($Output)+'-headless.log'))
$ErrorActionPreference='Continue'
& $headless @arguments
if ($LASTEXITCODE -ne 0) { throw "Ghidra exited with code $LASTEXITCODE" }
