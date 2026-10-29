param(
    [ValidateRange(5,180)][int]$Seconds=90,
    [ValidateRange(2,4)][int]$Players=3,
    [switch]$Network,
    [switch]$ProductionSwitch,
    [string]$Script='250:0 30:8000 1120:0 20:1000 1480:0 20:8000 280:0 20:8000 280:0 20:8000 280:0 20:8000 280:0 20:8000 280:0 20:8000 280:0 20:8000 200:0 20:8000 1200:0'
)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$run=Join-Path $root ('logs/background-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$profile=Join-Path $run 'profile'
New-Item -ItemType Directory -Path $profile -Force | Out-Null
# Seed only a COPY of the controller pak; the test never opens the live save file.
$pak=Join-Path $env:LOCALAPPDATA 'beetle-adventure-racing-recomp/mempak_p0.pak'
if(Test-Path -LiteralPath $pak){Copy-Item -LiteralPath $pak -Destination $profile}
$exe=Join-Path $root 'tools/host-build/beetle-adventure-racing-recomp.exe'
$start=New-Object System.Diagnostics.ProcessStartInfo
$start.FileName=$exe
$start.Arguments='"'+(Join-Path $root 'roms/beetle-usa.z64')+'"'
$start.WorkingDirectory=Split-Path $exe
$start.UseShellExecute=$false
$start.CreateNoWindow=$true
$start.WindowStyle=[System.Diagnostics.ProcessWindowStyle]::Hidden
$start.RedirectStandardOutput=$true
$start.RedirectStandardError=$true
foreach($name in @('BAR_NET_HOST_PORT','BAR_VIEW_ACTIVE','BAR_NET_TOKEN')){$start.EnvironmentVariables.Remove($name)}
$start.EnvironmentVariables['BAR_BACKGROUND_TEST']=[string]$Seconds
$start.EnvironmentVariables['BAR_TEST_CONFIG_DIR']=$profile
$start.EnvironmentVariables['BAR_TEST_RACE_PLAYERS']=[string]$Players
if($ProductionSwitch){
    $start.EnvironmentVariables['BAR_RACE_PLAYERS']=[string]$Players
    $start.EnvironmentVariables.Remove('BAR_TEST_RACE_PLAYERS')
}else{$start.EnvironmentVariables.Remove('BAR_RACE_PLAYERS')}
$start.EnvironmentVariables['BAR_AUTOPLAY']=$Script
$start.EnvironmentVariables['BAR_WINDOW_SIZE']='640x480'
$start.EnvironmentVariables['BAR_NO_AUDIO']='1'
if($Network){
    $rng=[Security.Cryptography.RandomNumberGenerator]::Create();$bytes=New-Object byte[] 16
    $rng.GetBytes($bytes);$rng.Dispose();$key=([BitConverter]::ToString($bytes)).Replace('-','').ToLowerInvariant()
    $start.EnvironmentVariables['BAR_NET_TOKEN']=$key
    $start.EnvironmentVariables['BAR_NET_BIND']='127.0.0.1'
    $start.EnvironmentVariables['BAR_NET_HOST_PORT']='28768'
    $start.EnvironmentVariables['BAR_NET_LOCAL_PLAYERS']='2'
    $start.EnvironmentVariables['BAR_VIEW_LAYOUT']='3'
    $start.EnvironmentVariables['BAR_VIEW_ACTIVE']='1'
    $start.EnvironmentVariables['BAR_VIEW_WIDTH']='320'
    $start.EnvironmentVariables['BAR_VIEW_HEIGHT']='240'
    $start.EnvironmentVariables['BAR_NET_FPS']='15'
}
$shotPrefix='background-'+[guid]::NewGuid().ToString('N')
$shotFrames=@(2000,4000,6000,7500)
$start.EnvironmentVariables['BAR_SHOTS']=($shotFrames | ForEach-Object {"${_}:${shotPrefix}-${_}.png"}) -join ' '
$process=New-Object System.Diagnostics.Process
$process.StartInfo=$start
[void]$process.Start()
$stdout=$process.StandardOutput.ReadToEndAsync()
$stderr=$process.StandardError.ReadToEndAsync()
if($Network){
    $peerStart=New-Object System.Diagnostics.ProcessStartInfo
    $peerStart.FileName=Join-Path $root 'tools/host-build/beetle-remote.exe'
    $peerStart.Arguments='--game-peer-test'
    $peerStart.UseShellExecute=$false;$peerStart.CreateNoWindow=$true
    $peerStart.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
    $peerStart.RedirectStandardOutput=$true;$peerStart.RedirectStandardError=$true
    $peerStart.EnvironmentVariables['BAR_NET_TOKEN']=$key
    $peer=New-Object Diagnostics.Process;$peer.StartInfo=$peerStart;[void]$peer.Start()
    $peerOut=$peer.StandardOutput.ReadToEndAsync();$peerErr=$peer.StandardError.ReadToEndAsync()
}
if(-not $process.WaitForExit(($Seconds+15)*1000)){$process.Kill();throw 'Background test exceeded watchdog timeout'}
[System.IO.File]::WriteAllText((Join-Path $run 'stdout.log'),$stdout.Result)
[System.IO.File]::WriteAllText((Join-Path $run 'stderr.log'),$stderr.Result)
if($Network){
    if(-not $peer.WaitForExit(5000)){$peer.Kill()}
    [IO.File]::WriteAllText((Join-Path $run 'peer.out.log'),$peerOut.Result)
    [IO.File]::WriteAllText((Join-Path $run 'peer.err.log'),$peerErr.Result)
}
foreach($frame in $shotFrames){
    $shot=Join-Path $start.WorkingDirectory "$shotPrefix-$frame.png"
    if(Test-Path -LiteralPath $shot){Move-Item -LiteralPath $shot -Destination (Join-Path $run "frame$frame.png")}
}
$spawnSlots=@([regex]::Matches($stderr.Result,'\[race-test\] spawned slot=(\d+)') | ForEach-Object {[int]$_.Groups[1].Value} | Sort-Object -Unique)
$allSlots=$true
for($i=0;$i -lt $Players;$i++){if($spawnSlots -notcontains $i){$allSlots=$false}}
$updatedSlots=@([regex]::Matches($stderr.Result,'\[race-test\] updated slot=(\d+) ticks=120') | ForEach-Object {[int]$_.Groups[1].Value} | Sort-Object -Unique)
$allUpdated=$true
for($i=0;$i -lt $Players;$i++){if($updatedSlots -notcontains $i){$allUpdated=$false}}
$remoteRead=$stderr.Result -match '\[race-test\] game controller slot=2 buttons=0x8000'
$report=[ordered]@{exitCode=$process.ExitCode;playersRequested=$Players;seconds=$Seconds;folder=$run;raceEntryReturned=($stderr.Result -match '\[race-test\] race entry returned');allRequestedSlotsSpawned=$allSlots;spawnSlots=$spawnSlots;allRequestedSlotsUpdated=$allUpdated;updatedSlots=$updatedSlots;networkTest=[bool]$Network;remoteControllerRead=$remoteRead;spawnMessages=@([regex]::Matches($stderr.Result,'\[race-test\] spawned[^\r\n]*') | ForEach-Object {$_.Value})}
$report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'report.json') -Encoding UTF8
if($Network){Write-Output $peerOut.Result.Trim()}
$report | ConvertTo-Json
if($process.ExitCode -ne 0){exit 1}
# Reaching the timeout is not proof of reaching a race.
if(-not $report.raceEntryReturned){exit 2}
if(-not $report.allRequestedSlotsSpawned){exit 3}
if(-not $report.allRequestedSlotsUpdated){exit 4}
if($Network -and ($peer.ExitCode -ne 0 -or -not $remoteRead)){exit 5}
