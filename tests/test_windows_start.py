"""PowerShell launcher contracts with a fake Python process; no installer runs."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
pwsh = os.environ.get('SMW_TEST_PWSH') or shutil.which('pwsh')
if not pwsh:raise SystemExit('Set SMW_TEST_PWSH to PowerShell 7 to run these contract checks.')
with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    fake = tmp / 'fake python.exe'
    fake.write_text('#!/usr/bin/env python3\nimport json,os,sys\nfrom pathlib import Path\nPath(os.environ["SMW_TEST_ARGS"]).write_text(json.dumps(sys.argv[1:]))\nprint("fake builder output")\nsys.exit(17)\n')
    fake.chmod(0o700)
    env = os.environ.copy()
    env.update(SMW_TEST_EXE=str(fake), SMW_TEST_ARGS=str(tmp/'args.json'),
               SMW_TEST_ROOT=str(ROOT), SMW_TEST_TEMP=str(tmp))
    harness = tmp / 'contract.ps1'
    harness.write_text(r'''
$ErrorActionPreference = 'Stop'
. (Join-Path $env:SMW_TEST_ROOT 'tools/Start_Windows.ps1')
$env:LOCALAPPDATA = $env:SMW_TEST_TEMP
$script:installed = $true
$script:installCalls = 0
function Find-SmwPython {
    param([string]$PrivateHome)
    if ($script:installed) { return [pscustomobject]@{ Exe=$env:SMW_TEST_EXE; Prefix=@('-3') } }
    return $null
}
function Install-SmwPython {
    param([string]$PrivateHome, [string]$Cache)
    $script:installCalls = $script:installCalls + 1
    $script:installed = $true
}
$code = Start-SmwBuilder -SelectedRom 'C:\ROM files\SMW USA.sfc'
if ($code -ne 17 -or $script:installCalls -ne 0) { throw 'Existing Python/exit propagation failed' }
$script:installed = $false
$code = Start-SmwBuilder -SelectedRom ''
if ($code -ne 17 -or $script:installCalls -ne 1) { throw 'Missing Python/setup/GUI flow failed' }
$script:installed = $false
function Install-SmwPython { throw 'setup cancelled fixture' }
try { Start-SmwBuilder -SelectedRom ''; throw 'Cancellation was ignored' }
catch { if ($_.Exception.Message -ne 'setup cancelled fixture') { throw } }
Write-Host 'PASS: existing/missing Python, GUI routing, spaced ROM paths, builder exit code and cancelled setup (dependencies mocked)'
''')
    result = subprocess.run([pwsh, '-NoLogo', '-NoProfile', '-File', str(harness)], env=env, text=True, capture_output=True)
    print(result.stdout.strip())
    if result.returncode:print(result.stderr);raise SystemExit(result.returncode)
    args = json.loads((tmp/'args.json').read_text())
    assert args[0]=='-3' and args[1].endswith('tools/build_from_rom.py')
    assert args[2:]==['--backend','portable','--gui']
