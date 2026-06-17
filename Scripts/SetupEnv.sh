# setup_env.sh — Set NanoEFI SDK environment variables for the current shell
# Usage: . scripts/setup_env.sh   (source to persist in current session)

export NANOEFI_SDK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)"
echo "NANOEFI_SDK_DIR = $NANOEFI_SDK_DIR"
