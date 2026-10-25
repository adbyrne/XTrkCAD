# Install-test the XTrackCAD Windows NSIS installer on a clean system.
#
# Usage: pwsh tools/package-install-test-nsis.ps1 <xtrkcad-setup-...exe>
#
# Run as an administrator (the installer installs for all users); on a
# GitHub windows runner the job already is one. release.yml's
# install-test-windows-nsis job runs it. It does what a user does and
# checks the result (Development Issues #49):
#   - installs silently (/S) into the default folder under Program Files
#   - one uninstall entry in the registry, pointing at the install folder
#   - the checks of tools/package-install-test-windows.ps1 on the
#     installed tree: binary, DLLs, /V, data directory, demos
#   - every Start menu shortcut points at a file that exists, and one
#     starts the installed binary
#   - .xtc and .xtce open with the installed binary
#   - installing again over the first install replaces it: still one
#     uninstall entry, one binary (the upgrade path, Development Issues #48)
#   - uninstalling silently removes the files, the uninstall entry, the
#     Start menu folder and the file association
#
# Hard failures exit 1. Warnings are printed as GitHub Actions ::warning::
# annotations and don't change the exit status.

param(
	[Parameter(Mandatory = $true)][string]$Installer,
	# Where to save screenshots of runs that time out (release.yml uploads it)
	[string]$ArtifactDir = ''
)

$ErrorActionPreference = 'Stop'
$script:Failed = $false

# Write-Host, not Write-Output: inside a function, Write-Output becomes
# part of the function's return value instead of reaching the log.
function Fail([string]$msg) { Write-Host "::error::$msg"; $script:Failed = $true }
function Warn([string]$msg) { Write-Host "::warning::$msg" }
function Step([string]$msg) { Write-Host ''; Write-Host "=== $msg" }

$Installer = (Resolve-Path $Installer).Path

# The installer is a 32-bit program, so it writes HKLM\Software through
# the WOW6432Node view; look in both in case that ever changes.
$UninstallRoots = @(
	'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall',
	'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall')

# Our uninstall entries: key XTrkCAD or "XTrkCAD <version>"
# (CPACK_PACKAGE_INSTALL_REGISTRY_KEY).
function Get-UninstallEntries {
	foreach ($r in $UninstallRoots) {
		if (Test-Path $r) {
			Get-ChildItem $r | Where-Object { $_.PSChildName -match '^XTrkCAD( |$)' } |
				ForEach-Object { Get-ItemProperty $_.PSPath }
		}
	}
}

function Get-StartMenuDir([string]$Folder) {
	Join-Path $env:ProgramData "Microsoft\Windows\Start Menu\Programs\$Folder"
}

# The default value of a key under HKEY_CLASSES_ROOT, or $null.
function Get-ClassDefault([string]$Key) {
	$p = "Registry::HKEY_CLASSES_ROOT\$Key"
	if (-not (Test-Path -LiteralPath $p)) { return $null }
	return (Get-Item -LiteralPath $p).GetValue('')
}

# Runs a silent installer or uninstaller and waits for it.
function Invoke-Silent([string]$Exe, [string[]]$ArgList, [int]$TimeoutSec) {
	$p = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru
	$null = $p.Handle
	if (-not $p.WaitForExit($TimeoutSec * 1000)) {
		$p.Kill($true)
		return $null
	}
	return $p.ExitCode
}

# Installs, then checks there's exactly one uninstall entry and returns it.
function Install-Once([string]$What) {
	Step $What
	$rc = Invoke-Silent $Installer @('/S') 600
	if ($rc -ne 0) {
		Fail "installer /S: $(if ($null -eq $rc) { 'timed out' } else { "exit $rc" })"
		return $null
	}
	$entries = @(Get-UninstallEntries)
	if ($entries.Count -ne 1) {
		Fail "expected one XTrkCAD uninstall entry, found $($entries.Count): $(($entries | ForEach-Object PSChildName) -join ', ')"
		if ($entries.Count -eq 0) { return $null }
	}
	$e = $entries[0]
	Write-Host "uninstall entry: $($e.PSChildName), DisplayName '$($e.DisplayName)'"
	Write-Host "UninstallString: $($e.UninstallString)"
	return $e
}

if (@(Get-UninstallEntries).Count -gt 0) {
	Fail 'an XTrkCAD uninstall entry exists before the test; this needs a clean system'
	exit 1
}

