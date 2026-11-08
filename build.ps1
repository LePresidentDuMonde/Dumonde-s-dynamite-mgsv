# Native Windows build: hook dll + launcher exe. Tools are downloaded into %LOCALAPPDATA%\dynamite-build-tools
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = 'Tls12'

$root = $PSScriptRoot
# GCC breaks when installed in a path with spaces, so tools live outside the project
$tools = Join-Path $env:LOCALAPPDATA 'dynamite-build-tools'
$log = Join-Path $root 'build.log'
$hookBuild = Join-Path $root 'hook\cmake-build-release-docker-mingw15'
$goUrl = 'https://go.dev/dl/go1.24.5.windows-amd64.zip'

Set-Content $log "build started $(Get-Date)"

function Say($msg, $color = 'White') {
    Write-Host $msg -ForegroundColor $color
    Add-Content $log $msg
}

function Step($n, $msg) { Say "`n[$n/4] $msg" Cyan }

# runs a native command, shows and logs its output, stops on failure
function Run($exe) {
    $ErrorActionPreference = 'Continue'
    & $exe @args 2>&1 | ForEach-Object {
        $line = "$_"
        if ($line -eq 'System.Management.Automation.RemoteException') { $line = '' } # empty stderr line
        Say $line Gray
    }
    if ($LASTEXITCODE -ne 0) { throw "$exe failed with exit code $LASTEXITCODE" }
}

function Get-Zip($name, $url) {
    $dir = Join-Path $tools $name
    if (Test-Path $dir) { Say "$name already downloaded"; return $dir }
    New-Item -ItemType Directory -Force $tools | Out-Null
    $zip = "$dir.zip"
    Say "Downloading $name from $url (can take a few minutes)..."
    Invoke-WebRequest $url -OutFile $zip
    Say "Extracting $name..."
    New-Item -ItemType Directory $dir | Out-Null
    Run tar -xf $zip -C $dir
    Remove-Item $zip
    return $dir
}

try {
    Step 1 'Checking required programs'
    foreach ($p in 'cmake', 'git') {
        if (-not (Get-Command $p -ErrorAction SilentlyContinue)) { throw "$p is not installed or not in PATH" }
        Say "$p found"
    }
    if ($tools -match ' ') { throw "Tools path contains a space, GCC cannot work there: $tools" }

    # reuse tools downloaded by the previous version of this script
    $oldTools = Join-Path $root 'tools'
    if ((Test-Path $oldTools) -and -not (Test-Path $tools)) {
        Say "Moving downloaded tools to $tools"
        Move-Item $oldTools $tools
    }

    Step 2 'Getting compiler (GCC) and Go'
    $gccDir = Join-Path $tools 'gcc'
    if (-not (Test-Path $gccDir)) {
        $releases = Invoke-RestMethod 'https://api.github.com/repos/brechtsanders/winlibs_mingw/releases?per_page=20'
        $asset = $releases.assets |
            Where-Object { $_.name -match '^winlibs-x86_64-posix-seh-gcc-.*-mingw-w64ucrt-.*\.zip$' -and $_.name -notmatch 'llvm' } |
            Select-Object -First 1
        if (-not $asset) { throw 'Could not find a WinLibs GCC download' }
        $gccDir = Get-Zip 'gcc' $asset.browser_download_url
    }
    $goDir = Get-Zip 'go' $goUrl

    $env:PATH = "$gccDir\mingw64\bin;$goDir\go\bin;$env:PATH"
    $env:CMAKE_POLICY_VERSION_MINIMUM = '3.5' # bundled CMake 4 rejects old dependency projects otherwise
    $env:GOPATH = Join-Path $tools 'gopath'
    $env:GOCACHE = Join-Path $tools 'gocache'
    Run g++ --version
    Run go version

    Step 3 'Building mod (dinput8.dll)'
    Run cmake --fresh -S (Join-Path $root 'hook') -B $hookBuild -G 'MinGW Makefiles' `
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
    # each compiler process needs up to ~1.5 GB, more parallel jobs than free memory allows fail
    $freeGB = (Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1MB
    $jobs = [math]::Max(1, [math]::Min([int]$env:NUMBER_OF_PROCESSORS, [math]::Floor($freeGB / 1.5)))
    Say "Compiling with $jobs parallel jobs ($([math]::Round($freeGB, 1)) GB free memory)"
    Run cmake --build $hookBuild -j $jobs

    Step 4 'Building launcher (dynamite-windows-amd64.exe)'
    Push-Location $root
    try {
        $env:CGO_ENABLED = '0'
        $env:GOOS = 'windows'
        $env:GOARCH = 'amd64'
        Run go generate
        Run go build -trimpath '-ldflags=-w -s' -o dynamite-windows-amd64.exe
    } finally {
        Pop-Location
    }

    Say "`nDONE: $(Join-Path $root 'dynamite-windows-amd64.exe')" Green
    Say 'Copy it into your MGS_TPP game folder, run it and click "Save and reinstall".' Green
    exit 0
} catch {
    Say "`nBUILD FAILED: $_" Red
    Say "Send build.log to get help: $log" Red
    exit 1
}
