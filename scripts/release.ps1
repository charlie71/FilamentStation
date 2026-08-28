<#
.SYNOPSIS
    Cuts a FilamentStation firmware release: bumps the version, builds,
    checks all native test suites, computes the update checksum, and
    (only with -Publish) tags and publishes a GitHub release.

.DESCRIPTION
    Implements the release process documented in docs/release.md
    ("Release") and TASKS.md Phase 13.8:

      1. Validate the working tree is clean and the version looks sane.
      2. Bump config::kApplicationVersion in src/config/AppConfig.h.
      3. Run all four native test suites (native-spoolman-tests,
         native-scale-tests, native-nfc-tests, native-logger-tests).
      4. Build the wt32-s3-wrover-n16r2 firmware and verify 0 warnings.
      5. Copy firmware.bin next to the repo root and generate
         firmware.bin.sha256 (lowercase hex, matching the OTA update
         checker's expected format). Do the same for
         data/bambu-materials/bambu_materials.json ->
         bambu_materials.json/bambu_materials.json.sha256 (TASKS.md
         Nachtrag 2026-08-28: the Bambu material-mapping table, downloaded
         at runtime the same way as the firmware).
      6. Without -Publish: stop here and print the exact commands to
         review/tag/publish by hand -- nothing is committed, tagged, or
         pushed automatically.
         With -Publish: commit the version bump, create annotated tag
         "v<Version>", push both, and create the GitHub release via
         `gh release create` with firmware.bin/firmware.bin.sha256/
         bambu_materials.json/bambu_materials.json.sha256 attached
         (requires `gh auth login` beforehand).

.PARAMETER Version
    New version number, e.g. "0.2.0" or "0.2.0-rc1" (no leading "v" --
    the git tag gets that prefixed automatically). Must match the format
    services::SemVer accepts (X.Y.Z or X.Y.Z-suffix).

.PARAMETER Publish
    Actually commit, tag, push, and create the GitHub release. Omit for a
    dry run that only builds and prepares the artifacts locally.

.PARAMETER SkipTests
    Skip the four native test suites. Only for iterating on this script
    itself -- never use this for a real release.

.EXAMPLE
    ./scripts/release.ps1 -Version 0.2.0
    Dry run: bumps the version locally, builds, verifies tests, produces
    firmware.bin/firmware.bin.sha256, but does not commit/tag/publish.

.EXAMPLE
    ./scripts/release.ps1 -Version 0.2.0 -Publish
    Full release: also commits the version bump, tags v0.2.0, pushes, and
    publishes a GitHub release with the firmware attached.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [switch]$Publish,

    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Fail {
    param([string]$Message)
    Write-Host "FEHLER: $Message" -ForegroundColor Red
    exit 1
}

# Repo root = parent of this script's directory.
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

$AppConfigPath = Join-Path $RepoRoot "src\config\AppConfig.h"
$BuildEnv = "wt32-s3-wrover-n16r2"
$NativeTestEnvs = @(
    "native-spoolman-tests",
    "native-scale-tests",
    "native-nfc-tests",
    "native-logger-tests"
)
$FirmwareSourcePath = Join-Path $RepoRoot ".pio\build\$BuildEnv\firmware.bin"
$FirmwareOutPath = Join-Path $RepoRoot "firmware.bin"
$ChecksumOutPath = Join-Path $RepoRoot "firmware.bin.sha256"
$BambuMaterialsSourcePath = Join-Path $RepoRoot "data\bambu-materials\bambu_materials.json"
$BambuMaterialsOutPath = Join-Path $RepoRoot "bambu_materials.json"
$BambuMaterialsChecksumOutPath = Join-Path $RepoRoot "bambu_materials.json.sha256"
$TagName = "v$Version"

# --- 1. Vorpruefungen -------------------------------------------------

Write-Step "Pruefe Versionsformat"
if ($Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.+-]+)?$') {
    Fail "Ungueltiges Versionsformat '$Version' -- erwartet X.Y.Z oder X.Y.Z-suffix (services::SemVer)."
}
if ($Version -match '-dev') {
    Write-Host "Hinweis: '$Version' enthaelt '-dev' -- das ist fuer einen echten Release unueblich." -ForegroundColor Yellow
}

