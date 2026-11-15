param([string]$AnalysisRoot = 'D:\mgsv-analysis\mgsv-analysis')
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Set-Location -LiteralPath $repoRoot
$toolRoot = Join-Path $repoRoot '.build-tools'
$env:PATH = "$toolRoot\cmake\cmake-4.1.1-windows-x86_64\bin;$toolRoot\gcc\mingw64\bin;$toolRoot\go\go\bin;$env:PATH"
$env:CMAKE_POLICY_VERSION_MINIMUM = '3.5'
$env:GOPATH = Join-Path $toolRoot 'gopath'
$env:GOCACHE = Join-Path $toolRoot 'gocache'
$env:CGO_ENABLED = '0'
function Run-Test($program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "$program failed: $LASTEXITCODE" }
}
New-Item -ItemType Directory -Force -Path tests/bin | Out-Null
if (-not (Test-Path -LiteralPath tests/bin/lua51.exe)) {
    $luaSources = Get-ChildItem -LiteralPath (Join-Path $toolRoot 'lua/lua-5.1.5/src') -Filter '*.c' |
        Where-Object { $_.Name -ne 'luac.c' -and $_.Name -ne 'print.c' } | ForEach-Object { $_.FullName }
    Run-Test 'gcc' (@('-O2', '-static', '-DLUA_USE_WINDOWS', '-o', 'tests/bin/lua51.exe') + $luaSources)
}
Run-Test 'cmake' @('-S', 'hook', '-B', 'hook/cmake-build-release-docker-mingw15', '-G', 'MinGW Makefiles', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=gcc', '-DCMAKE_CXX_COMPILER=g++', '-DDYNAMITE_BUILD_TESTS=ON', '-DFETCHCONTENT_UPDATES_DISCONNECTED=ON')
Run-Test 'cmake' @('--build', 'hook/cmake-build-release-docker-mingw15', '--target', 'sync_tests', 'session_notify_tests', 'receive_demux_tests', 'dinput8', '-j', '3')
Run-Test './hook/cmake-build-release-docker-mingw15/receive_demux_tests.exe' @()
Run-Test './hook/cmake-build-release-docker-mingw15/session_notify_tests.exe' @()
Run-Test './hook/cmake-build-release-docker-mingw15/sync_tests.exe' @()
rg --files moddata -g '*.lua' | Set-Content -LiteralPath tests/bin/lua-files.txt
Run-Test './tests/bin/lua51.exe' @('tests/syntax.lua', 'tests/bin/lua-files.txt')
Run-Test './tests/bin/lua51.exe' @('tests/gameplay.lua', ($AnalysisRoot -replace '\\', '/'))
Run-Test './tests/bin/lua51.exe' @('tests/buddies.lua')
Run-Test './tests/bin/lua51.exe' @('tests/mission13.lua')
Run-Test './tests/bin/lua51.exe' @('tests/bootstrap.lua')
Run-Test './tests/bin/lua51.exe' @('tests/player_visibility.lua')
Run-Test 'go' @('generate', './...')
Run-Test 'go' @('test', './...')
