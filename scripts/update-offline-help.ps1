# Rebuilds the offline help under docs/help from the EddyFlow documentation.
# Run from the repo root: pwsh scripts/update-offline-help.ps1
#
# The pages have to be built with --no-directory-urls. With directory URLs a
# page links its neighbour as ../<name>/, and no browser resolves a directory
# over file://, so every link in the bundle would be dead. Without them a page
# is topics/<name>.html, which is what WidgetUtils::localHelpUrl() looks for
# when "Use Offline Help" is on.
#
# Re-run this whenever the documentation changes: the bundle is a copy, and
# tests/test_help_links_static.py fails once the GUI links a page it lacks.

param(
    [string]$RepoRoot = "$PSScriptRoot\..",
    [string]$DocsRepo = "$PSScriptRoot\..\..\eddyflow-documentation",
    [string]$Python = "python"
)

$ErrorActionPreference = "Stop"

function Resolve-Dir {
    param([string]$path, [string]$what)

    if (-not (Test-Path -LiteralPath $path -PathType Container)) {
        throw "$what not found: $path"
    }
    return (Resolve-Path -LiteralPath $path).Path
}

$RepoRoot = Resolve-Dir $RepoRoot "repository root"
$DocsRepo = Resolve-Dir $DocsRepo "documentation checkout"

$config = Join-Path $DocsRepo "mkdocs.yml"
if (-not (Test-Path -LiteralPath $config -PathType Leaf)) {
    throw "no mkdocs.yml in $DocsRepo - is that the documentation checkout?"
}

# --- the build environment lives in the documentation checkout, gitignored ---
$venv = Join-Path $DocsRepo ".venv"
$venvPython = Join-Path $venv "Scripts\python.exe"

if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
    Write-Host "Creating $venv"
    & $Python -m venv $venv
    if ($LASTEXITCODE -ne 0) {
        throw "could not create the virtual environment with '$Python'. Pass -Python <path to python.exe>."
    }
}

Write-Host "Installing the documentation requirements"
& $venvPython -m pip install --quiet --upgrade pip
& $venvPython -m pip install --quiet -r (Join-Path $DocsRepo "requirements-docs.txt")
if ($LASTEXITCODE -ne 0) {
    throw "pip install failed - this step needs network access"
}

# --- build into a temporary folder, so a failure leaves docs/help intact ---
$staging = Join-Path ([System.IO.Path]::GetTempPath()) ("eddyflow-help-" + [System.Guid]::NewGuid().ToString("N"))

Write-Host "Building the documentation"
Push-Location $DocsRepo
try {
    & $venvPython -m mkdocs build --strict --no-directory-urls --site-dir $staging
    if ($LASTEXITCODE -ne 0) {
        throw "mkdocs build failed"
    }
}
finally {
    Pop-Location
}

$index = Join-Path $staging "index.html"
if (-not (Test-Path -LiteralPath $index -PathType Leaf)) {
    throw "the build produced no index.html"
}

# a page still linking a directory means the flag did not take effect
$sample = Join-Path $staging "topics\planar-fit.html"
if (-not (Test-Path -LiteralPath $sample -PathType Leaf)) {
    throw "the build used directory URLs: expected topics\planar-fit.html"
}

$help = Join-Path $RepoRoot "docs\help"
if (Test-Path -LiteralPath $help) {
    Remove-Item -LiteralPath $help -Recurse -Force
}
# -Path, not -LiteralPath: the wildcard copies the build's contents rather
# than nesting the staging folder itself inside docs/help
Copy-Item -Path $staging -Destination $help -Recurse -Force
Remove-Item -LiteralPath $staging -Recurse -Force

$pages = (Get-ChildItem -LiteralPath $help -Recurse -Filter "*.html" | Measure-Object).Count
Write-Host "Offline help updated: $pages pages under $help"
Write-Host 'Review and commit docs/help, then check "Use Offline Help" in the app.'