Write-Step "Pruefe Arbeitsverzeichnis"
$GitStatus = git status --porcelain
if ($LASTEXITCODE -ne 0) {
    Fail "git status fehlgeschlagen -- laeuft dieses Skript in einem Git-Repository?"
}
if ($GitStatus) {
    Write-Host "Nicht committete Aenderungen gefunden:" -ForegroundColor Yellow
    Write-Host $GitStatus
    Fail "Bitte zuerst committen oder stashen, bevor ein Release geschnitten wird."
}

if (-not (Test-Path $AppConfigPath)) {
    Fail "AppConfig.h nicht gefunden unter $AppConfigPath"
}
$CurrentVersionLine = Select-String -Path $AppConfigPath -Pattern 'kApplicationVersion\s*\[\]\s*=\s*"([^"]+)"'
if (-not $CurrentVersionLine) {
    Fail "kApplicationVersion in AppConfig.h nicht gefunden -- Format der Datei geaendert?"
}
$CurrentVersion = $CurrentVersionLine.Matches[0].Groups[1].Value
Write-Host "Aktuelle Version: $CurrentVersion -> Neue Version: $Version"

if ($Publish) {
    Write-Step "Pruefe gh-CLI-Anmeldung"
    gh auth status | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Fail "gh ist nicht angemeldet -- 'gh auth login' zuerst ausfuehren, oder ohne -Publish als Probelauf starten."
    }

    Write-Step "Pruefe, ob Tag $TagName bereits existiert"
    git rev-parse $TagName 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        Fail "Tag $TagName existiert bereits lokal. Anderen Versionsnamen waehlen oder Tag zuerst entfernen."
    }
    $RemoteTag = git ls-remote --tags origin $TagName
    if ($RemoteTag) {
        Fail "Tag $TagName existiert bereits im Remote 'origin'. Anderen Versionsnamen waehlen."
    }
}

# --- 2. Version setzen --------------------------------------------------

