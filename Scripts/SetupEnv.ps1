# setup_env.ps1 — Set NanoEFI SDK environment variables for the current shell
# Usage: . .\scripts\setup_env.ps1   (dot-source to persist in current session)

$env:NANOEFI_SDK_DIR = (Split-Path $PSScriptRoot -Parent)
Write-Host "NANOEFI_SDK_DIR = $env:NANOEFI_SDK_DIR"
