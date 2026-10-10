# Parse sources without executing them or resolving Windows-only cmdlets.
param([Parameter(Mandatory = $true)][string[]]$Paths)
$ErrorActionPreference = 'Stop'
$results = New-Object 'System.Collections.Generic.List[object]'
$failed = $false
foreach ($source in $Paths) {
    $path = (Resolve-Path -LiteralPath $source).ProviderPath
    $tokens = $null
    $parseErrors = $null
    $null = [Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$parseErrors)
    $issues = @($parseErrors | ForEach-Object {
        [ordered]@{
            error_id = $_.ErrorId
            line = $_.Extent.StartLineNumber
            column = $_.Extent.StartColumnNumber
            message = $_.Message
        }
    })
    if ($issues.Count -ne 0) { $failed = $true }
    $results.Add([ordered]@{
        file = [IO.Path]::GetFileName($path)
        source_sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        errors = $issues
    })
}
[ordered]@{
    schema = 'cider.powershell-syntax-check/v1'
    parser_version = $PSVersionTable.PSVersion.ToString()
    scope = 'syntax only; script bodies and Windows cmdlets were not executed'
    files = @($results.ToArray())
    success = (-not $failed)
} | ConvertTo-Json -Depth 6
if ($failed) { exit 1 }
