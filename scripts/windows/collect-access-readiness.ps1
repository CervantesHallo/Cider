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
$accountSource = $null
try {
    # Classify the caller's account without collecting email, password or its SID.
    $callerSid = [Security.Principal.WindowsIdentity]::GetCurrent().User
    $callerAccount = Get-LocalUser -SID $callerSid
    if ($callerAccount.PrincipalSource) { $accountSource = $callerAccount.PrincipalSource.ToString() }
} catch {
    $errors.Add('The current account source could not be read: ' + $_.Exception.Message)
}
$processors = @(Get-CimInstance Win32_Processor)
$sshService = Get-Service -Name sshd -ErrorAction SilentlyContinue
$sshBinary = Join-Path $env:WINDIR 'System32\OpenSSH\sshd.exe'
if ($sshService) {
    # A standalone Microsoft MSI uses Program Files, not the in-box path.
    # Read the actual service image; never evaluate its command line.
    try {
        $sshImage = Get-CimInstance Win32_Service -Filter "Name='sshd'"
        if ($sshImage -and $sshImage.PathName -match '^\s*(?:"([^"]+)"|(\S+))') {
            $candidate = $(if ($Matches[1]) { $Matches[1] } else { $Matches[2] })
            if ($candidate -match '\.exe$' -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
                $sshBinary = $candidate
            } else {
                $sshBinary = $null
                $errors.Add('The sshd service executable path could not be verified.')
            }
        } else {
            $sshBinary = $null
            $errors.Add('The sshd service image was not available.')
        }
    } catch {
        $sshBinary = $null
        $errors.Add('The sshd service image could not be read: ' + $_.Exception.Message)
    }
}
$firewallProfiles = $null
try {
    $firewallProfiles = @(Get-NetFirewallProfile -PolicyStore ActiveStore | ForEach-Object {
        [ordered]@{
            name = $_.Name
            enabled = $_.Enabled.ToString()
            default_inbound_action = $_.DefaultInboundAction.ToString()
            allow_inbound_rules = $_.AllowInboundRules.ToString()
            shields_up_effective = ($_.Enabled.ToString() -eq 'True' -and
                $_.DefaultInboundAction.ToString() -eq 'Block' -and
                $_.AllowInboundRules.ToString() -eq 'False')
        }
    })
} catch {
    $errors.Add('Effective firewall profiles could not be read: ' + $_.Exception.Message)
}
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
    current_account_principal_source = $accountSource
    tailscale_installed = (Test-Path -LiteralPath $tailscalePath -PathType Leaf)
    tailscale_ipv4 = $addresses
    openssh_server_binary_present = $(if ($sshBinary) { Test-Path -LiteralPath $sshBinary -PathType Leaf } else { $null })
    openssh_server_binary_path = $sshBinary
    sshd_service_status = $(if ($sshService) { [string]$sshService.Status } else { 'NotInstalled' })
    firewall_effective_profiles = $firewallProfiles
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
