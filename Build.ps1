param([string]$BuildDirectory = 'build', [switch]$SkipTests)
$ErrorActionPreference='Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    if(-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        throw 'CMake must be installed and available on PATH. See README.md.'
    }
    $configure=@('-S','.', '-B',$BuildDirectory,'-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=ON','-DBUILD_MAP_EDITOR=OFF')
    if(-not (Test-Path (Join-Path $BuildDirectory 'CMakeCache.txt'))) {
        if(Get-Command mingw32-make -ErrorAction SilentlyContinue) {$configure+=@('-G','MinGW Makefiles')}
    }
    & cmake @configure
    if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
    & cmake --build $BuildDirectory --config Release --parallel 4
    if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
    if(-not $SkipTests) {
        & ctest --test-dir $BuildDirectory -C Release --output-on-failure
        if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
    }
} finally { Pop-Location }
