param(
    [Parameter(Mandatory=$true)][string]$StagingDir,
    [Parameter(Mandatory=$true)][string]$OutputFile
)

$resolvedStaging = (Resolve-Path $StagingDir).Path.TrimEnd('\', '/')
$files = Get-ChildItem -Path $resolvedStaging -File -Recurse

if ($files.Count -eq 0) {
    Write-Error "No files found in staging directory: $StagingDir"
    exit 1
}

$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine('<?xml version="1.0" encoding="utf-8"?>')
[void]$sb.AppendLine('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
[void]$sb.AppendLine('  <Fragment>')
[void]$sb.AppendLine('    <ComponentGroup Id="AppFiles" Directory="INSTALLFOLDER">')

$dirs = @{}
$fileIndex = 0

foreach ($f in $files) {
    $fileIndex++
    $fullPath = $f.FullName
    $relPath = $fullPath.Substring($resolvedStaging.Length).TrimStart('\', '/')
    $dirName = [System.IO.Path]::GetDirectoryName($relPath)
    
    $fileId = "fil_$fileIndex"
    $compId = "cmp_$fileIndex"
    
    $escapedPath = $fullPath.Replace('&', '&amp;').Replace('"', '&quot;').Replace('<', '&lt;').Replace('>', '&gt;')
    
    if ([string]::IsNullOrEmpty($dirName)) {
        [void]$sb.AppendLine("      <Component Id=`"$compId`" Guid=`"*`">")
        [void]$sb.AppendLine("        <File Id=`"$fileId`" Source=`"$escapedPath`" KeyPath=`"yes`" />")
        [void]$sb.AppendLine("      </Component>")
    } else {
        $dirId = "dir_" + ($dirName -replace '[^a-zA-Z0-9_]', '_')
        $dirs[$dirName] = $dirId
        [void]$sb.AppendLine("      <Component Id=`"$compId`" Directory=`"$dirId`" Guid=`"*`">")
        [void]$sb.AppendLine("        <File Id=`"$fileId`" Source=`"$escapedPath`" KeyPath=`"yes`" />")
        [void]$sb.AppendLine("      </Component>")
    }
}

[void]$sb.AppendLine('    </ComponentGroup>')

if ($dirs.Count -gt 0) {
    [void]$sb.AppendLine('    <DirectoryRef Id="INSTALLFOLDER">')
    foreach ($d in $dirs.Keys) {
        $dId = $dirs[$d]
        [void]$sb.AppendLine("      <Directory Id=`"$dId`" Name=`"$d`" />")
    }
    [void]$sb.AppendLine('    </DirectoryRef>')
}

[void]$sb.AppendLine('  </Fragment>')
[void]$sb.AppendLine('</Wix>')

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($OutputFile, $sb.ToString(), $utf8NoBom)
Write-Host "Generated $OutputFile for $($files.Count) file(s)."
