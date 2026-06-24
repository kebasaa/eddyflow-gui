# Generates dark-theme QSS files from the light-theme originals.
# Run from the repo root: pwsh scripts/gen_dark_qss.ps1

param([string]$RepoRoot = $PSScriptRoot + "\..")

function Convert-ToDark {
    param([string]$src)

    # --- Specific multi-part gradient stops (most specific first) ---
    $src = $src -replace 'stop:0 #e5f9ff, stop:0\.25 #f4fcff,(\s+)stop:0\.75 #d4f3ff, stop:1 #aae6ff', 'stop:0 #1e3040, stop:0.25 #222838,$1stop:0.75 #1a2c3c, stop:1 #162030'
    $src = $src -replace 'stop:0 white, stop:1 #D1E4E7',               'stop:0 #252525, stop:1 #2a3a42'
    $src = $src -replace 'stop:0 white, stop:1 #dee6f1',               'stop:0 #252525, stop:1 #1e2a35'
    $src = $src -replace 'stop:0 white, stop:1 #CCCCCC',               'stop:0 #252525, stop:1 #444444'
    $src = $src -replace 'stop:0 white, stop:1 #c6e1f4',               'stop:0 #1e2a40, stop:1 #1a2d48'
    $src = $src -replace 'stop:0 white, stop:1 #d8d8d8',               'stop:0 #252525, stop:1 #333333'
    $src = $src -replace 'stop:0 white, stop:1 #b7b7b7',               'stop:0 #2a2a2a, stop:1 #1e1e1e'
    $src = $src -replace 'stop:0 white, stop:0\.7 #cccccc, stop:1 #aaa','stop:0 #2a2a2a, stop:0.7 #1e1e1e, stop:1 #181818'
    $src = $src -replace 'stop:0 white, stop:1 #CCC',                   'stop:0 #252525, stop:1 #444'
    $src = $src -replace 'stop:0 #fff,\s+stop:1 #cdcdcd',              'stop:0 #2a2a2a, stop:1 #1e1e1e'
    $src = $src -replace 'stop:0 #e3e3e3, stop:0\.6 #bbbbbb, stop:1 #888888', 'stop:0 #3a3a3a, stop:0.6 #2e2e2e, stop:1 #282828'
    $src = $src -replace 'stop:0 #f3f3f4, stop:1 #e7e8e9',             'stop:0 #252525, stop:1 #1e1e1e'
    $src = $src -replace 'stop:0 #eff0f1, stop:1 #ddd',                'stop:0 #252525, stop:1 #1a1a1a'
    $src = $src -replace 'stop:0 #dadbde, stop:1 #f6f7fa',             'stop:0 #252525, stop:1 #1e1e1e'
    $src = $src -replace 'stop:0 #e3e3e3, stop:1 #cdcdcd',             'stop:0 #2a2a2a, stop:1 #222222'
    $src = $src -replace 'stop:0 #e9e9e9, stop:0\.5 #d9d9d9, stop:1 #d9d9d9', 'stop:0 #2d2d2d, stop:0.5 #252525, stop:1 #252525'
    $src = $src -replace 'stop:0 #f4f4f4, stop:1 #d9d9d9',             'stop:0 #2a2a2a, stop:1 #222222'
    $src = $src -replace 'stop:0 #60bbee, stop:1 #2b87f5',             'stop:0 #1e7fd0, stop:1 #0a5cb8'
    $src = $src -replace 'stop: 0 #f2fcfe, stop: 1 #d6f4fd',           'stop: 0 #1a2d35, stop: 1 #0d1e25'
    $src = $src -replace 'stop: 0 #ddf0f7, stop: 1 #96d2e7',           'stop: 0 #1e3a48, stop: 1 #0d2535'
    $src = $src -replace 'stop:0 rgba\(247, 247, 247, 255\), stop:1 rgba\(215, 215, 215, 255\)', 'stop:0 rgba(40, 40, 40, 255), stop:1 rgba(30, 30, 30, 255)'
    $src = $src -replace 'rgba\(172, 172, 172, 255\)',                  'rgba(80, 80, 80, 255)'
    $src = $src -replace 'rgba\(255, 255, 255, 255\)',                  'rgba(50, 50, 50, 255)'
    $src = $src -replace 'rgba\(235,230,235,([01])\)',                  'rgba(60,60,60,$1)'
    $src = $src -replace 'rgba\(102, 102, 102, 255\)',                  'rgba(140, 140, 140, 255)'

    # --- QTableView row selection (different from global selection-bg) ---
    $src = $src -replace 'selection-background-color: #D1E4E7',         'selection-background-color: #1a4a6e'

    # --- background-color / background (non-gradient, non-button) ---
    $src = $src -replace '(background(?:-color)?\s*:\s*)white\b',        '${1}#1e1e1e'
    $src = $src -replace '(background(?:-color)?\s*:\s*)lightgrey\b',    '${1}#3a3a3a'
    $src = $src -replace '(background(?:-color)?\s*:\s*)#eeeeee\b',      '${1}#252525'
    $src = $src -replace '(background(?:-color)?\s*:\s*)#E2EEF0\b',      '${1}#1e2d32'
    $src = $src -replace '(background(?:-color)?\s*:\s*)#F0F0F0\b',      '${1}#2e2e2e'
    $src = $src -replace '(background(?:-color)?\s*:\s*)#f0f0f0\b',      '${1}#2e2e2e'
    $src = $src -replace '(alternate-background-color\s*:\s*)#e9f8fc\b', '${1}#1e2d32'
    $src = $src -replace '(background-color\s*:\s*)#A6D7F2\b',           '${1}#1a4a6e'

    # --- text color: (context-prefixed replacements) ---
    $src = $src -replace '(color\s*:\s*)black\b',                       '${1}#e8e8e8'
    $src = $src -replace '(color\s*:\s*)#333333\b',                     '${1}#c0c0c0'
    $src = $src -replace '(color\s*:\s*)#333\b',                        '${1}#c0c0c0'
    $src = $src -replace '(color\s*:\s*)#444444\b',                     '${1}#aaaaaa'
    $src = $src -replace '(color\s*:\s*)#444\b',                        '${1}#aaaaaa'
    $src = $src -replace '(color\s*:\s*)#666666\b',                     '${1}#999999'
    $src = $src -replace '(color\s*:\s*)#666\b',                        '${1}#999999'
    $src = $src -replace '(color\s*:\s*)#58595b\b',                     '${1}#a0a0a0'
    $src = $src -replace '(color\s*:\s*)#8f8f8f\b',                     '${1}#888888'
    $src = $src -replace '(color\s*:\s*)#0078a0\b',                     '${1}#1a9bc0'
    $src = $src -replace '(color\s*:\s*)grey\b',                        '${1}#888888'
    $src = $src -replace '(color\s*:\s*)#999\b(?!9)',                   '${1}#777777'
    $src = $src -replace '(color\s*:\s*)#999999\b',                     '${1}#777777'
    $src = $src -replace '(color\s*:\s*)#007FBD\b',                     '${1}#5ab4e5'
    $src = $src -replace '(color\s*:\s*)#0063A8\b',                     '${1}#4a9fd0'

    # --- QGroupBox borders and backgrounds ---
    $src = $src -replace 'border:\s*2px solid rgb\(180, 180, 180\)',    'border: 2px solid rgb(70, 70, 70)'
    $src = $src -replace 'border:\s*2px solid rgb\(180,180,180\)',      'border: 2px solid rgb(70,70,70)'
    $src = $src -replace 'border:\s*2px solid rgb\(75,75,75\)',         'border: 2px solid rgb(100,100,100)'

    # --- Accent / brand label color (global — titles use this) ---
    $src = $src -replace '#007FBD',  '#5ab4e5'
    $src = $src -replace '#6abfdd',  '#2a6a8a'
    $src = $src -replace '#0078a0',  '#1a9bc0'

    # --- QHeaderView ---
    $src = $src -replace '#F0F0F0',  '#2e2e2e'
    $src = $src -replace '#CCCCCC',  '#444444'
    $src = $src -replace '#CCC\b',   '#444'

    # --- QListWidget advSettingsMenu ---
    $src = $src -replace '(background\s*:\s*)lightgrey\b', '${1}#3a3a3a'
    $src = $src -replace '#a6a6a6', '#484848'
    $src = $src -replace '#828282', '#383838'
    $src = $src -replace '#a8a8a8', '#555555'

    # --- QTabWidget#metadataEditorsTabwidget ---
    $src = $src -replace '#E2EEF0', '#1e2d32'
    $src = $src -replace '#B7D8E1', '#2a4a55'

    # --- Miscellaneous specific hex values ---
    $src = $src -replace '#dee6f1',  '#1e2a35'
    $src = $src -replace '#6ac0dd',  '#1a5a70'
    $src = $src -replace '#ddf0f7',  '#1e3a48'
    $src = $src -replace '#96d2e7',  '#0d2535'
    $src = $src -replace '#f2fcfe',  '#1a2d35'
    $src = $src -replace '#d6f4fd',  '#0d1e25'
    $src = $src -replace '#d9d9d9',  '#252525'
    $src = $src -replace '#e9e9e9',  '#2d2d2d'
    $src = $src -replace '#f4f4f4',  '#2a2a2a'

    return $src
}

$root = Resolve-Path $RepoRoot

foreach ($platform in @("win","mac","lin")) {
    $srcFile  = Join-Path $root "css\EddyFlow-$platform.qss"
    $dstFile  = Join-Path $root "css\EddyFlow-$platform-dark.qss"
    $content  = Get-Content $srcFile -Raw -Encoding UTF8
    $dark     = Convert-ToDark $content
    [System.IO.File]::WriteAllText($dstFile, $dark, [System.Text.Encoding]::UTF8)
    Write-Host "Generated: $dstFile"
}
