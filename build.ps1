# Сборка портативным тулчейном из .tools (clang из llvm-mingw, CMake, Ninja).
# Если тулчейна нет, используется то, что есть в PATH (например, MSVC из Developer PowerShell).
param([string]$Config = 'Release')
$ErrorActionPreference = 'Stop'

$tools = Join-Path $PSScriptRoot '.tools'
if (Test-Path $tools) {
    $llvm  = Get-ChildItem $tools -Directory -Filter 'llvm-mingw-*' | Select-Object -First 1
    $cmake = Get-ChildItem $tools -Directory -Filter 'cmake-*' | Select-Object -First 1
    $env:PATH = "$($llvm.FullName)\bin;$($cmake.FullName)\bin;$tools;$env:PATH"
    $compiler = @('-DCMAKE_CXX_COMPILER=clang++')
} else {
    $compiler = @()
}

$build = Join-Path $PSScriptRoot 'build'
cmake -S $PSScriptRoot -B $build -G Ninja "-DCMAKE_BUILD_TYPE=$Config" @compiler
if ($LASTEXITCODE) { exit $LASTEXITCODE }
cmake --build $build
exit $LASTEXITCODE