Write-Step "Setze kApplicationVersion auf '$Version' in AppConfig.h"
$Content = Get-Content -Path $AppConfigPath -Raw
$NewContent = $Content -replace 'kApplicationVersion\[\]\s*=\s*"[^"]+"', "kApplicationVersion[] = `"$Version`""
if ($NewContent -eq $Content) {
    Fail "Ersetzung der Versionszeile hat nichts geaendert -- Regex/Datei pruefen."
}
Set-Content -Path $AppConfigPath -Value $NewContent -NoNewline -Encoding utf8

# --- 3. Native Tests ------------------------------------------------------

if (-not $SkipTests) {
    foreach ($Env in $NativeTestEnvs) {
        Write-Step "Native Tests: $Env"
        pio test -e $Env
        if ($LASTEXITCODE -ne 0) {
            Fail "Native Tests in Umgebung '$Env' fehlgeschlagen -- Release abgebrochen. Version-Bump in AppConfig.h ist noch nicht committet, per 'git checkout -- src/config/AppConfig.h' verwerfbar."
        }
    }
} else {
    Write-Host "SkipTests gesetzt -- native Tests werden NICHT ausgefuehrt (nur fuer Skript-Entwicklung, nicht fuer echte Releases)." -ForegroundColor Yellow
}

# --- 4. Firmware-Build ------------------------------------------------

Write-Step "Baue Firmware ($BuildEnv)"
$BuildOutput = pio run -e $BuildEnv 2>&1 | Tee-Object -Variable BuildOutputVar
if ($LASTEXITCODE -ne 0) {
    Fail "Firmware-Build fehlgeschlagen -- Release abgebrochen."
}
$WarningLines = $BuildOutputVar | Select-String -Pattern 'warning:'
if ($WarningLines) {
    Write-Host "Compilerwarnungen gefunden:" -ForegroundColor Red
    $WarningLines | ForEach-Object { Write-Host $_.Line }
    Fail "Build hat Warnungen erzeugt -- dieses Projekt verlangt 0 Warnungen. Release abgebrochen."
}
if (-not (Test-Path $FirmwareSourcePath)) {
    Fail "firmware.bin nicht gefunden unter $FirmwareSourcePath -- Build-Ausgabepfad geaendert?"
}

# --- 5. Pruefsumme --------------------------------------------------------

Write-Step "Erzeuge firmware.bin / firmware.bin.sha256"
Copy-Item -Path $FirmwareSourcePath -Destination $FirmwareOutPath -Force
$Hash = (Get-FileHash -Path $FirmwareOutPath -Algorithm SHA256).Hash.ToLower()
Set-Content -Path $ChecksumOutPath -Value $Hash -NoNewline -Encoding ascii
Write-Host "SHA256: $Hash"
Write-Host "Firmware:  $FirmwareOutPath"
Write-Host "Pruefsumme: $ChecksumOutPath"

Write-Step "Erzeuge bambu_materials.json / bambu_materials.json.sha256"
if (-not (Test-Path $BambuMaterialsSourcePath)) {
    Fail "bambu_materials.json nicht gefunden unter $BambuMaterialsSourcePath"
}
Copy-Item -Path $BambuMaterialsSourcePath -Destination $BambuMaterialsOutPath -Force
$BambuMaterialsHash = (Get-FileHash -Path $BambuMaterialsOutPath -Algorithm SHA256).Hash.ToLower()
Set-Content -Path $BambuMaterialsChecksumOutPath -Value $BambuMaterialsHash -NoNewline -Encoding ascii
Write-Host "SHA256: $BambuMaterialsHash"
Write-Host "Material-Mapping: $BambuMaterialsOutPath"
Write-Host "Pruefsumme: $BambuMaterialsChecksumOutPath"

# --- 6. Commit/Tag/Release (nur mit -Publish) -----------------------------

if (-not $Publish) {
    Write-Step "Probelauf abgeschlossen (kein -Publish angegeben)"
    Write-Host "Version, Build und Pruefsumme sind lokal vorbereitet, aber NICHT committet/getaggt/veroeffentlicht."
    Write-Host ""
    Write-Host "Zum Veroeffentlichen manuell oder mit -Publish erneut ausfuehren:"
    Write-Host "  git add `"$AppConfigPath`""
    Write-Host "  git commit -m `"Release $Version`""
    Write-Host "  git tag -a $TagName -m `"Release $Version`""
    Write-Host "  git push origin HEAD --tags"
    Write-Host "  gh release create $TagName `"$FirmwareOutPath`" `"$ChecksumOutPath`" `"$BambuMaterialsOutPath`" `"$BambuMaterialsChecksumOutPath`""
    exit 0
}

Write-Step "Committe Versions-Bump"
git add -- $AppConfigPath
git commit -m "Release $Version"
if ($LASTEXITCODE -ne 0) { Fail "git commit fehlgeschlagen." }

Write-Step "Erzeuge Tag $TagName"
git tag -a $TagName -m "Release $Version"
if ($LASTEXITCODE -ne 0) { Fail "git tag fehlgeschlagen." }

Write-Step "Push Commit + Tag nach origin"
git push origin HEAD --tags
if ($LASTEXITCODE -ne 0) { Fail "git push fehlgeschlagen -- Tag ist lokal bereits gesetzt, ggf. manuell nachziehen oder 'git tag -d $TagName' zum Zuruecksetzen." }

Write-Step "Veroeffentliche GitHub-Release $TagName"
gh release create $TagName $FirmwareOutPath $ChecksumOutPath $BambuMaterialsOutPath $BambuMaterialsChecksumOutPath --title $TagName --generate-notes
if ($LASTEXITCODE -ne 0) { Fail "gh release create fehlgeschlagen -- Commit/Tag/Push sind bereits durch, Release kann manuell mit 'gh release create $TagName $FirmwareOutPath $ChecksumOutPath $BambuMaterialsOutPath $BambuMaterialsChecksumOutPath' nachgeholt werden." }

Write-Step "Fertig"
Write-Host "Release $TagName veroeffentlicht. Geraete finden es automatisch ueber Einstellungen -> Firmware -> Nach Update suchen." -ForegroundColor Green
