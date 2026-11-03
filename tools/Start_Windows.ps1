param([string]$RomPath = '')
# Windows PowerShell 5.1-compatible launcher. No Docker/WSL/compiler setup.
$ErrorActionPreference = 'Stop'

function Find-SmwPython {
    param([string]$PrivateHome)
    $candidates = @()
    $privateExe = Join-Path $PrivateHome 'python.exe'
    if (Test-Path -LiteralPath $privateExe -PathType Leaf) {
        $candidates += [pscustomobject]@{ Exe = $privateExe; Prefix = @() }
    }
    $launcher = Get-Command py.exe -ErrorAction SilentlyContinue
    if ($launcher) { $candidates += [pscustomobject]@{ Exe = $launcher.Source; Prefix = @('-3') } }
    $python = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($python -and $python.Source -notmatch '\\WindowsApps\\') {
        $candidates += [pscustomobject]@{ Exe = $python.Source; Prefix = @() }
    }
    foreach ($candidate in $candidates) {
        $prefix = @($candidate.Prefix)
        try {
            & $candidate.Exe @prefix -c 'import sys, tkinter; assert sys.version_info >= (3, 9)' *> $null
            if ($LASTEXITCODE -eq 0) { return $candidate }
        } catch { continue }
    }
    return $null
}

function Install-SmwPython {
    param([string]$PrivateHome, [string]$Cache)
    Add-Type -AssemblyName System.Windows.Forms
    $reply = [System.Windows.Forms.MessageBox]::Show(
        'This builder needs Python 3 with file-dialog support. Download and install official Python for your account? Docker, WSL and a compiler are not needed.',
        'SMW PS2 - first-time setup', 'YesNo', 'Question')
    if ($reply -ne [System.Windows.Forms.DialogResult]::Yes) { throw 'Python setup cancelled.' }
    $architecture = $env:PROCESSOR_ARCHITEW6432
    if (-not $architecture) { $architecture = $env:PROCESSOR_ARCHITECTURE }
    if ($architecture -notin @('AMD64', 'ARM64')) { throw 'This setup launcher supports 64-bit Windows.' }
    $suffix = if ($architecture -eq 'ARM64') { 'arm64' } else { 'amd64' }
    $version = '3.13.17'
    $url = "https://www.python.org/ftp/python/$version/python-$version-$suffix.exe"
    New-Item -ItemType Directory -Force -Path $Cache | Out-Null
    $installer = Join-Path $Cache "python-$version-$suffix.exe"
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    Write-Host 'Downloading official Python setup...'
    $oldProgress = $ProgressPreference
    try {
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $url -OutFile $installer -UseBasicParsing
    } finally { $ProgressPreference = $oldProgress }
    $signature = Get-AuthenticodeSignature -LiteralPath $installer
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch '(^|,\s*)(CN|O)=Python Software Foundation(,|$)') {
        Remove-Item -LiteralPath $installer -Force
        throw 'Python installer signature could not be verified. Setup stopped.'
    }
    Write-Host 'Installing Python for your account...'
    $installArgs = @('/passive', 'InstallAllUsers=0', 'Include_launcher=0', 'Include_tcltk=1',
        'Include_test=0', 'PrependPath=0', ('TargetDir="' + $PrivateHome + '"'))
    $process = Start-Process -FilePath $installer -ArgumentList $installArgs -Wait -PassThru
    if ($process.ExitCode -eq 3010) { throw 'Python setup requested a restart. Restart Windows, then open Build_ISO.cmd again.' }
    if ($process.ExitCode -ne 0) { throw "Python setup returned $($process.ExitCode)." }
}

function Start-SmwBuilder {
    param([string]$SelectedRom)
    $privateHome = Join-Path $env:LOCALAPPDATA 'SMWPS2Builder\Python313'
    $python = Find-SmwPython -PrivateHome $privateHome
    if (-not $python) {
        Install-SmwPython -PrivateHome $privateHome -Cache (Join-Path $env:LOCALAPPDATA 'SMWPS2Builder\Downloads')
        $python = Find-SmwPython -PrivateHome $privateHome
    }
    if (-not $python) { throw 'Python with tkinter was not found after setup.' }
    $builder = Join-Path $PSScriptRoot 'build_from_rom.py'
    $buildArgs = @($python.Prefix) + @($builder, '--backend', 'portable')
    if ($SelectedRom) { $buildArgs += @($SelectedRom) } else { $buildArgs += @('--gui') }
    & $python.Exe @buildArgs | Out-Host
    return $LASTEXITCODE
}

if ($MyInvocation.InvocationName -ne '.') {
    try {
        if ([Environment]::OSVersion.Platform -ne 'Win32NT') { throw 'Use build_from_rom.py on Linux/macOS.' }
        exit (Start-SmwBuilder -SelectedRom $RomPath)
    }
    catch { Write-Host ('Setup/build stopped: ' + $_.Exception.Message) -ForegroundColor Red; exit 1 }
}
