$ErrorActionPreference='Stop'
$shared=Join-Path $PSScriptRoot 'profiles/shared'
$runtime=Join-Path $PSScriptRoot 'tools/host-build'
if(!(Test-Path -LiteralPath (Join-Path $runtime 'beetle-adventure-racing-recomp.exe'))){throw 'Extraire la version Windows avant de restaurer la sauvegarde.'}
$backup=Join-Path $PSScriptRoot ('backups/profile-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
foreach($name in @('mempak_p0.pak','general.json','graphics.json','input.json','sound.json','controls.json','mods.json')){
    $source=Join-Path $shared $name;$target=Join-Path $runtime $name
    if(!(Test-Path -LiteralPath $source)){continue}
    if(Test-Path -LiteralPath $target){[void][IO.Directory]::CreateDirectory($backup);Copy-Item -LiteralPath $target -Destination (Join-Path $backup $name)}
    Copy-Item -LiteralPath $source -Destination $target -Force
}
Write-Output 'Sauvegarde partagee installee. Les anciens fichiers existants ont ete conserves dans backups/.'
