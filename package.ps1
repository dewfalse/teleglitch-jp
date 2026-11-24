param([Parameter(Mandatory)][string]$Destination)
$ErrorActionPreference = 'Stop'
$files = @(
    'bootstrap.lua', 'layout.lua', 'narratives.lua', 'tutorial.lua',
    'inventory_ja.tsv', 'monsters_ja.tsv', 'narrative_ja.tsv', 'narrative_messages.tsv',
    'opening_ja.txt', 'translations.tsv', 'tutorial_messages.tsv', 'text_settings.ini',
    'teleglitch_jp.dll', 'install.cmd', 'install.ps1', 'uninstall.cmd', 'uninstall.ps1'
)
$source = Join-Path $PSScriptRoot 'teleglitch-jp'
foreach ($file in $files) {
    if (!(Test-Path -LiteralPath (Join-Path $source $file) -PathType Leaf)) {
        throw "Missing $file. Build the MOD before packaging."
    }
}
if (Test-Path -LiteralPath $Destination) { throw 'Choose a new output directory.' }
$output = New-Item -ItemType Directory -Path $Destination
$mod = New-Item -ItemType Directory -Path (Join-Path $output.FullName 'teleglitch-jp')
foreach ($file in $files) { Copy-Item -LiteralPath (Join-Path $source $file) -Destination $mod.FullName }
foreach ($file in 'README-JA.txt','LICENSE.txt','NOTICE.txt') {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $output.FullName
}
