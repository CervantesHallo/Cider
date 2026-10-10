# One-time, attended setup. Existing SSH services/configuration are not overwritten.
# Account/key and host-address results are operational data: do not commit the receipt.
param(
    [Parameter(Mandatory = $true)][string]$PublicKey,
    [ValidateRange(1024, 65535)][int]$Port = 2222,
    [switch]$UseMsi
)
$ErrorActionPreference = 'Stop'
$clock = [Diagnostics.Stopwatch]::StartNew()
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal $identity
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script from an Administrator Windows PowerShell window.'
}
if ($PublicKey -notmatch '\Assh-ed25519 [A-Za-z0-9+/]+={0,2}( [A-Za-z0-9_.-]+)?\z') {
    throw 'Expected a single project ed25519 public key, without configuration text.'
}
$keyBlob = [Convert]::FromBase64String(($PublicKey -split ' ')[1])
if ($keyBlob.Length -ne 51) { throw 'Invalid ed25519 key encoding.' }
$login = $env:USERNAME.ToLowerInvariant()
if ($login -notmatch '\A[a-z0-9_.-]+\z') { throw 'This Windows login needs an individually reviewed SSH account mapping.' }
$sshRoot = Join-Path $env:ProgramData 'ssh'
$config = Join-Path $sshRoot 'sshd_config'
$project = Join-Path $env:ProgramData 'Cider\WindowsReferenceAccess'
$ruleName = 'Cider-WindowsReference-SSH'
$existingHostKeys = @(Get-ChildItem -LiteralPath $sshRoot -Filter 'ssh_host_*' -ErrorAction SilentlyContinue)
if ((Get-Service -Name sshd -ErrorAction SilentlyContinue) -or
        (Test-Path -LiteralPath $config) -or
        (Test-Path -LiteralPath $project) -or
        ($existingHostKeys.Count -ne 0) -or
        (Get-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue)) {
    throw 'Existing SSH or project setup found. Stop and send its state; do not overwrite or repeat setup.'
}
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners() |
        Where-Object { $_.Port -eq $Port }).Count -ne 0) {
    throw 'Requested SSH port is already occupied.'
}
$capability = $null
if (-not $UseMsi) { $capability = Get-WindowsCapability -Online -Name 'OpenSSH.Server~~~~0.0.1.0' }
$installedHere = $false
$installationUncertain = $false
$configured = $false
$ownedService = $false
$ownedProject = $false
$ownedRule = $false
$configWritten = $false
$backup = $null
$installJob = $null
$queryJob = $null
$msiProcess = $null
$provider = 'windows-capability'
try {
    if ($UseMsi) {
        $provider = 'microsoft-msi-10.0.0.0p2-preview'
        # Read the in-box state without starting another update/install request.
        $queryJob = Start-Job -ScriptBlock {
            $ErrorActionPreference = 'Stop'
            [string](Get-WindowsCapability -Online -Name 'OpenSSH.Server~~~~0.0.1.0').State
        }
        if (-not (Wait-Job -Job $queryJob -Timeout 15)) {
            throw 'Windows component state could not be read promptly. Stop instead of installing a second provider.'
        }
        $inboxState = @(Receive-Job -Job $queryJob -ErrorAction Stop)
        if ($queryJob.State -ne 'Completed' -or $inboxState.Count -ne 1 -or $inboxState[0] -ne 'NotPresent') {
            throw 'The in-box OpenSSH state is not NotPresent. Stop instead of installing a second provider.'
        }
        Remove-Job -Job $queryJob
        $queryJob = $null
        $packageDirectory = Join-Path $env:TEMP ('Cider-OpenSSH-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $packageDirectory | Out-Null
        $package = Join-Path $packageDirectory 'OpenSSH-Win64-v10.0.0.0.msi'
        $msiLog = Join-Path $packageDirectory 'install.log'
        Write-Host 'Downloading the standalone Microsoft package using this PowerShell session...'
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 120 -Uri 'https://github.com/PowerShell/Win32-OpenSSH/releases/download/10.0.0.0p2-Preview/OpenSSH-Win64-v10.0.0.0.msi' -OutFile $package
        if ((Get-Item -LiteralPath $package).Length -ne 6586368 -or
                (Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash -ne 'ddec9c53864280759cf9f74791cefd387100e3946aa849a1c138a4ed1b96b7d9') {
            throw 'Microsoft package size/hash mismatch.'
        }
        $signature = Get-AuthenticodeSignature -LiteralPath $package
        if ($signature.Status -ne 'Valid' -or -not $signature.SignerCertificate -or
                $signature.SignerCertificate.Subject -notmatch '(^|,\s*)O=Microsoft Corporation(,|$)') {
            throw 'The package does not have a valid Microsoft Authenticode signature.'
        }
        # Server only: keep the existing Windows SSH client and its search path.
        $arguments = '/i "' + $package + '" ADDLOCAL=Server /qn /norestart /L*v "' + $msiLog + '"'
        Write-Host 'Installing the standalone server (waiting at most 2 minutes)...'
        $msiProcess = Start-Process -FilePath (Join-Path $env:WINDIR 'System32\msiexec.exe') -ArgumentList $arguments -PassThru
        $null = $msiProcess.Handle
        if (-not $msiProcess.WaitForExit(120000)) {
            $installationUncertain = $true
            Write-Host ('Installer log: ' + $msiLog)
            throw 'The MSI request is still running. Stop here; do not repeat setup or start another installer.'
        }
        $msiProcess.Refresh()
        if ($msiProcess.ExitCode -eq 3010 -or $msiProcess.ExitCode -eq 1641) {
            throw 'The MSI requested a restart. A human handoff is needed; SSH has not been configured.'
        }
        if ($msiProcess.ExitCode -ne 0) {
            Write-Host ('Installer log: ' + $msiLog)
            throw ('Standalone installer failed with exit code ' + $msiProcess.ExitCode)
        }
        $installedHere = $true
        $serverDirectory = Join-Path $env:ProgramFiles 'OpenSSH'
    } elseif ($capability.State -ne 'Installed') {
        Write-Host 'Installing Windows OpenSSH Server (waiting at most 5 minutes)...'
        $installJob = Start-Job -ScriptBlock {
            $ErrorActionPreference = 'Stop'
            Add-WindowsCapability -Online -Name 'OpenSSH.Server~~~~0.0.1.0'
        }
        if (-not (Wait-Job -Job $installJob -Timeout 300)) {
            $installationUncertain = $true
            Stop-Job -Job $installJob
            throw 'Component installation timed out. Windows servicing may still be completing it; send this error instead of retrying.'
        }
        if ($installJob.State -ne 'Completed') {
            Receive-Job -Job $installJob -ErrorAction Stop
            throw 'OpenSSH component installation failed; send the job error.'
        }
        $result = @(Receive-Job -Job $installJob -ErrorAction Stop)
        Remove-Job -Job $installJob
        $installJob = $null
        $installedHere = $true
        if (@($result | Where-Object { $_.RestartNeeded }).Count -ne 0) {
            throw 'Windows requested a restart. SSH has not been configured; a human handoff is needed.'
        }
    }
    if (-not $UseMsi) { $serverDirectory = Join-Path $env:WINDIR 'System32\OpenSSH' }
    $server = Join-Path $serverDirectory 'sshd.exe'
    $keygen = Join-Path $serverDirectory 'ssh-keygen.exe'
    if (-not (Test-Path -LiteralPath $server -PathType Leaf) -or
            -not (Test-Path -LiteralPath $keygen -PathType Leaf) -or
            -not (Get-Service -Name sshd -ErrorAction SilentlyContinue)) {
        throw 'The OpenSSH provider did not provide the expected service and binaries.'
    }
    $serviceRegistration = Get-ItemProperty -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Services\sshd'
    $expectedImage = '"' + $server + '"'
    if ($serviceRegistration.ImagePath -ne $server -and $serviceRegistration.ImagePath -ne $expectedImage) {
        throw 'The SSH service points at a different provider. Stop instead of changing it.'
    }
    $ownedService = $true
    Stop-Service -Name sshd
    Set-Service -Name sshd -StartupType Manual
    # Installation can create a default port-22 firewall rule; this fresh service uses another port.
    if ($installedHere) {
        Get-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -ErrorAction SilentlyContinue |
            Disable-NetFirewallRule | Out-Null
    }
    $rdp = Get-ItemProperty -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Control\Terminal Server'
    $rdpTransport = Get-ItemProperty -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Control\Terminal Server\WinStations\RDP-Tcp'
    $rdpPort = [int]$rdpTransport.PortNumber
    if ($rdpPort -lt 1 -or $rdpPort -gt 65535) { throw 'Invalid Remote Desktop port.' }
    New-Item -ItemType Directory -Path $project | Out-Null
    $ownedProject = $true
    & "$env:WINDIR\System32\icacls.exe" $project '/inheritance:r' '/grant:r' '*S-1-5-32-544:(OI)(CI)F' '*S-1-5-18:(OI)(CI)F' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot protect the project SSH directory.' }
    $encoding = New-Object Text.UTF8Encoding $false
    $authorizedKeys = Join-Path $project 'authorized_keys'
    [IO.File]::WriteAllText($authorizedKeys, $PublicKey + "`r`n", $encoding)
    & "$env:WINDIR\System32\icacls.exe" $authorizedKeys '/inheritance:r' '/grant:r' '*S-1-5-32-544:F' '*S-1-5-18:F' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot protect the project authorized key.' }
    New-Item -ItemType Directory -Path $sshRoot -Force | Out-Null
    if (Test-Path -LiteralPath $config) {
        $backupPath = Join-Path $project 'sshd_config.before'
        Copy-Item -LiteralPath $config -Destination $backupPath
        $backup = $backupPath
    }
    $keysPath = $authorizedKeys.Replace('\', '/')
    $text = @(
        '# Cider attended Windows-reference access'
        "Port $Port"
        'PubkeyAuthentication yes'
        'PasswordAuthentication no'
        'AuthenticationMethods publickey'
        "AllowUsers $login"
        ('AuthorizedKeysFile "' + $keysPath + '"')
        'AllowTcpForwarding local'
        "PermitOpen 127.0.0.1:$rdpPort"
        ('Subsystem sftp "' + (Join-Path $serverDirectory 'sftp-server.exe').Replace('\', '/') + '"')
    ) -join "`r`n"
    $configWritten = $true
    [IO.File]::WriteAllText($config, $text + "`r`n", $encoding)
    & $keygen -A
    if ($LASTEXITCODE -ne 0) { throw 'Host key generation failed.' }
    foreach ($hostPrivateKey in @(Get-ChildItem -LiteralPath $sshRoot -Filter 'ssh_host_*_key' -File)) {
        & "$env:WINDIR\System32\icacls.exe" $hostPrivateKey.FullName '/inheritance:r' '/grant:r' '*S-1-5-32-544:F' '*S-1-5-18:F' | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Cannot protect a new SSH host private key.' }
        & "$env:WINDIR\System32\icacls.exe" $hostPrivateKey.FullName '/setowner' '*S-1-5-18' | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Cannot assign the host private key to SYSTEM.' }
        & "$env:WINDIR\System32\icacls.exe" $hostPrivateKey.FullName '/remove:g' ('*' + $identity.User.Value) | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Cannot remove the installer-only host-key permission.' }
    }
    & $server -t -f $config
    if ($LASTEXITCODE -ne 0) { throw 'OpenSSH rejected the configuration.' }
    New-NetFirewallRule -Name $ruleName -DisplayName 'Cider Windows Reference SSH' -Enabled True -Direction Inbound -Protocol TCP -LocalPort $Port -Program $server -Action Allow -Profile Any | Out-Null
    $ownedRule = $true
    Start-Service -Name sshd
    Set-Service -Name sshd -StartupType Automatic
    $hostKey = Join-Path $sshRoot 'ssh_host_ed25519_key.pub'
    $fingerprint = @(& $keygen -lf $hostKey -E sha256)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read the SSH host fingerprint.' }
    $clientFingerprint = @(& $keygen -lf $authorizedKeys -E sha256)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read the project key fingerprint.' }
    if ((Get-Service -Name sshd).Status -ne 'Running' -or
            @([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners() |
                Where-Object { $_.Port -eq $Port }).Count -eq 0) {
        throw 'The configured service is not listening on the expected SSH port.'
    }
    $addresses = @([Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces() |
        Where-Object { $_.OperationalStatus -eq 'Up' -and $_.NetworkInterfaceType -ne 'Loopback' } |
        ForEach-Object { $_.GetIPProperties().UnicastAddresses } |
        Where-Object { $_.Address.AddressFamily -eq 'InterNetwork' } |
        ForEach-Object { $_.Address.ToString() })
    $report = [ordered]@{
        schema = 'cider.windows-ssh-setup/v1'
        status = 'configured'
        provider = $provider
        elapsed_seconds = [Math]::Round($clock.Elapsed.TotalSeconds, 2)
        setup_source_sha256 = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
        ssh_user = $login
        ssh_port = $Port
        host_key_fingerprint = $fingerprint
        authorized_key_fingerprint = $clientFingerprint
        local_ipv4 = $addresses
        rdp_enabled = ($rdp.fDenyTSConnections -eq 0)
        rdp_port = $rdpPort
        public_forwarding_verified = $false
        desktop_automation_verified = $false
    }
    $json = $report | ConvertTo-Json -Depth 4
    [IO.File]::WriteAllText((Join-Path $project 'setup-result.json'), $json + "`r`n", $encoding)
    $configured = $true
    Write-Output $json
    Write-Host ('Saved: ' + (Join-Path $project 'setup-result.json'))
} catch {
    Write-Host ('Setup failed: ' + $_.Exception.Message)
    if (-not $configured -and -not $installationUncertain) {
        # Only the fresh service and files created by this attempt are involved.
        if ($ownedService) {
            Get-Service -Name sshd -ErrorAction SilentlyContinue | Stop-Service -ErrorAction SilentlyContinue
            if (Get-Service -Name sshd -ErrorAction SilentlyContinue) { Set-Service -Name sshd -StartupType Manual }
        }
        if ($ownedRule) { Remove-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue }
        if ($configWritten -and $backup) { Copy-Item -LiteralPath $backup -Destination $config -Force }
        elseif ($configWritten -and (Test-Path -LiteralPath $config)) { Remove-Item -LiteralPath $config }
        if ($ownedProject) { Remove-Item -LiteralPath (Join-Path $project 'authorized_keys') -ErrorAction SilentlyContinue }
    }
    throw
} finally {
    if ($queryJob) {
        Stop-Job -Job $queryJob -ErrorAction SilentlyContinue
        Remove-Job -Job $queryJob -Force -ErrorAction SilentlyContinue
    }
    if ($installJob) {
        Stop-Job -Job $installJob -ErrorAction SilentlyContinue
        Remove-Job -Job $installJob -Force -ErrorAction SilentlyContinue
    }
}
