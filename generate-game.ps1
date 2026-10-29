$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$barSource = Join-Path $PSScriptRoot 'source'
$barElf = Join-Path $PSScriptRoot 'source/lib/bar-decomp/build/recomp.elf'
$barRecompiler = Join-Path $PSScriptRoot 'tools/recompiler-build/N64Recomp.exe'
if (!(Test-Path $barElf)) { throw 'Compile source/lib/bar-decomp/build/recomp.elf first (Ubuntu: make recomp).' }
if (!(Test-Path $barRecompiler)) { throw 'Run build-tools.cmd first.' }
Copy-Item -LiteralPath $barElf -Destination (Join-Path $barSource 'elf/recomp.elf')
Push-Location $barSource
try {
    & $barRecompiler 'beetle-adventure-racing-recomp.toml'
    if ($LASTEXITCODE) { throw 'N64Recomp game generation failed.' }
    & wsl -d Ubuntu --cd $barSource -- bash scripts/fix-recompiled.sh
    if ($LASTEXITCODE) { throw 'Generated code verification failed.' }
    & wsl -d Ubuntu --cd $barSource -- python3 tools/gen_reference_syms.py elf/recomp.elf syms/beetleadventurerac.us.syms.toml syms/beetleadventurerac.us.datasyms.toml
    if ($LASTEXITCODE) { throw 'Reference symbol generation failed.' }
    & wsl -d Ubuntu --cd (Join-Path $barSource 'patches') -- make CC=clang-18 LD=ld.lld-18
    if ($LASTEXITCODE) { throw 'MIPS patch compilation failed.' }
    & $barRecompiler 'patches.toml'
    if ($LASTEXITCODE) { throw 'Patch recompilation failed.' }
} finally {
    Pop-Location
}
