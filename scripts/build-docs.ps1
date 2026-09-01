<#
.SYNOPSIS
    Builds the Doxygen API documentation and reports how many source
    members are still undocumented.

.DESCRIPTION
    Runs `doxygen Doxyfile` from the repo root and summarizes
    doxygen-warnings.log, which WARN_IF_UNDOCUMENTED/WARN_NO_PARAMDOC in
    the Doxyfile fill with one line per undocumented file/function/
    parameter/member. This count is the completeness signal for the
    Doxygen documentation phases (TASKS.md Phase 15) -- analogous to the
    firmware's own 0-compiler-warnings rule, but for documentation
    coverage instead of code correctness.

    Output goes to doxygen-output/ (gitignored, see .gitignore) --
    open doxygen-output/html/index.html to browse it.

.PARAMETER ShowWarnings
    Print every remaining warning line, not just the count.
#>
[CmdletBinding()]
param(
    [switch]$ShowWarnings
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

if (-not (Get-Command doxygen -ErrorAction SilentlyContinue)) {
    Write-Host "FEHLER: doxygen ist nicht im PATH gefunden." -ForegroundColor Red
    exit 1
}

Write-Host "==> doxygen Doxyfile" -ForegroundColor Cyan
doxygen Doxyfile
if ($LASTEXITCODE -ne 0) {
    Write-Host "FEHLER: doxygen ist fehlgeschlagen (Exit code $LASTEXITCODE)." -ForegroundColor Red
    exit 1
}

$LogPath = Join-Path $RepoRoot "doxygen-warnings.log"
if (-not (Test-Path $LogPath)) {
    Write-Host "Kein doxygen-warnings.log gefunden -- 0 Warnungen." -ForegroundColor Green
    exit 0
}

$Lines = Get-Content -Path $LogPath
$Count = $Lines.Count

Write-Host ""
if ($Count -eq 0) {
    Write-Host "0 Doxygen-Warnungen -- vollstaendig dokumentiert." -ForegroundColor Green
} else {
    Write-Host "$Count Doxygen-Warnung(en) (undokumentierte Member/Parameter/Dateien):" -ForegroundColor Yellow
    if ($ShowWarnings) {
        $Lines | ForEach-Object { Write-Host "  $_" }
    } else {
        Write-Host "  (mit -ShowWarnings die einzelnen Zeilen anzeigen, oder direkt doxygen-warnings.log oeffnen)"
    }
}

Write-Host ""
Write-Host "HTML-Ausgabe: doxygen-output/html/index.html"
