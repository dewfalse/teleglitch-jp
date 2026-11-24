$ErrorActionPreference='Stop'
$modRoot=Split-Path -Parent $PSScriptRoot
$sourcePath=Join-Path $modRoot 'narrative_ja.tsv'
$outputPath=Join-Path $modRoot 'narrative_messages.tsv'
$utf8=[Text.UTF8Encoding]::new($false)
$output=[Collections.Generic.List[string]]::new()
$output.Add('# Generated from narrative_ja.tsv by tools/build_narratives.ps1. UTF-8.')
$seen=@{}
$maxRows=0
$maxId=''
$rowsById=@{}
foreach($line in [IO.File]::ReadAllLines($sourcePath,$utf8)) {
    if($line.StartsWith('#') -or $line.Length -eq 0){continue}
    $fields=$line.Split([char]9,3)
    if($fields.Count -ne 3 -or $seen.ContainsKey($fields[0])){throw 'Invalid or duplicate translation ID'}
    $seen[$fields[0]]=$true
    $rows=[Collections.Generic.List[string]]::new()
    foreach($paragraph in [regex]::Split($fields[2],'\\n')) {
        $current=''
        $units=0
        foreach($ch in $paragraph.ToCharArray()) {
            $advance=if([int]$ch -lt 128){1}else{2}
            if($units+$advance -gt 66) {
                # Keep closing punctuation with its preceding Japanese phrase.
                $closing=[string][char]0x3001+[char]0x3002+[char]0xFF01+[char]0xFF1F+[char]0xFF09+[char]0x300D+[char]0x300F+[char]0x3011
                if($closing.Contains([string]$ch) -and $units+$advance -le 68){
                    $current+=$ch;$units+=$advance;continue
                }
                $rows.Add($current)
                $current='';$units=0
            }
            $current+=$ch;$units+=$advance
        }
        $rows.Add($current)
    }
    $rowsById[$fields[0]]=$rows.Count
    if($rows.Count -gt $maxRows){$maxRows=$rows.Count;$maxId=$fields[0]}
    $output.Add($fields[0]+"`t"+$fields[1]+"`t"+($rows -join '\n'))
}
[IO.File]::WriteAllLines($outputPath,$output,$utf8)
Write-Output ('Generated {0} records; largest={1} ({2} Japanese rows).' -f $seen.Count,$maxId,$maxRows)
$rowsById.GetEnumerator() | Where-Object Value -gt 21 | Sort-Object Value -Descending | ForEach-Object {Write-Output ('Long text: {0} = {1} rows' -f $_.Key,$_.Value)}
