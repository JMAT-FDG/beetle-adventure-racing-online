param([switch]$Session)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$run=Join-Path $root ('logs/menu-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
[void][IO.Directory]::CreateDirectory($run)
$info=New-Object Diagnostics.ProcessStartInfo
$info.FileName=Join-Path $root 'tools/host-build/beetle-adventure-racing-recomp.exe'
$info.Arguments='"'+(Join-Path $root 'roms/beetle-usa.z64')+'"'
$info.WorkingDirectory=Split-Path $info.FileName
$info.UseShellExecute=$false;$info.CreateNoWindow=$true
$info.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
foreach($name in @('BAR_NET_HOST_PORT','BAR_NET_TOKEN','BAR_RACE_PLAYERS','BAR_AUTOPLAY')){$info.EnvironmentVariables.Remove($name)}
$info.EnvironmentVariables['BAR_BACKGROUND_TEST']='25'
$info.EnvironmentVariables['BAR_AUTOPLAY']='120:0 30:8000 1000:0'
$info.EnvironmentVariables['BAR_TEST_CONFIG_DIR']=Join-Path $run 'profile'
$info.EnvironmentVariables['BAR_TEST_ONLINE_MENU']='1'
if($Session){$info.EnvironmentVariables['BAR_TEST_ONLINE_SESSION']='1'}else{$info.EnvironmentVariables.Remove('BAR_TEST_ONLINE_SESSION')}
$shot='online-menu-'+[guid]::NewGuid().ToString('N')+'.png'
$info.EnvironmentVariables['BAR_TEST_ONLINE_SHOT']=$shot
$info.EnvironmentVariables['BAR_WINDOW_SIZE']='1280x900'
$process=New-Object Diagnostics.Process;$process.StartInfo=$info;[void]$process.Start()
$stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
if(-not $process.WaitForExit(40000)){$process.Kill();throw 'Hidden menu test timed out'}
[IO.File]::WriteAllText((Join-Path $run 'stdout.log'),$stdout.Result)
[IO.File]::WriteAllText((Join-Path $run 'stderr.log'),$stderr.Result)
$capture=Join-Path $info.WorkingDirectory $shot
if(Test-Path -LiteralPath $capture){Move-Item -LiteralPath $capture -Destination (Join-Path $run 'panel.png')}
Write-Output $run
if($process.ExitCode -ne 0 -or $stderr.Result -notmatch 'PASS: integrated online panel'){Write-Output $stderr.Result;throw 'Integrated menu test failed'}
if($Session -and $stderr.Result -notmatch 'PASS: integrated menu client process exit=0'){Write-Output $stderr.Result;throw 'Integrated menu session failed'}
'PASS: integrated menu test (hidden window)'
