$ErrorActionPreference = 'Stop'
$gameRoot = Split-Path -Parent $PSScriptRoot
$menuPath = Join-Path $gameRoot 'lua\menu.lua'
$backupPath = Join-Path $PSScriptRoot 'backup\menu.lua.original'
$manifestPath = Join-Path $PSScriptRoot 'installation.json'
if (-not (Test-Path -LiteralPath $manifestPath)) { throw 'Installation manifest not found.' }
$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$currentHash = (Get-FileHash -LiteralPath $menuPath -Algorithm SHA256).Hash
if ($currentHash -eq $manifest.original_menu_sha256) {
    Write-Output 'Original menu.lua is already restored.'
    exit 0
}
if ($currentHash -ne $manifest.installed_menu_sha256) {
    throw 'menu.lua was changed after installation. Restore stopped to preserve those changes.'
}
if ((Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash -ne $manifest.original_menu_sha256) {
    throw 'Backup checksum mismatch. Restore stopped.'
}
$pending = Join-Path (Split-Path -Parent $menuPath) ([IO.Path]::GetRandomFileName())
try {
    [IO.File]::Copy($backupPath, $pending)
    [IO.File]::Replace($pending, $menuPath, [System.Management.Automation.Language.NullString]::Value)
} finally {
    if (Test-Path -LiteralPath $pending) { Remove-Item -LiteralPath $pending }
}
Write-Output 'Restored the original menu.lua. Restart Teleglitch to unload the renderer.'
