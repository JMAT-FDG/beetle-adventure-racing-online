$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$exe=Join-Path $root 'tools/host-build/beetle-remote.exe'
$run=Join-Path $root ('logs/network-three-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $run | Out-Null
$random=[System.Security.Cryptography.RandomNumberGenerator]::Create()
$bytes=New-Object byte[] 16
$random.GetBytes($bytes);$random.Dispose()
$key=([BitConverter]::ToString($bytes)).Replace('-','').ToLowerInvariant()
$processes=@()
try {
    foreach($role in @('host','local','peer')) {
        $start=New-Object System.Diagnostics.ProcessStartInfo
        $start.FileName=$exe
        $start.Arguments=if($role -eq 'host'){'--transport-host-test'}elseif($role -eq 'local'){'--transport-local-test'}else{'--transport-peer-test'}
        $start.UseShellExecute=$false;$start.CreateNoWindow=$true
        $start.WindowStyle=[System.Diagnostics.ProcessWindowStyle]::Hidden
        $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
        $start.EnvironmentVariables['BAR_NET_TOKEN']=$key
        $process=New-Object System.Diagnostics.Process
        $process.StartInfo=$start
        [void]$process.Start()
        $processes+=@{Role=$role;Process=$process;Out=$process.StandardOutput.ReadToEndAsync();Err=$process.StandardError.ReadToEndAsync()}
    }
    $ok=$true
    foreach($item in $processes){
        if(-not $item.Process.WaitForExit(16000)){$item.Process.Kill();$ok=$false}
        [IO.File]::WriteAllText((Join-Path $run ($item.Role+'.out.log')),$item.Out.Result)
        [IO.File]::WriteAllText((Join-Path $run ($item.Role+'.err.log')),$item.Err.Result)
        if($item.Process.ExitCode -ne 0){$ok=$false}
        Write-Output ($item.Role+': '+$item.Out.Result.Trim()+' exit='+$item.Process.ExitCode)
    }
    Write-Output ('Logs: '+$run)
    if(-not $ok){exit 1}
} finally {
    foreach($item in $processes){if(-not $item.Process.HasExited){$item.Process.Kill()}}
}
