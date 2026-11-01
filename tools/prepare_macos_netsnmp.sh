#!/usr/bin/env bash
set -euo pipefail

# Build minimal SHA-256-verified Net-SNMP 5.10.pre2 for self-contained macOS packages
# (matching carlkidcrypto/ezsnmp v2026.10.28a0 standalone packaging).

NETSNMP_VERSION="5.10.pre2"
EXPECTED_SHA256="6bce4ef368db15ad0f400330d5a1433009d63f0a98ca8b2eca0f87abd3469bd6"
PREFIX="${PREFIX:-/usr/local}"

if [[ -z "${OPENSSL_PREFIX:-}" ]]; then
    if [[ -d "/opt/homebrew/opt/openssl@3" ]]; then
        OPENSSL_PREFIX="/opt/homebrew/opt/openssl@3"
    elif [[ -d "/usr/local/opt/openssl@3" ]]; then
        OPENSSL_PREFIX="/usr/local/opt/openssl@3"
    else
        OPENSSL_PREFIX="/opt/local"
    fi
fi

if [[ -x "${PREFIX}/bin/net-snmp-config" ]]; then
    INSTALLED_VER=$("${PREFIX}/bin/net-snmp-config" --version || true)
    if [[ "${INSTALLED_VER}" == *"5.10"* ]]; then
        echo "Net-SNMP ${INSTALLED_VER} already installed at ${PREFIX}."
        exit 0
    fi
fi

WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ezsnmp-dl-netsnmp.XXXXXX")
trap 'rm -rf "${WORK_DIR}"' EXIT

ARCHIVE_PATH="${WORK_DIR}/v${NETSNMP_VERSION}.tar.gz"
DOWNLOAD_URL="https://github.com/net-snmp/net-snmp/archive/refs/tags/v${NETSNMP_VERSION}.tar.gz"

curl --proto '=https' --proto-redir '=https' -fsSL -o "${ARCHIVE_PATH}" "${DOWNLOAD_URL}"
ACTUAL_SHA256=$(shasum -a 256 "${ARCHIVE_PATH}" | awk '{print $1}')
if [[ "${ACTUAL_SHA256}" != "${EXPECTED_SHA256}" ]]; then
    echo "ERROR: SHA256 mismatch! Expected ${EXPECTED_SHA256}, got ${ACTUAL_SHA256}" >&2
    exit 1
fi

tar -xzf "${ARCHIVE_PATH}" -C "${WORK_DIR}"
cd "${WORK_DIR}/net-snmp-${NETSNMP_VERSION}"

NPROCS=$(sysctl -n hw.ncpu 2>/dev/null || echo 2)
DEFAULT_MIBS="IP-MIB:IF-MIB:TCP-MIB:UDP-MIB:SNMPv2-MIB:RFC1213-MIB:SNMP-FRAMEWORK-MIB:SNMP-MPD-MIB:SNMP-USER-BASED-SM-MIB:SNMP-VIEW-BASED-ACM-MIB:SNMP-COMMUNITY-MIB"
DEFAULT_MIBDIRS="\$HOME/.snmp/mibs:/opt/homebrew/share/snmp/mibs:${PREFIX}/share/snmp/mibs"

CPPFLAGS="-I${OPENSSL_PREFIX}/include ${CPPFLAGS:-}" \
LDFLAGS="-L${OPENSSL_PREFIX}/lib ${LDFLAGS:-}" \
./configure \
    --prefix="${PREFIX}" \
    --with-defaults \
    --with-mibs="${DEFAULT_MIBS}" \
    --with-mibdirs="${DEFAULT_MIBDIRS}" \
    --enable-shared \
    --disable-static \
    --disable-agent \
    --disable-applications \
    --disable-manuals \
    --disable-scripts \
    --disable-mibs \
    --without-perl-modules \
    --disable-embedded-perl \
    --with-openssl="${OPENSSL_PREFIX}" \
    --enable-blumenthal-aes

make -j"${NPROCS}"
make install
"${PREFIX}/bin/net-snmp-config" --version
