param([string]$Path)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
if(!$Path){
    Add-Type -AssemblyName System.Windows.Forms
    $picker=New-Object Windows.Forms.OpenFileDialog
    $picker.Title='Choisir votre ROM USA de Beetle Adventure Racing!'
    $picker.Filter='ROM N64 ou ZIP|*.z64;*.zip'
    if($picker.ShowDialog() -ne 'OK'){exit 0}
    $Path=$picker.FileName
}
$expected='e5ab4d226c08d22f68a2edcc48870203e67454b8'
$sha=[Security.Cryptography.SHA1]::Create()
$found=$null
try{
    if([IO.Path]::GetExtension($Path) -ieq '.zip'){
        $archive=[IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $Path).Path)
        try{
            foreach($entry in $archive.Entries){
                if($entry.Length -ne 16777216 -or [IO.Path]::GetExtension($entry.FullName) -ine '.z64'){continue}
                $stream=$entry.Open();$memory=New-Object IO.MemoryStream
                try{$stream.CopyTo($memory);$candidate=$memory.ToArray()}finally{$stream.Dispose();$memory.Dispose()}
                $hash=([BitConverter]::ToString($sha.ComputeHash($candidate))).Replace('-','').ToLowerInvariant()
                if($hash -eq $expected){$found=$candidate;break}
            }
        }finally{$archive.Dispose()}
    }else{
        $file=Get-Item -LiteralPath $Path
        if($file.Length -ne 16777216){throw 'La ROM attendue fait 16 Mio.'}
        $candidate=[IO.File]::ReadAllBytes($file.FullName)
        $hash=([BitConverter]::ToString($sha.ComputeHash($candidate))).Replace('-','').ToLowerInvariant()
        if($hash -eq $expected){$found=$candidate}
    }
    if(!$found){throw 'ROM USA non reconnue : empreinte SHA-1 incorrecte.'}
    $romFolder=Join-Path $PSScriptRoot 'roms';[void][IO.Directory]::CreateDirectory($romFolder)
    [IO.File]::WriteAllBytes((Join-Path $romFolder 'beetle-usa.z64'),$found)
    Write-Output 'ROM locale verifiee et importee. Vous pouvez lancer le jeu.'
}finally{$sha.Dispose()}
