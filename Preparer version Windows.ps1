param([string]$BuildDir=(Join-Path $PSScriptRoot 'tools/host-build'),[string]$OutDir=(Join-Path $PSScriptRoot 'dist'))
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$stage=Join-Path $OutDir ('stage-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$bin=Join-Path $stage 'tools/host-build'
[void][IO.Directory]::CreateDirectory($bin)
foreach($name in @('beetle-adventure-racing-recomp.exe','beetle-remote.exe','SDL2.dll','dxcompiler.dll','dxil.dll')){
    $file=Join-Path $BuildDir $name
    if(!(Test-Path -LiteralPath $file)){throw "Missing runtime: $name"}
    Copy-Item -LiteralPath $file -Destination $bin
}
Copy-Item -LiteralPath (Join-Path $BuildDir 'assets') -Destination (Join-Path $bin 'assets') -Recurse
foreach($name in @('README.md','LICENSE','PROVENANCE.json','Lancer Beetle.cmd','Importer ROM.ps1','Importer ROM.cmd','Installer sauvegarde.ps1','Installer sauvegarde.cmd')){
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $root 'profiles') -Destination (Join-Path $stage 'profiles') -Recurse
# The portable profile contains only the explicitly shared saves/settings, never a cached ROM.
foreach($file in Get-ChildItem -LiteralPath (Join-Path $root 'profiles/shared') -File){Copy-Item -LiteralPath $file.FullName -Destination $bin}
[IO.File]::WriteAllText((Join-Path $bin 'portable.txt'),'Portable shared profile; ROM imported locally.')
Copy-Item -LiteralPath (Join-Path $root 'source/THIRD_PARTY_NOTICES.md') -Destination $stage
$licenseSources=[ordered]@{
    'N64ModernRuntime.txt'='source/lib/N64ModernRuntime/COPYING'
    'N64Recomp.txt'='source/lib/N64Recomp/LICENSE'
    'RT64.txt'='source/lib/rt64/LICENSE'
    'BeetleDecomp.txt'='source/lib/bar-decomp/LICENSE'
    'RmlUi.txt'='source/lib/RecompFrontend/recompui/lib/RmlUi/LICENSE.txt'
    'lunasvg.txt'='source/lib/RecompFrontend/recompui/lib/lunasvg/LICENSE'
    'FreeType.txt'='source/lib/RecompFrontend/recompui/lib/freetype-windows-binaries/LICENSE.TXT'
    'GamepadMotionHelpers.txt'='source/lib/RecompFrontend/lib/GamepadMotionHelpers/LICENSE'
}
$licenses=Join-Path $stage 'licenses';[void][IO.Directory]::CreateDirectory($licenses)
foreach($entry in $licenseSources.GetEnumerator()){
    $path=Join-Path $root $entry.Value
    if(!(Test-Path -LiteralPath $path)){throw "Missing license: $($entry.Value)"}
    Copy-Item -LiteralPath $path -Destination (Join-Path $licenses $entry.Key)
}
Copy-Item -LiteralPath (Join-Path $BuildDir '_deps/sdl2-src/COPYING.txt') -Destination (Join-Path $licenses 'SDL2.txt')
foreach($file in Get-ChildItem -LiteralPath (Join-Path $root 'build-support/licenses') -File){Copy-Item -LiteralPath $file.FullName -Destination $licenses}
$forbidden=@(Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object {$_.Extension -in '.z64','.n64','.v64','.rom','.elf' -or $_.Name -in 'session-key.txt','hosts.yml','.env'})
if($forbidden.Count){throw 'Forbidden ROM/credential found in release stage'}
$zip=Join-Path $OutDir 'Beetle-Online-Windows-x64.zip'
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
$hash=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $OutDir 'SHA256SUMS.txt'),"$hash  Beetle-Online-Windows-x64.zip`n")
Write-Output $zip
Write-Output "SHA256 $hash"
