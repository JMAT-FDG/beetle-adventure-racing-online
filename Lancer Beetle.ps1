param([switch]$ValidateOnly,[switch]$ValidateForm,[switch]$JoinOnly)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$barRoot=$PSScriptRoot
$barRuntime=Join-Path $barRoot 'tools/host-build'
if(Test-Path -LiteralPath (Join-Path $barRoot 'beetle-remote.exe')){$barRuntime=$barRoot}
$barGame=Join-Path $barRuntime 'beetle-adventure-racing-recomp.exe'
$barClient=Join-Path $barRuntime 'beetle-remote.exe'
$barRom=Join-Path $barRoot 'roms/beetle-usa.z64'
if($ValidateOnly) {
    $barRequired=$(if($JoinOnly){@($barClient)}else{@($barGame,$barClient,$barRom)})
    foreach($barFile in $barRequired){if(!(Test-Path -LiteralPath $barFile)){throw "Missing: $barFile"}}
    'Launcher paths valid';exit 0
}
function New-PartyKey {
    $barBytes=New-Object byte[] 16
    $barRandom=[System.Security.Cryptography.RandomNumberGenerator]::Create()
    try{$barRandom.GetBytes($barBytes)}finally{$barRandom.Dispose()}
    ([BitConverter]::ToString($barBytes)).Replace('-','').ToLowerInvariant()
}
[System.Windows.Forms.Application]::EnableVisualStyles()
$barForm=New-Object System.Windows.Forms.Form
$barForm.Text='Beetle Adventure Racing - Multijoueur (prototype)'
$barForm.ClientSize=New-Object System.Drawing.Size(640,610)
$barForm.StartPosition='CenterScreen'
$barForm.Font=New-Object System.Drawing.Font('Segoe UI',10)
$barForm.FormBorderStyle='FixedDialog';$barForm.MaximizeBox=$false
function Add-Label($text,$x,$y,$width=580,$height=25){
    $barLabel=New-Object System.Windows.Forms.Label
    $barLabel.Text=$text;$barLabel.SetBounds($x,$y,$width,$height);$barForm.Controls.Add($barLabel);return $barLabel
}
function Add-Combo($items,$x,$y,$width=380){
    $barCombo=New-Object System.Windows.Forms.ComboBox
    $barCombo.DropDownStyle='DropDownList';$barCombo.SetBounds($x,$y,$width,30)
    foreach($barItem in $items){[void]$barCombo.Items.Add($barItem)}
    $barCombo.SelectedIndex=0;$barForm.Controls.Add($barCombo);return $barCombo
}
[void](Add-Label 'Mode' 20 18 160)
$barMode=Add-Combo @('Local sur plusieurs ecrans','Heberger en ligne','Rejoindre une partie') 210 15
[void](Add-Label "Adresse de l'hote" 20 60 180)
$barAddress=New-Object System.Windows.Forms.TextBox;$barAddress.Text='127.0.0.1';$barAddress.SetBounds(210,57,380,28);$barAddress.Enabled=$false;$barForm.Controls.Add($barAddress)
[void](Add-Label 'Code de la partie' 20 102 180)
$barKey=New-Object System.Windows.Forms.TextBox;$barKey.SetBounds(210,99,300,28);$barKey.Text=New-PartyKey;$barForm.Controls.Add($barKey)
$barCopy=New-Object System.Windows.Forms.Button;$barCopy.Text='Copier';$barCopy.SetBounds(518,98,72,30);$barCopy.Add_Click({[System.Windows.Forms.Clipboard]::SetText($barKey.Text)});$barForm.Controls.Add($barCopy)
[void](Add-Label 'Joueurs sur ce PC' 20 144 180)
$barCount=New-Object System.Windows.Forms.NumericUpDown;$barCount.Minimum=1;$barCount.Maximum=2;$barCount.Value=2;$barCount.SetBounds(210,141,65,28);$barForm.Controls.Add($barCount)
[void](Add-Label '1 : clavier + Xbox ; 2 : clavier J1, Xbox J2' 285 145 330)
[void](Add-Label 'Disposition dans le jeu' 20 186 180)
$barLayout=Add-Combo @('Deux vues haut / bas','Deux vues gauche / droite','Quatre vues') 210 183
[void](Add-Label 'Ecran du joueur 1' 20 228 180)
[void](Add-Label 'Ecran du joueur 2' 20 270 180)
$barScreens=@()
if(Test-Path -LiteralPath $barClient){
    $barDisplayProcessInfo=New-Object System.Diagnostics.ProcessStartInfo
    $barDisplayProcessInfo.FileName=$barClient;$barDisplayProcessInfo.Arguments='--list-displays'
    $barDisplayProcessInfo.UseShellExecute=$false;$barDisplayProcessInfo.CreateNoWindow=$true
    $barDisplayProcessInfo.RedirectStandardOutput=$true;$barDisplayProcessInfo.RedirectStandardError=$true
    $barDisplayProcess=[System.Diagnostics.Process]::Start($barDisplayProcessInfo)
    $barListing=$barDisplayProcess.StandardOutput.ReadToEnd() -split '\r?\n'
    $barDisplayProcess.WaitForExit();$barDisplayProcess.Dispose()
    foreach($barLine in $barListing){
        if($barLine -match '^(\d+): (.+) (\d+)x(\d+) at (-?\d+),(-?\d+)$'){
            $barScreens+= [PSCustomObject]@{id=[int]$Matches[1];width=[int]$Matches[3];height=[int]$Matches[4];label="Ecran $([int]$Matches[1]+1) - $($Matches[3]) x $($Matches[4])"}
        }
    }
}
if(!$barScreens.Count){
    if($ValidateForm){throw 'No SDL displays found'}
    [System.Windows.Forms.MessageBox]::Show('Le client doit etre compile et au moins un ecran doit etre disponible.');exit 1
}
$barDisplay1=Add-Combo @($barScreens|ForEach-Object{$_.label}) 210 225
$barDisplay2=Add-Combo @($barScreens|ForEach-Object{$_.label}) 210 267
if($barScreens.Count -ge 3){$barDisplay1.SelectedIndex=1;$barDisplay2.SelectedIndex=2}
elseif($barScreens.Count -eq 2){$barDisplay2.SelectedIndex=1}
[void](Add-Label 'Rendu par joueur' 20 312 180)
$barResolution=Add-Combo @('Adapter aux moniteurs choisis','1920 x 1080','2560 x 1440','1280 x 720 (connexion plus lente)') 210 309
$barThree=New-Object System.Windows.Forms.CheckBox
$barThree.Text='Course a 3 : deux ici + un ami distant (experimental)'
$barThree.SetBounds(20,350,595,30);$barThree.Enabled=$false;$barForm.Controls.Add($barThree)
[void](Add-Label 'A 3 : rejoindre avant Start Game, puis choisir Single Race. Course complete non validee.' 20 383 600 40)
[void](Add-Label 'F9 : vues individuelles / menus complets. F8 : changer la disposition.' 20 425)
[void](Add-Label 'Course standard : 2 pilotes. Toute la grille humaine reste en developpement.' 20 453)
$barLaunch=New-Object System.Windows.Forms.Button;$barLaunch.Text='Lancer';$barLaunch.SetBounds(210,490,180,38);$barForm.Controls.Add($barLaunch)
$barStatus=Add-Label 'Pret. La ROM est necessaire uniquement sur le PC hote.' 20 547 600 55
$barMode.Add_SelectedIndexChanged({
    $barJoining=$barMode.SelectedIndex -eq 2
    $barAddress.Enabled=$barJoining;$barLayout.Enabled=!$barJoining;$barResolution.Enabled=!$barJoining
    $barCount.Value= $(if($barJoining){1}else{2})
    $barThree.Enabled=$barMode.SelectedIndex -eq 1
    if(!$barThree.Enabled){$barThree.Checked=$false}
})
$barThree.Add_CheckedChanged({if($barThree.Checked){$barCount.Value=2;$barLayout.SelectedIndex=2};$barCount.Enabled=!$barThree.Checked;$barLayout.Enabled=!$barThree.Checked -and $barMode.SelectedIndex -ne 2})
$barCount.Add_ValueChanged({$barDisplay2.Enabled=$barCount.Value -gt 1})
$barLaunch.Add_Click({
    $barOldEnv=@{}
    try{
        $barJoining=$barMode.SelectedIndex -eq 2;$barPlayers=[int]$barCount.Value
        $barRequired=$(if($barJoining){@($barClient)}else{@($barGame,$barClient)})
        foreach($barFile in $barRequired){if(!(Test-Path -LiteralPath $barFile)){throw 'Compiler le projet avant de lancer une partie.'}}
        [void][IO.Directory]::CreateDirectory((Join-Path $barRoot 'logs'))
        if($barKey.Text -notmatch '^[0-9a-fA-F]{32}$'){throw 'Le code doit contenir 32 caracteres hexadecimaux.'}
        if($barJoining -and $barAddress.Text -notmatch '^[a-zA-Z0-9.-]+$'){throw "Adresse de l'hote invalide."}
        $barSelected=@($barScreens[$barDisplay1.SelectedIndex]);if($barPlayers -eq 2){$barSelected+= $barScreens[$barDisplay2.SelectedIndex]}
        if($barPlayers -eq 2 -and $barSelected[0].id -eq $barSelected[1].id){throw 'Choisir deux moniteurs differents.'}
        $barEnv=@{BAR_NET_TOKEN=$barKey.Text;BAR_NET_HOST_PORT='28765';BAR_NET_LOCAL_PLAYERS="$barPlayers";BAR_NET_BIND='0.0.0.0';BAR_VIEW_LAYOUT="$($barLayout.SelectedIndex+1)";BAR_VIEW_ACTIVE='0';BAR_NET_FPS='30';BAR_NET_QUALITY='90';BAR_SEPARATE_KEYBOARD=$(if($barPlayers -eq 2){'1'}else{'0'})}
        $barEnv.BAR_RACE_PLAYERS=$(if($barThree.Checked -and !$barJoining){'3'}else{''})
        if($barMode.SelectedIndex -eq 0){$barEnv.BAR_NET_BIND='127.0.0.1'}
        switch($barResolution.SelectedIndex){
            1{$barWidth=1920;$barHeight=1080}
            2{$barWidth=2560;$barHeight=1440}
            3{$barWidth=1280;$barHeight=720}
            default{$barWidth=[Math]::Min(2560,($barSelected|Measure-Object width -Maximum).Maximum);$barHeight=[Math]::Min(1440,($barSelected|Measure-Object height -Maximum).Maximum)}
        }
        $barEnv.BAR_VIEW_WIDTH="$barWidth";$barEnv.BAR_VIEW_HEIGHT="$barHeight"
        foreach($barName in $barEnv.Keys){$barOldEnv[$barName]=[Environment]::GetEnvironmentVariable($barName,'Process');[Environment]::SetEnvironmentVariable($barName,$barEnv[$barName],'Process')}
        $barStamp=Get-Date -Format 'yyyyMMdd-HHmmss'
        if(!$barJoining){
            if(!(Test-Path -LiteralPath $barRom)){throw 'ROM USA introuvable dans roms/beetle-usa.z64.'}
            [IO.File]::WriteAllText((Join-Path $barRoot 'session-key.txt'),$barKey.Text)
            $barHostProcess=Start-Process -FilePath $barGame -WorkingDirectory $barRuntime -ArgumentList ('"'+$barRom+'"') -PassThru -RedirectStandardOutput (Join-Path $barRoot "logs/host-$barStamp.out.log") -RedirectStandardError (Join-Path $barRoot "logs/host-$barStamp.err.log")
            Start-Sleep -Milliseconds 1500
            if($barHostProcess.HasExited){throw "Le jeu hote n'a pas demarre. Voir le journal host-$barStamp.err.log."}
        }
        $barTarget=$(if($barJoining){$barAddress.Text}else{'127.0.0.1'})
        $barArguments=@('--host',$barTarget,'--port','28765','--players',"$barPlayers",'--displays',(($barSelected|ForEach-Object{$_.id})-join ','))
        if(!$barJoining){$barArguments+='--mirror'}
        Start-Process -FilePath $barClient -WorkingDirectory $barRuntime -ArgumentList $barArguments -RedirectStandardOutput (Join-Path $barRoot "logs/viewer-$barStamp.out.log") -RedirectStandardError (Join-Path $barRoot "logs/viewer-$barStamp.err.log")
        $barStatus.Text='Clique sur Start Game dans la fenetre du jeu, puis joue depuis les vues. Une fois en multijoueur, F9 separe les vues ; F8 ajuste leur disposition.'
        if($barThree.Checked){$barStatus.Text='Donne le code a ton ami. Il doit rejoindre avant Start Game. Choisis Single Race ; trois voitures seront creees. F9 separe les vues en course.'}
    }catch{[System.Windows.Forms.MessageBox]::Show($_.Exception.Message,'Beetle',0,16)|Out-Null}
    finally{foreach($barName in $barOldEnv.Keys){[Environment]::SetEnvironmentVariable($barName,$barOldEnv[$barName],'Process')}}
})
if($JoinOnly){$barMode.SelectedIndex=2;$barMode.Enabled=$false;$barForm.Text='Beetle Adventure Racing - Rejoindre (prototype)'}
if($ValidateForm){
    if($barDisplay1.Items.Count -ne $barScreens.Count -or $barResolution.Items.Count -ne 4){throw 'Invalid launcher controls'}
    if(!$JoinOnly){
        $barMode.SelectedIndex=1;$barThree.Checked=$true
        if($barCount.Value -ne 2 -or $barLayout.SelectedIndex -ne 2 -or $barCount.Enabled){throw 'Invalid three-player host configuration'}
    }
    $barMode.SelectedIndex=2
    if($barThree.Checked -or $barThree.Enabled -or $barCount.Value -ne 1){throw 'Invalid join configuration'}
    "Launcher form valid: $($barScreens.Count) displays, $($barMode.Items.Count) modes"
    $barForm.Dispose();exit 0
}
[void]$barForm.ShowDialog()
