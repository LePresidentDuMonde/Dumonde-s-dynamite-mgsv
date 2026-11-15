# Portable build dependencies; all writes stay in the repository.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = 'Tls12'
$buildRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\.build-tools'))
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
function Get-Tool($name, $url, $expectedHash) {
    $toolDir = Join-Path $buildRoot $name
    if (Test-Path -LiteralPath (Join-Path $toolDir '.complete')) { return }
    $archive = Join-Path $buildRoot "$name.zip"
    Write-Output "Downloading $name"
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $archive
    if ($expectedHash -and (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
        throw "SHA256 mismatch for $name"
    }
    New-Item -ItemType Directory -Force -Path $toolDir | Out-Null
    & tar -xf $archive -C $toolDir
    if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $name" }
    Set-Content -LiteralPath (Join-Path $toolDir '.complete') -Value $url
}
$gccAsset = Invoke-RestMethod 'https://api.github.com/repos/brechtsanders/winlibs_mingw/releases?per_page=5' |
    ForEach-Object { $_.assets } |
    Where-Object { $_.name -match '^winlibs-x86_64-posix-seh-gcc-.*-mingw-w64ucrt-.*\.zip$' -and $_.name -notmatch 'llvm' } |
    Select-Object -First 1
if (-not $gccAsset) { throw 'WinLibs archive not found' }
Get-Tool 'gcc' $gccAsset.browser_download_url ($gccAsset.digest -replace '^sha256:', '')
Get-Tool 'cmake' 'https://github.com/Kitware/CMake/releases/download/v4.1.1/cmake-4.1.1-windows-x86_64.zip' ''
Get-Tool 'go' 'https://go.dev/dl/go1.24.5.windows-amd64.zip' ''
Get-Tool 'lua' 'https://www.lua.org/ftp/lua-5.1.5.tar.gz' ''
Write-Output "Portable tools ready in $buildRoot"
