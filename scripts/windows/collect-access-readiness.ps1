# Read-only preparation for unattended access. Does not install software or change protection.
$ErrorActionPreference = 'Stop'
$errors = New-Object 'System.Collections.Generic.List[string]'
$tailscalePath = Join-Path $env:ProgramFiles 'Tailscale\tailscale.exe'
$addresses = @()
if (Test-Path -LiteralPath $tailscalePath -PathType Leaf) {
    # Only our device's overlay address, not the tailnet roster or identities.
    $addresses = @(& $tailscalePath ip -4)
    if ($LASTEXITCODE -ne 0) { $errors.Add('tailscale ip -4 failed'); $addresses = @() }
}
$computer = Get-CimInstance Win32_ComputerSystem
$processors = @(Get-CimInstance Win32_Processor)
$sshService = Get-Service -Name sshd -ErrorAction SilentlyContinue
$sshBinary = Join-Path $env:WINDIR 'System32\OpenSSH\sshd.exe'
$rdpService = Get-Service -Name TermService -ErrorAction SilentlyContinue
$rdpSettings = Get-ItemProperty -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Control\Terminal Server' -ErrorAction SilentlyContinue
$rdpTransport = Get-ItemProperty -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Control\Terminal Server\WinStations\RDP-Tcp' -ErrorAction SilentlyContinue
$tcp = [Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners()
$interestingPorts = @(22, 2222, 3389)
if ($rdpTransport -and $rdpTransport.PortNumber) { $interestingPorts += [int]$rdpTransport.PortNumber }
$listeners = @($tcp | Where-Object { $_.Port -in $interestingPorts } | ForEach-Object { $_.Port } | Sort-Object -Unique)
$report = [ordered]@{
    schema = 'cider.windows-access-readiness/v1'
    collected_utc = [DateTime]::UtcNow.ToString('o')
    scope = 'availability only; no installation, connection or driver execution'
    tailscale_installed = (Test-Path -LiteralPath $tailscalePath -PathType Leaf)
    tailscale_ipv4 = $addresses
    openssh_server_binary_present = (Test-Path -LiteralPath $sshBinary -PathType Leaf)
    sshd_service_status = $(if ($sshService) { [string]$sshService.Status } else { 'NotInstalled' })
    rdp_connections_enabled = $(if ($rdpSettings) { $rdpSettings.fDenyTSConnections -eq 0 } else { $null })
    rdp_service_status = $(if ($rdpService) { [string]$rdpService.Status } else { 'NotInstalled' })
    rdp_port = $(if ($rdpTransport) { $rdpTransport.PortNumber } else { $null })
    relevant_listening_tcp_ports = $listeners
    public_forwarding_verified = $false
    total_physical_memory_bytes = $computer.TotalPhysicalMemory
    hypervisor_present = $computer.HypervisorPresent
    virtualization = @($processors | Select-Object VirtualizationFirmwareEnabled, SecondLevelAddressTranslationExtensions, VMMonitorModeExtensions)
    errors = @($errors.ToArray())
}
$json = $report | ConvertTo-Json -Depth 5
$destination = Join-Path $PSScriptRoot 'access-readiness.json'
[IO.File]::WriteAllText($destination, $json + "`r`n", (New-Object Text.UTF8Encoding $false))
Write-Output $json
Write-Output ('Saved: ' + $destination)
