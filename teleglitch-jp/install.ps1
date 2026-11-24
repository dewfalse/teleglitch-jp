$ErrorActionPreference = 'Stop'
$gameRoot = Split-Path -Parent $PSScriptRoot
$menuPath = Join-Path $gameRoot 'lua\menu.lua'
$backupPath = Join-Path $PSScriptRoot 'backup\menu.lua.original'
$expectedExe = '28DDC3F47A4C65490D82978795C1DB31F81A8F4F4ECDB90F70A3D4E10FBD93B5'
$expectedMenu = 'A6DA24B277F961B20C8A3A0B8D6EB13F12B256DFD79C8C94588C8C3BF4554DC4'
$installedMenu = '25BD7C431C66929098BC415581BD923E8D5BD795F45EB6C0659D54707CB4B9EA'
if ((Get-FileHash -LiteralPath (Join-Path $gameRoot 'Teleglitch.exe') -Algorithm SHA256).Hash -ne $expectedExe) {
    throw 'Unsupported Teleglitch.exe. Installation stopped.'
}
foreach ($file in 'teleglitch_jp.dll','bootstrap.lua','layout.lua','narratives.lua','tutorial.lua','inventory_ja.tsv','monsters_ja.tsv','narrative_ja.tsv','narrative_messages.tsv','opening_ja.txt','translations.tsv','tutorial_messages.tsv','text_settings.ini') {
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $file) -PathType Leaf)) {
        throw "Missing $file. Copy the complete release folder first."
    }
}
$marker = '-- Teleglitch JP prototype loader v0.1'
$ascii = [Text.Encoding]::ASCII
$bytes = [IO.File]::ReadAllBytes($menuPath)
$currentHash = (Get-FileHash -LiteralPath $menuPath -Algorithm SHA256).Hash
if ($currentHash -ne $installedMenu -and $currentHash -ne $expectedMenu) {
    throw 'lua/menu.lua contains other changes. Installation stopped to preserve them.'
}
New-Item -ItemType Directory -Path (Split-Path -Parent $backupPath) -Force | Out-Null
if (Test-Path -LiteralPath $backupPath) {
    if ((Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash -ne $expectedMenu) {
        throw 'Backup checksum mismatch. Installation stopped.'
    }
} else {
    if ($currentHash -eq $installedMenu) { throw 'Existing installation is missing its original menu backup.' }
    [IO.File]::WriteAllBytes($backupPath, $bytes)
}
$manifest = [ordered]@{
    original_menu_sha256 = $expectedMenu
    installed_menu_sha256 = $installedMenu
    exe_sha256 = $expectedExe
    version = '0.3.1'
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'installation.json') -Encoding UTF8
if ($currentHash -eq $installedMenu) {
    Write-Output 'Japanese MOD is already installed.'
    exit 0
}
$loader = $ascii.GetBytes($marker + "`r`ndofile(`"teleglitch-jp/bootstrap.lua`")`r`n")
$pending = Join-Path (Split-Path -Parent $menuPath) ([IO.Path]::GetRandomFileName())
try {
    [IO.File]::WriteAllBytes($pending, [byte[]]($loader + $bytes))
    if ((Get-FileHash -LiteralPath $pending).Hash -ne $installedMenu) { throw 'Patched menu checksum mismatch.' }
    [IO.File]::Replace($pending, $menuPath, [System.Management.Automation.Language.NullString]::Value)
} finally {
    if (Test-Path -LiteralPath $pending) { Remove-Item -LiteralPath $pending }
}
Write-Output 'Installed. Restart Teleglitch to load the Japanese renderer.'
