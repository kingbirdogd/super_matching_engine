#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'

$RootDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ImageName = 'super_cmake-dev-shell:latest'
$ContainerName = 'super_cmake-dev-shell'

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Write-Error 'ERROR: docker is not installed or not on PATH.'
    exit 1
}

Write-Host '[1/4] Exporting host certs and creating optional mount files...'
$ExportCertsScript = Join-Path $RootDir '.devcontainer/export-certs.sh'
if ((Test-Path $ExportCertsScript) -and (Get-Command bash -ErrorAction SilentlyContinue)) {
    & bash $ExportCertsScript
} else {
    Write-Host 'Skipping cert export (bash or script not found).'
}

Write-Host "[2/4] Building Docker image: $ImageName"
& docker build `
    -f (Join-Path $RootDir 'Dockerfile') `
    -t $ImageName `
    $RootDir

Write-Host "[3/4] Removing previous container (if exists): $ContainerName"
& docker rm -f $ContainerName *> $null

Write-Host '[4/4] Starting development shell in container...'

$HomeDir = if ($env:HOME) { $env:HOME } else { $env:USERPROFILE }
$DockerArgs = @(
    'run', '--rm', '-it',
    '--name', $ContainerName,
    '--cap-add=SYS_PTRACE',
    '--security-opt', 'seccomp=unconfined',
    '-v', "${RootDir}:/workspace",
    '-w', '/workspace'
)

$SshPath = Join-Path $HomeDir '.ssh'
if (Test-Path $SshPath) {
    $DockerArgs += @('-v', "${SshPath}:/root/.ssh-host:ro")
}

$GitConfigPath = Join-Path $HomeDir '.gitconfig'
if (Test-Path $GitConfigPath) {
    $DockerArgs += @('-v', "${GitConfigPath}:/root/.gitconfig-host:ro")
}

$ClaudeDir = Join-Path $HomeDir '.claude'
if (Test-Path $ClaudeDir) {
    $DockerArgs += @('-v', "${ClaudeDir}:/root/.claude")
}

$ClaudeJson = Join-Path $HomeDir '.claude.json'
if (Test-Path $ClaudeJson) {
    $DockerArgs += @('-v', "${ClaudeJson}:/root/.claude.json")
}

$DockerArgs += @(
    $ImageName,
    'bash', '-lc', 'bash .devcontainer/postCreate.sh && exec bash'
)

& docker @DockerArgs
