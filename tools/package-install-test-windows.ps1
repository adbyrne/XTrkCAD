# Install-test the XTrackCAD Windows ZIP package on a clean system.
#
# Usage: pwsh tools/package-install-test-windows.ps1 <package.zip>
#        pwsh tools/package-install-test-windows.ps1 <installer.exe> -InstallDir <dir>
#
# With -InstallDir it checks a tree the NSIS installer already installed
# there (tools/package-install-test-nsis.ps1) instead of unpacking a ZIP;
# the package is then only used for the version in its file name.
#
# The Windows counterpart of tools/package-install-test.sh; release.yml's
# install-test-windows job runs it on a fresh windows runner. A user installs
# the ZIP by unpacking it anywhere, so the script unpacks it into a directory
# whose name has a space in it, runs it with only the Windows system
# directories on PATH (so no MSYS2, Git or Strawberry Perl DLL can stand in
# for one the package forgot), and checks:
#   - exactly one bin\xtrkcad-<version>.exe (or bin\xtrkcad.exe), and every DLL it loads is in
#     the package (a missing one shows as STATUS_DLL_NOT_FOUND)
#   - /V prints the version in the package file name (options start with
#     '/' on Windows)
#   - share\<binary name> holds params, demos, examples and the help HTML
#   - the binary plays demos from the unpacked data
#   - Help opens the installed help pages (a fake .html handler records
#     what ShellExecute() gives it; Development Issues #58)
#   - running it writes nothing into the install directory, which may be
#     read-only (Program Files)
#
# Hard failures exit 1. Warnings are printed as GitHub Actions ::warning::
# annotations and don't change the exit status.

param(
	[Parameter(Mandatory = $true)][string]$Package,
	# An installed tree to check instead of unpacking the package
	[string]$InstallDir = '',
	# Where to save screenshots of runs that time out (release.yml uploads it)
	[string]$ArtifactDir = ''
)

$ErrorActionPreference = 'Stop'
$script:Failed = $false

function Fail([string]$msg) { Write-Output "::error::$msg"; $script:Failed = $true }
function Warn([string]$msg) { Write-Output "::warning::$msg" }
function Step([string]$msg) { Write-Output ''; Write-Output "=== $msg" }

# Saves a screenshot of the whole desktop, to show which dialog a hung
# run is waiting on.
function Save-Screenshot([string]$Path) {
	try {
		Add-Type -AssemblyName System.Windows.Forms, System.Drawing
		$b = [System.Windows.Forms.SystemInformation]::VirtualScreen
		$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
		$g = [System.Drawing.Graphics]::FromImage($bmp)
		$g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size)
		$bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
		$g.Dispose(); $bmp.Dispose()
		Write-Output "screenshot: $Path"
	} catch {
		Warn "screenshot failed: $_"
	}
}

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
		$p.Refresh()
		Write-Output "timed out; window title: '$($p.MainWindowTitle)'" | Out-Host
		if ($script:ArtifactDir) {
			$shot = Join-Path $script:ArtifactDir "$(Split-Path -Leaf $OutFile).png"
			Save-Screenshot $shot | Out-Host
		}
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
$script:ArtifactDir = $ArtifactDir
if ($ArtifactDir) { New-Item -ItemType Directory -Force $ArtifactDir | Out-Null }
$Work = Join-Path ([IO.Path]::GetTempPath()) "xtrkcad-install-test-$PID"
New-Item -ItemType Directory -Force $Work | Out-Null
if ($InstallDir) {
	$Root = (Resolve-Path $InstallDir).Path
	Step "Installed tree"
} else {
	$Root = Join-Path $Work 'XTrackCAD install test'
	New-Item -ItemType Directory -Force $Root | Out-Null
	Step "Unpack $(Split-Path -Leaf $Package)"
	Expand-Archive -Path $Package -DestinationPath $Root
}
Write-Output $Root

