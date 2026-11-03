$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$cmake=Join-Path $PSScriptRoot 'toolchain\bin\cmake.exe'
$build=Join-Path $PSScriptRoot 'build'
# A moved/extracted kit gets a fresh cache; preserve any old build as a backup.
$marker=Join-Path $build 'kit-location.txt'
if((Test-Path -LiteralPath $build) -and (!(Test-Path -LiteralPath $marker) -or (Get-Content -LiteralPath $marker -Raw).Trim() -ne $PSScriptRoot)) {
 $resolved=[IO.Path]::GetFullPath($build)
 if($resolved -ne (Join-Path $PSScriptRoot 'build')) {throw 'Unexpected build path'}
 Move-Item -LiteralPath $resolved -Destination (Join-Path $PSScriptRoot ('build-backup-'+[guid]::NewGuid().ToString('N')))
}
& $cmake -S . -B build -G 'MinGW Makefiles' '-DCMAKE_BUILD_TYPE=Release' '-DBUILD_TESTING=ON' "-DCMAKE_C_COMPILER=$PSScriptRoot/toolchain/bin/gcc.exe" "-DCMAKE_CXX_COMPILER=$PSScriptRoot/toolchain/bin/g++.exe" "-DCMAKE_MAKE_PROGRAM=$PSScriptRoot/toolchain/bin/mingw32-make.exe"
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
Set-Content -LiteralPath $marker -Value $PSScriptRoot
& $cmake --build build --parallel 4
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
& (Join-Path $PSScriptRoot 'toolchain/bin/ctest.exe') --test-dir build --output-on-failure
exit $LASTEXITCODE
