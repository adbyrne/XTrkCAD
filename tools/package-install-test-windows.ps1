# Install-test the XTrackCAD Windows ZIP package on a clean system.
#
# Usage: pwsh tools/package-install-test-windows.ps1 <package.zip>
#
# The Windows counterpart of tools/package-install-test.sh; release.yml's
# install-test-windows job runs it on a fresh windows runner. A user installs
# the ZIP by unpacking it anywhere, so the script unpacks it into a directory
# whose name has a space in it, runs it with only the Windows system
# directories on PATH (so no MSYS2, Git or Strawberry Perl DLL can stand in
# for one the package forgot), and checks:
#   - exactly one bin\xtrkcad-<version>.exe, and every DLL it loads is in
#     the package (a missing one shows as STATUS_DLL_NOT_FOUND)
#   - -V prints the version in the package file name
#   - share\<binary name> holds params, demos, examples and the help HTML
#   - the binary plays demos from the unpacked data
#   - running it writes nothing into the install directory, which may be
#     read-only (Program Files)
#
# Hard failures exit 1. Warnings are printed as GitHub Actions ::warning::
# annotations and don't change the exit status.

param(
	[Parameter(Mandatory = $true)][string]$Package
)

$ErrorActionPreference = 'Stop'
$script:Failed = $false

function Fail([string]$msg) { Write-Output "::error::$msg"; $script:Failed = $true }
function Warn([string]$msg) { Write-Output "::warning::$msg" }
function Step([string]$msg) { Write-Output ''; Write-Output "=== $msg" }

# Runs the binary with a timeout: a modal dialog (bad option, missing
# file) would otherwise hang the job. Returns the exit code, or $null on
# timeout. GUI-subsystem programs only see stdout when it's redirected.
function Invoke-XTrkCAD([string[]]$ArgList, [int]$TimeoutSec, [string]$OutFile) {
	$p = Start-Process -FilePath $script:Exe -ArgumentList $ArgList `
		-PassThru -NoNewWindow `
		-RedirectStandardOutput $OutFile -RedirectStandardError "$OutFile.err"
	# Without a cached handle, ExitCode reads as $null after exit.
	$null = $p.Handle
	if (-not $p.WaitForExit($TimeoutSec * 1000)) {
		$p.Kill($true)
		return $null
	}
	return $p.ExitCode
}

function Show-Exit($code) {
	if ($null -eq $code) { return 'timed out' }
	# 0xC0000135 STATUS_DLL_NOT_FOUND, 0xC0000139 entry point not found
	switch ($code) {
		-1073741515 { return "exit 0xC0000135: a DLL is missing" }
		-1073741511 { return "exit 0xC0000139: a DLL lacks an entry point" }
		default { return "exit $code" }
	}
}

$Package = (Resolve-Path $Package).Path
$Work = Join-Path ([IO.Path]::GetTempPath()) "xtrkcad-install-test-$PID"
$Root = Join-Path $Work 'XTrackCAD install test'
New-Item -ItemType Directory -Force $Root | Out-Null

Step "Unpack $(Split-Path -Leaf $Package)"
Expand-Archive -Path $Package -DestinationPath $Root
Write-Output $Root

Step 'Binary'
$exes = @(Get-ChildItem (Join-Path $Root 'bin') -Filter 'xtrkcad-*.exe' -ErrorAction SilentlyContinue)
if ($exes.Count -ne 1) {
	Fail "expected one bin\xtrkcad-*.exe, found $($exes.Count)"
	exit 1
}
$script:Exe = $exes[0].FullName
$Name = $exes[0].BaseName
Write-Output $script:Exe

# Only Windows' own directories, as on a user's machine.
$env:PATH = @("$env:SystemRoot\System32", $env:SystemRoot,
	"$env:SystemRoot\System32\Wbem") -join ';'

$Before = @(Get-ChildItem -Recurse -File $Root | ForEach-Object FullName)

# xtrkcad-<version>-windows-x64.zip
$Want = ''
if ((Split-Path -Leaf $Package) -match '^xtrkcad-(.+)-windows-[a-z0-9]+\.zip$') {
	$Want = $Matches[1]
}
$out = Join-Path $Work 'version.txt'
$rc = Invoke-XTrkCAD @('-V') 60 $out
$Got = ''
if (Test-Path $out) {
	$line = Select-String -Path $out -Pattern '^Version: (.+)$' | Select-Object -First 1
	if ($line) { $Got = $line.Matches[0].Groups[1].Value.Trim() }
}
Write-Output "-V: $Got (package file says $Want), $(Show-Exit $rc)"
if ($rc -ne 0) { Fail "$Name -V failed: $(Show-Exit $rc)" }
elseif (-not $Got) { Fail "$Name -V printed no version" }
# The binary's version carries a build hash the ZIP name may not have
# (5.4.0.<hash> vs 5.4.0); compare the part the file name has.
elseif ($Want -and $Got -ne $Want -and -not $Got.StartsWith("$Want.")) {
	Fail "-V reports $Got, package file name says $Want"
}

Step 'Data directory'
$Share = Join-Path $Root "share\$Name"
Write-Output $Share
foreach ($d in 'params', 'demos', 'examples', 'html') {
	$n = @(Get-ChildItem -Recurse -File (Join-Path $Share $d) -ErrorAction SilentlyContinue).Count
	Write-Output "${d}: $n files"
	if ($n -eq 0) { Fail "$Share\$d is missing or empty" }
}
foreach ($f in 'xtrkcad.xtq', 'html\index.html') {
	if (-not (Test-Path (Join-Path $Share $f))) { Fail "$Share\$f is missing" }
}

Step 'Play demos from the unpacked data'
# dmstrtrk carries REGRESSION checks; dmintro exercises a full demo with
# dialogs. On Windows the log goes to a file (lprintf.c), so name one.
$log = Join-Path $Work 'xtclog.txt'
$out = Join-Path $Work 'demo.txt'
$rc = Invoke-XTrkCAD @('-l', "`"$log`"", '-Tdmintro,dmstrtrk') 600 $out
Write-Output "demo run: $(Show-Exit $rc)"
foreach ($f in $out, "$out.err", $log) {
	if (Test-Path $f) {
		Write-Output "--- $(Split-Path -Leaf $f) (last 20 lines)"
		Get-Content $f -Tail 20
	}
}
if ($rc -ne 0) { Fail "demo run failed: $(Show-Exit $rc)" }
if (-not ((Test-Path $log) -and (Select-String -Quiet -Path $log -Pattern '^Regression Tests Pass'))) {
	Fail "demo run did not report 'Regression Tests Pass'"
}

Step 'Install directory left unchanged'
$New = @(Get-ChildItem -Recurse -File $Root | ForEach-Object FullName |
	Where-Object { $Before -notcontains $_ })
if ($New.Count -gt 0) {
	Warn "running $Name wrote into its install directory, which is read-only under Program Files: $($New -join ', ')"
}

Write-Output ''
Remove-Item -Recurse -Force $Work -ErrorAction SilentlyContinue
if ($script:Failed) {
	Write-Output 'FAILED'
	exit 1
}
Write-Output 'PASSED'
