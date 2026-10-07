# Read-only HY2 environment inventory. No driver load, elevation, tests or system changes.
# PowerShell 5.1+. Run in the independent Windows reference workspace.
# With no OutputDirectory, writes JSON to stdout; otherwise creates a new uniquely named file.
[CmdletBinding()]
param([string] $OutputDirectory)

$ErrorActionPreference = 'Stop'
$inventoryErrors = [System.Collections.Generic.List[object]]::new()

function Protect-LocalPath([string] $Value) {
    if ($env:USERPROFILE) { return $Value.Replace($env:USERPROFILE, '<USERPROFILE>') }
    return $Value
}

function Record-InventoryError([string] $Stage, $Failure) {
    $inventoryErrors.Add([ordered]@{
        stage = $Stage
        message = Protect-LocalPath ([string]$Failure.Exception.Message)
    })
}

$os = $null
try {
    $nativeOS = Get-CimInstance Win32_OperatingSystem
    $os = [ordered]@{
        caption = $nativeOS.Caption
        version = $nativeOS.Version
        build = $nativeOS.BuildNumber
        architecture = $nativeOS.OSArchitecture
        is_64_bit_process = [Environment]::Is64BitProcess
    }
} catch { Record-InventoryError 'operating_system' $_ }

$kitVersions = [System.Collections.Generic.List[object]]::new()
$kitRootRegistered = $null
try {
    # The 64-bit view also works when invoked from a 32-bit PowerShell host.
    $machine = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        [Microsoft.Win32.RegistryHive]::LocalMachine, [Microsoft.Win32.RegistryView]::Registry64)
    try {
        $roots = $machine.OpenSubKey('SOFTWARE\Microsoft\Windows Kits\Installed Roots')
        try { $kitRoot = if ($roots) { [string]$roots.GetValue('KitsRoot10') } else { $null } }
        finally { if ($roots) { $roots.Dispose() } }
    } finally { $machine.Dispose() }
    $kitRootRegistered = [bool]$kitRoot
    if ($kitRoot -and (Test-Path -LiteralPath (Join-Path $kitRoot 'Include') -PathType Container)) {
        foreach ($directory in Get-ChildItem -LiteralPath (Join-Path $kitRoot 'Include') -Directory) {
            if ($directory.Name -notmatch '^\d+\.\d+\.\d+\.\d+$') { continue }
            $kitVersions.Add([ordered]@{
                version = $directory.Name
                sdk_windows_header = Test-Path -LiteralPath (Join-Path $directory.FullName 'um\Windows.h') -PathType Leaf
                wdk_kernel_header = Test-Path -LiteralPath (Join-Path $directory.FullName 'km\ntddk.h') -PathType Leaf
            })
        }
    }
} catch { Record-InventoryError 'windows_kits' $_ }

$visualStudio = [System.Collections.Generic.List[object]]::new()
$vswherePresent = $null
try {
    $programFiles32 = ${env:ProgramFiles(x86)}
    if (-not $programFiles32) { $programFiles32 = $env:ProgramFiles }
    $vswhere = Join-Path $programFiles32 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vswherePresent = Test-Path -LiteralPath $vswhere -PathType Leaf
    if ($vswherePresent) {
        $raw = & $vswhere -all -products '*' -prerelease -format json
        if ($LASTEXITCODE -ne 0) { throw 'vswhere did not complete successfully' }
        foreach ($instance in ($raw -join "`n" | ConvertFrom-Json)) {
            $compilerVersions = [System.Collections.Generic.List[string]]::new()
            $msvc = Join-Path $instance.installationPath 'VC\Tools\MSVC'
            if (Test-Path -LiteralPath $msvc -PathType Container) {
                foreach ($version in Get-ChildItem -LiteralPath $msvc -Directory) {
                    if (Test-Path -LiteralPath (Join-Path $version.FullName 'bin\Hostx64\x64\cl.exe') -PathType Leaf) {
                        $compilerVersions.Add($version.Name)
                    }
                }
            }
            $visualStudio.Add([ordered]@{
                name = $instance.displayName
                version = $instance.installationVersion
                complete = $instance.isComplete
                x64_msvc_directories = @($compilerVersions.ToArray())
            })
        }
    }
} catch { Record-InventoryError 'visual_studio' $_ }

$report = [ordered]@{
    schema = 'cider.windows-reference-environment/v1'
    collected_utc = [DateTime]::UtcNow.ToString('o')
    collector_scope = 'OS and installed SDK/WDK headers/MSVC directories; availability only, no compilation or contract result'
    powershell_version = $PSVersionTable.PSVersion.ToString()
    operating_system = $os
    windows_kits_root_registered = $kitRootRegistered
    windows_kits = @($kitVersions.ToArray())
    vswhere_present = $vswherePresent
    visual_studio = @($visualStudio.ToArray())
    errors = @($inventoryErrors.ToArray())
}
$json = $report | ConvertTo-Json -Depth 8
if (-not $OutputDirectory) { $json; return }

$directoryInfo = [System.IO.Directory]::CreateDirectory($OutputDirectory)
$name = 'environment-{0}-{1}.json' -f [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'), [Guid]::NewGuid().ToString('N')
$output = Join-Path $directoryInfo.FullName $name
$stream = [System.IO.File]::Open($output, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write)
try {
    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($json + "`n")
    $stream.Write($bytes, 0, $bytes.Length)
} finally { $stream.Dispose() }
Write-Output (Protect-LocalPath $output)