Step 'Binary'
# xtrkcad-<version>.exe, or plain xtrkcad.exe for an unversioned release
# build (XTRKCAD_VERSIONED_INSTALL).
$exes = @(Get-ChildItem (Join-Path $Root 'bin') -Filter 'xtrkcad*.exe' -ErrorAction SilentlyContinue |
	Where-Object { $_.Name -match '^xtrkcad(-.+)?\.exe$' })
if ($exes.Count -ne 1) {
	Fail "expected one bin\xtrkcad.exe or bin\xtrkcad-*.exe, found $($exes.Count)"
	exit 1
}
$script:Exe = $exes[0].FullName
$Name = $exes[0].BaseName
Write-Output $script:Exe

# Only Windows' own directories, as on a user's machine.
$env:PATH = @("$env:SystemRoot\System32", $env:SystemRoot,
	"$env:SystemRoot\System32\Wbem") -join ';'

$Before = @(Get-ChildItem -Recurse -File $Root | ForEach-Object FullName)

# xtrkcad-<version>-windows-x64.zip, or the installer's
# xtrkcad-setup-<version>-x64.exe; the UCRT64 variants end in -ucrt
# (Development Issues #34)
$Want = ''
$Leaf = Split-Path -Leaf $Package
if ($Leaf -match '^xtrkcad-(.+)-windows-[a-z0-9]+(-ucrt)?\.zip$' -or
	$Leaf -match '^xtrkcad-setup-(.+)-x(64|86)(-ucrt)?\.exe$') {
	$Want = $Matches[1]
}
$out = Join-Path $Work 'version.txt'
# Options start with '/' on Windows (app/wlib/gtk3lib/win32/getopt.c);
# a '-V' would be taken as a layout file name.
$rc = Invoke-XTrkCAD @('/V') 60 $out
$Got = ''
if (Test-Path $out) {
	$line = Select-String -Path $out -Pattern '^Version: (.+)$' | Select-Object -First 1
	if ($line) { $Got = $line.Matches[0].Groups[1].Value.Trim() }
}
Write-Output "/V: $Got (package file says $Want), $(Show-Exit $rc)"
if ($rc -ne 0) { Fail "$Name /V failed: $(Show-Exit $rc)" }
elseif (-not $Got) { Fail "$Name /V printed no version" }
# The binary's version carries a build hash the ZIP name may not have
# (5.4.0.<hash> vs 5.4.0); compare the part the file name has.
elseif ($Want -and $Got -ne $Want -and -not $Got.StartsWith("$Want.")) {
	Fail "/V reports $Got, package file name says $Want"
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

Step 'Play demos from the installed data'
# dmstrtrk carries REGRESSION checks; dmintro exercises a full demo with
# dialogs. On Windows the log goes to a file (lprintf.c), so name one
# with /l.
$log = Join-Path $Work 'xtclog.txt'
$out = Join-Path $Work 'demo.txt'
$rc = Invoke-XTrkCAD @('/l', "`"$log`"", '/Tdmintro,dmstrtrk') 600 $out
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

Step 'Help opens in the browser'
# wHelp() hands file:///<data dir>/html/<topic>.html to ShellExecute(),
# which opens it with the .html file association. A fake browser
# registered for .html under HKCU writes down what it's given; the
# playback script opens help like the Help menu does (Development Issues
# #58). If the fake browser got nothing, the process list shows what
# opened the page instead.
$helpXtr = Join-Path $PSScriptRoot 'package-install-test-help.xtr'
$urls = Join-Path $Work 'help-urls.txt'
$fake = Join-Path $Work 'fake-browser.cmd'
Set-Content -Path $fake -Encoding ascii -Value @(
	'@echo off',
	">>`"$urls`" echo %~1"
)
$cls = 'HKCU:\Software\Classes'
$cmd = "`"$fake`" `"%1`""
# Put back whatever a developer's own machine had for .html afterwards.
$hadHtml = Test-Path "$cls\.html"
$oldHtml = if ($hadHtml) { (Get-Item "$cls\.html").GetValue('') } else { $null }
if (-not $hadHtml) { New-Item "$cls\.html" | Out-Null }
Set-Item "$cls\.html" 'XTrkCADTest.html'
$progIds = @('XTrkCADTest.html')
# A UserChoice default for .html (the runner has Edge's, MSEdgeHTM) wins
# over the .html key above, and Windows won't let us remove it. It only
# names a ProgId, though, and HKCU's open command for a ProgId wins over
# HKLM's. So in CI, point that ProgId at the fake browser too; never on a
# developer's machine.
$choice = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.html'
foreach ($k in 'UserChoice', 'UserChoiceLatest') {
	if (Test-Path "$choice\$k") {
		$id = (Get-ItemProperty "$choice\$k").ProgId
		Write-Output "$k for .html: $id"
		if ($id -and $env:GITHUB_ACTIONS -eq 'true' -and -not (Test-Path "$cls\$id")) {
			$progIds += $id
		}
	}
}
foreach ($id in $progIds) {
	New-Item -Force "$cls\$id\shell\open\command" | Out-Null
	Set-Item "$cls\$id\shell\open\command" $cmd
}
$started = Get-Date
$log = Join-Path $Work 'xtclog-help.txt'
$out = Join-Path $Work 'help.txt'
$rc = Invoke-XTrkCAD @('/l', "`"$log`"", "/T$helpXtr") 300 $out
Write-Output "help run: $(Show-Exit $rc)"
if (Test-Path $log) { Get-Content $log -Tail 10 }
if ($rc -ne 0) { Fail "help run failed: $(Show-Exit $rc)" }
if (-not ((Test-Path $log) -and (Select-String -Quiet -Path $log -Pattern '^Regression Tests Pass'))) {
	Fail "help run did not report 'Regression Tests Pass'"
}
Start-Sleep -Seconds 3	# the browser is started, not waited for
$got = @()
if (Test-Path $urls) { $got = @(Get-Content $urls) }
$topics = @(Select-String -Path $helpXtr -Pattern '^HELP (.+)$' |
	ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() })
foreach ($topic in $topics) {
	$want = [IO.Path]::GetFullPath((Join-Path $Share "html\$topic.html"))
	$hit = $got | Where-Object { $_ -match "[\\/]html[\\/]$topic\.html$" } |
		Select-Object -First 1
	if (-not $hit) {
		Fail "help topic $topic was not opened in the fake browser"
		continue
	}
	Write-Output "${topic}: $hit"
	# A file:// URL or a path; wHelp() writes file:///C:\...\html/<topic>.html
	$path = ($hit -replace '^file:/*', '') -replace '/', '\'
	if ([IO.Path]::GetFullPath($path) -ne $want) {
		Fail "help topic $topic opened $hit, not $want"
	}
}
if ($got.Count -eq 0) {
	# Show what else was started with a help page, e.g. Edge by UserChoice.
	Get-CimInstance Win32_Process |
		Where-Object { $_.CreationDate -ge $started -and $_.CommandLine -match '\.html' } |
		ForEach-Object { Write-Output "started instead: $($_.Name): $($_.CommandLine)" }
}
Get-Process msedge, chrome, firefox -ErrorAction SilentlyContinue | Stop-Process -Force
foreach ($id in $progIds) {
	Remove-Item -Recurse -Force "$cls\$id" -ErrorAction SilentlyContinue
}
if (-not $hadHtml) {
	Remove-Item -Force "$cls\.html" -ErrorAction SilentlyContinue
} elseif ($null -ne $oldHtml) {
	Set-Item "$cls\.html" $oldHtml
} else {
	Remove-ItemProperty "$cls\.html" -Name '(default)' -ErrorAction SilentlyContinue
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
exit 0
