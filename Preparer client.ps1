$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$kit=Join-Path $root 'client-online'
$src=Join-Path $kit 'source'
$sdl=Join-Path $root 'tools/host-build/_deps/sdl2-src'
foreach($folder in @($kit,$src,(Join-Path $src 'include'),(Join-Path $src 'lib'),(Join-Path $src 'SDL2'))){[void][IO.Directory]::CreateDirectory($folder)}
foreach($file in @('beetle-remote.exe','SDL2.dll')){Copy-Item -LiteralPath (Join-Path $root "tools/host-build/$file") -Destination $kit -Force}
Copy-Item -LiteralPath (Join-Path $root 'Lancer Beetle.ps1') -Destination $kit -Force
Copy-Item -LiteralPath (Join-Path $root 'source/COPYING') -Destination (Join-Path $kit 'COPYING-AGPL-3.0.txt') -Force
Copy-Item -LiteralPath (Join-Path $sdl 'COPYING.txt') -Destination (Join-Path $kit 'COPYING-SDL2.txt') -Force
foreach($file in @('remote.hpp','remote.cpp','client.cpp')){Copy-Item -LiteralPath (Join-Path $root "source/src/remote/$file") -Destination $src -Force}
foreach($file in @('stb_image.h','stb_image_write.h')){Copy-Item -LiteralPath (Join-Path $root "source/lib/rt64/src/contrib/stb/$file") -Destination (Join-Path $src 'include') -Force}
Copy-Item -LiteralPath (Join-Path $sdl 'include') -Destination (Join-Path $src 'SDL2') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $sdl 'lib/x64/SDL2.lib') -Destination (Join-Path $src 'lib') -Force
Copy-Item -LiteralPath (Join-Path $root 'client-kit/CMakeLists.txt') -Destination $src -Force
Copy-Item -LiteralPath (Join-Path $root 'client-kit/README.txt') -Destination $kit -Force
Copy-Item -LiteralPath (Join-Path $root 'client-kit/Rejoindre.cmd') -Destination $kit -Force
Get-FileHash -LiteralPath (Join-Path $kit 'beetle-remote.exe'),(Join-Path $kit 'SDL2.dll') -Algorithm SHA256 | Select-Object @{n='file';e={Split-Path $_.Path -Leaf}},Hash | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $kit 'checksums.json') -Encoding UTF8
Compress-Archive -Path (Join-Path $kit '*') -DestinationPath (Join-Path $root 'client-online.zip') -Force
Write-Output 'Client prepared: client-online.zip (no ROM, no host game)'
