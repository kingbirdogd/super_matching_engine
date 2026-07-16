# Exports the host's private/corporate root CAs (incl. the Netskope TLS-inspection
# root) to PEM .crt files consumed by the Dockerfile so the container trusts them.
#
# Rather than hard-coding thumbprints, the corporate roots are discovered
# dynamically from the host's certificate stores: every certificate in
# LocalMachine\Root that was installed *locally* -- via Group Policy, an
# enterprise push, or by hand (how the Netskope root and similar TLS-inspection
# CAs land) -- as opposed to the public roots managed by the Microsoft Trusted
# Root auto-update program. Those locally-installed roots are exactly the
# private/proxy CAs the container needs and that a stock Ubuntu ca-certificates
# bundle won't already trust.
$ErrorActionPreference = 'Stop'

# Ensure the optional bind-mount sources referenced by devcontainer.json exist so
# `docker run` won't fail on a fresh host (missing ~/.ssh, ~/.claude, ~/.gitconfig,
# ~/.claude.json). Creating them here keeps this behaviour host-node-free.
$home_ = $env:USERPROFILE
foreach ($d in '.ssh', '.claude') {
    New-Item -ItemType Directory -Force -Path (Join-Path $home_ $d) | Out-Null
}
foreach ($f in '.gitconfig', '.claude.json') {
    $p = Join-Path $home_ $f
    if (-not (Test-Path $p)) { New-Item -ItemType File -Force -Path $p | Out-Null }
}

$dir = Join-Path $PSScriptRoot 'certs'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

# Registry-backed physical stores that hold locally-installed roots. Subkey names
# under each are the certificates' SHA-1 thumbprints.
$rootStoreKeys = @(
    'HKLM:\SOFTWARE\Policies\Microsoft\SystemCertificates\Root\Certificates'  # Group Policy
    'HKLM:\SOFTWARE\Microsoft\EnterpriseCertificates\Root\Certificates'       # Enterprise
    'HKLM:\SOFTWARE\Microsoft\SystemCertificates\Root\Certificates'           # Local machine / manual
)

$localThumbprints = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($key in $rootStoreKeys) {
    if (Test-Path $key) {
        Get-ChildItem $key | ForEach-Object { [void]$localThumbprints.Add($_.PSChildName) }
    }
}

# Clear previously-exported certs so roots removed from the host don't linger.
Get-ChildItem -Path $dir -Filter '*.crt' -File -ErrorAction SilentlyContinue | Remove-Item -Force

$exported = 0
foreach ($c in Get-ChildItem Cert:\LocalMachine\Root) {
    if (-not $localThumbprints.Contains($c.Thumbprint)) { continue }

    # Build a stable, filesystem-safe name from the subject common name (falling
    # back to the thumbprint), plus a short thumbprint suffix for uniqueness.
    $cn = $c.GetNameInfo(
        [System.Security.Cryptography.X509Certificates.X509NameType]::SimpleName, $false)
    if ([string]::IsNullOrWhiteSpace($cn)) { $cn = $c.Thumbprint }
    $safe = ($cn -replace '[^A-Za-z0-9._-]', '-').Trim('-').ToLowerInvariant()
    if ([string]::IsNullOrWhiteSpace($safe)) { $safe = 'root-ca' }
    $name = '{0}-{1}' -f $safe, $c.Thumbprint.Substring(0, 8).ToLowerInvariant()

    $b64 = [System.Convert]::ToBase64String($c.RawData, 'InsertLineBreaks')
    $pem = "-----BEGIN CERTIFICATE-----`r`n$b64`r`n-----END CERTIFICATE-----`r`n"
    $path = Join-Path $dir ($name + '.crt')
    [System.IO.File]::WriteAllText($path, $pem, (New-Object System.Text.UTF8Encoding($false)))
    Write-Output ("WROTE {0}.crt ({1})" -f $name, $c.Thumbprint)
    $exported++
}

if ($exported -eq 0) {
    Write-Warning ("No locally-installed root CAs found in LocalMachine\Root; " +
        "the container may not trust the corporate proxy.")
} else {
    Write-Output ("Exported {0} certificate(s) to {1}" -f $exported, $dir)
}
