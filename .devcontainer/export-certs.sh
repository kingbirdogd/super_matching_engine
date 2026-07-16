#!/usr/bin/env bash
# Exports locally managed root CAs from Linux/macOS hosts into PEM .crt files
# consumed by the Dockerfile (.devcontainer/certs/).
set -euo pipefail

# Ensure the optional bind-mount sources referenced by devcontainer.json exist so
# `docker run` won't fail on a fresh host (missing ~/.ssh, ~/.claude, ~/.gitconfig,
# ~/.claude.json). Creating them here keeps this behaviour host-node-free.
for d in "${HOME}/.ssh" "${HOME}/.claude"; do
    mkdir -p "${d}"
done
for f in "${HOME}/.gitconfig" "${HOME}/.claude.json"; do
    [ -e "${f}" ] || : > "${f}"
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CERT_DIR="${SCRIPT_DIR}/certs"
mkdir -p "${CERT_DIR}"
find "${CERT_DIR}" -maxdepth 1 -type f -name '*.crt' -delete

exported=0

store_pem_cert() {
    local cert_file="$1"
    local prefix="$2"
    local thumbprint
    thumbprint="$(openssl x509 -in "${cert_file}" -noout -fingerprint -sha1 2>/dev/null | sed -E 's/.*=//; s/://g' | tr '[:upper:]' '[:lower:]')"

    if [[ -z "${thumbprint}" ]]; then
        return 0
    fi

    local short
    short="${thumbprint:0:8}"
    local out
    out="${CERT_DIR}/${prefix}-${short}.crt"

    if [[ -f "${out}" ]]; then
        return 0
    fi

    openssl x509 -in "${cert_file}" -out "${out}" >/dev/null 2>&1
    echo "WROTE $(basename "${out}") (${thumbprint})"
    exported=$((exported + 1))
}

export_from_macos() {
    local tmpdir
    tmpdir="$(mktemp -d)"
    local bundle
    bundle="${tmpdir}/macos-certs.pem"

    local login_keychain="${HOME}/Library/Keychains/login.keychain-db"
    if [[ -f "${login_keychain}" ]]; then
        security find-certificate -a -p /Library/Keychains/System.keychain "${login_keychain}" > "${bundle}" 2>/dev/null || true
    else
        security find-certificate -a -p /Library/Keychains/System.keychain > "${bundle}" 2>/dev/null || true
    fi

    if [[ -s "${bundle}" ]]; then
        awk '
            /-----BEGIN CERTIFICATE-----/ {
                in_cert=1
                cert=sprintf("%s/cert-%05d.pem", tmpdir, ++n)
            }
            in_cert { print > cert }
            /-----END CERTIFICATE-----/ {
                in_cert=0
                close(cert)
            }
        ' tmpdir="${tmpdir}" "${bundle}"

        local cert
        for cert in "${tmpdir}"/cert-*.pem; do
            [[ -f "${cert}" ]] || continue
            store_pem_cert "${cert}" "macos-root"
        done
    fi

    rm -rf "${tmpdir}"
}

export_from_linux() {
    local dirs=(
        "/usr/local/share/ca-certificates"
        "/etc/ca-certificates/trust-source/anchors"
        "/etc/pki/ca-trust/source/anchors"
    )

    local d
    for d in "${dirs[@]}"; do
        [[ -d "${d}" ]] || continue

        local f
        for f in "${d}"/*.crt "${d}"/*.pem "${d}"/*.cer; do
            [[ -f "${f}" ]] || continue

            local tmp_pem
            tmp_pem="$(mktemp)"
            if openssl x509 -in "${f}" -noout >/dev/null 2>&1; then
                openssl x509 -in "${f}" -out "${tmp_pem}" >/dev/null 2>&1 || true
            elif openssl x509 -inform DER -in "${f}" -out "${tmp_pem}" >/dev/null 2>&1; then
                :
            else
                rm -f "${tmp_pem}"
                continue
            fi

            store_pem_cert "${tmp_pem}" "linux-local"
            rm -f "${tmp_pem}"
        done
    done
}

case "$(uname -s)" in
    Darwin)
        export_from_macos
        ;;
    Linux)
        export_from_linux
        ;;
    *)
        echo "WARN: Unsupported host OS for export-certs.sh: $(uname -s)"
        ;;
esac

if [[ "${exported}" -eq 0 ]]; then
    echo "WARN: No local root CAs were exported from this host."
else
    echo "Exported ${exported} certificate(s) to ${CERT_DIR}"
fi

exit 0