$entry = Install-Once "Install $(Split-Path -Leaf $Installer)"
if (-not $entry) { Write-Output 'FAILED'; exit 1 }
$Uninstaller = $entry.UninstallString.Trim('"')
$InstallDir = Split-Path -Parent $Uninstaller
Write-Output "install folder: $InstallDir"
if (-not (Test-Path $Uninstaller)) { Fail "uninstaller $Uninstaller is missing" }
if (-not $InstallDir.StartsWith($env:ProgramFiles + '\')) {
	Fail "installed into $InstallDir, not under $env:ProgramFiles"
}

Step 'Installed tree (tools/package-install-test-windows.ps1)'
$testArgs = @{ Package = $Installer; InstallDir = $InstallDir }
if ($ArtifactDir) { $testArgs.ArtifactDir = $ArtifactDir }
$global:LASTEXITCODE = 0
& "$PSScriptRoot/package-install-test-windows.ps1" @testArgs
if ($LASTEXITCODE -ne 0) { Fail 'checks of the installed tree failed (see above)' }

$exes = @(Get-ChildItem (Join-Path $InstallDir 'bin') -Filter 'xtrkcad*.exe' |
	Where-Object { $_.Name -match '^xtrkcad(-.+)?\.exe$' })
$Exe = if ($exes.Count -eq 1) { $exes[0].FullName } else { '' }

Step 'Start menu'
if (-not $entry.StartMenu) { Fail 'the uninstall entry has no StartMenu value' }
$StartMenu = Get-StartMenuDir $entry.StartMenu
Write-Output $StartMenu
$shell = New-Object -ComObject WScript.Shell
$links = @(Get-ChildItem $StartMenu -Filter '*.lnk' -ErrorAction SilentlyContinue)
if ($links.Count -eq 0) { Fail "no shortcuts in $StartMenu" }
$runsExe = $false
foreach ($l in $links) {
	$lnk = $shell.CreateShortcut($l.FullName)
	Write-Output "$($l.Name) -> $($lnk.TargetPath) $($lnk.Arguments)"
	$target = $lnk.TargetPath
	# The ReadMe/CHANGELOG shortcuts run notepad.exe on a file.
	if ((Split-Path -Leaf $target) -eq 'notepad.exe') { $target = $lnk.Arguments.Trim('"') }
	if (-not (Test-Path $target)) { Fail "shortcut $($l.Name) points at $target, which doesn't exist" }
	if ($Exe -and $lnk.TargetPath -eq $Exe) { $runsExe = $true }
}
if (-not $runsExe) { Fail "no Start menu shortcut starts $Exe" }

Step 'File association'
foreach ($ext in '.xtc', '.xtce') {
	$cls = Get-ClassDefault $ext
	Write-Output "$ext -> $cls"
	if ($cls -ne 'XTrackCAD.Design') { Fail "$ext is associated with '$cls', not XTrackCAD.Design" }
}
$cmd = Get-ClassDefault 'XTrackCAD.Design\shell\open\command'
Write-Output "open command: $cmd"
if (-not $Exe -or -not $cmd -or -not $cmd.StartsWith("$Exe ")) {
	Fail "XTrackCAD.Design opens with '$cmd', not $Exe"
}

# The installer offers to uninstall the previous install first
# (CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL); /S answers yes.
$entry = Install-Once 'Install again over the first install'
if ($entry) {
	if ($entry.UninstallString.Trim('"') -ne $Uninstaller) {
		Fail "second install's uninstaller is $($entry.UninstallString), the first's was $Uninstaller"
	}
	$exes = @(Get-ChildItem (Join-Path $InstallDir 'bin') -Filter 'xtrkcad*.exe' |
		Where-Object { $_.Name -match '^xtrkcad(-.+)?\.exe$' })
	if ($exes.Count -ne 1) { Fail "expected one binary after reinstalling, found $($exes.Count)" }
}

Step 'Uninstall'
# _?= makes the uninstaller run in place and wait, instead of copying
# itself to TEMP and returning at once; it then can't delete itself or
# the folder, so those two are the only things allowed to remain.
$rc = Invoke-Silent $Uninstaller @('/S', "_?=$InstallDir") 600
if ($rc -ne 0) { Fail "uninstaller /S: $(if ($null -eq $rc) { 'timed out' } else { "exit $rc" })" }
$left = @(Get-ChildItem -Recurse -File $InstallDir -ErrorAction SilentlyContinue |
	Where-Object { $_.FullName -ne $Uninstaller })
if ($left.Count -gt 0) {
	Fail "$($left.Count) files left in $InstallDir after uninstalling, e.g. $(($left | Select-Object -First 5 | ForEach-Object FullName) -join ', ')"
}
Remove-Item -Recurse -Force $InstallDir -ErrorAction SilentlyContinue
$entries = @(Get-UninstallEntries)
if ($entries.Count -gt 0) { Fail "uninstall entry still there: $(($entries | ForEach-Object PSChildName) -join ', ')" }
if (Test-Path $StartMenu) { Fail "Start menu folder $StartMenu still there" }
foreach ($ext in '.xtc', '.xtce') {
	$cls = Get-ClassDefault $ext
	if ($cls -eq 'XTrackCAD.Design') { Fail "$ext is still associated with XTrackCAD.Design" }
}
if (Test-Path -LiteralPath 'Registry::HKEY_CLASSES_ROOT\XTrackCAD.Design') {
	Fail 'HKEY_CLASSES_ROOT\XTrackCAD.Design is still there'
}

Write-Output ''
if ($script:Failed) {
	Write-Output 'FAILED'
	exit 1
}
Write-Output 'PASSED'
