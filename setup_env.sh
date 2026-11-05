#!/usr/bin/env bash
# ==============================================================================
# NKFADC500_mini_CLI — Environment Setup Script (source setup_env.sh)
# ==============================================================================
# Usage:
#   source /path/to/NKFADC500_mini_CLI/setup_env.sh
# Or add the line above to ~/.bashrc for permanent registration.
# ==============================================================================

_NKFADC_SRC="${BASH_SOURCE[0]}"
_NKFADC_REAL="$(readlink -f "${_NKFADC_SRC}" 2>/dev/null || realpath "${_NKFADC_SRC}" 2>/dev/null || echo "${_NKFADC_SRC}")"
export NKFADC500_ROOT="$(cd "$(dirname "${_NKFADC_REAL}")" && pwd)"

# 1. CERN ROOT environment (if not already loaded)
if ! command -v root-config >/dev/null 2>&1; then
    for _root_sh in /usr/local/bin/thisroot.sh /opt/root/bin/thisroot.sh; do
        if [ -f "${_root_sh}" ]; then
            # shellcheck disable=SC1090
            source "${_root_sh}"
            break
        fi
    done
fi

# 2. Notice Korea SDK path (NKHOME)
if [ -z "${NKHOME:-}" ] && [ -d "/usr/local/notice" ]; then
    export NKHOME="/usr/local/notice"
fi

# 3. Executable PATH registration (frontend_nkfadc500, production_nkfadc_500, online_nkfadc500, fadc500_gui)
case ":${PATH:-}:" in
    *":${NKFADC500_ROOT}/bin:"*) ;;
    *) export PATH="${NKFADC500_ROOT}/bin${PATH:+:${PATH}}" ;;
esac

# 4. Shared library & ROOT PCM dictionary path (LD_LIBRARY_PATH & ROOT_INCLUDE_PATH)
for _lib_dir in "${NKFADC500_ROOT}/lib" "${NKHOME:-/usr/local/notice}/lib"; do
    if [ -d "${_lib_dir}" ]; then
        case ":${LD_LIBRARY_PATH:-}:" in
            *":${_lib_dir}:"*) ;;
            *) export LD_LIBRARY_PATH="${_lib_dir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" ;;
        esac
    fi
done

for _inc_dir in "${NKFADC500_ROOT}/objects/include" "${NKFADC500_ROOT}/core/include"; do
    if [ -d "${_inc_dir}" ]; then
        case ":${ROOT_INCLUDE_PATH:-}:" in
            *":${_inc_dir}:"*) ;;
            *) export ROOT_INCLUDE_PATH="${_inc_dir}${ROOT_INCLUDE_PATH:+:${ROOT_INCLUDE_PATH}}" ;;
        esac
    fi
done

unset _NKFADC_SRC _NKFADC_REAL _root_sh _lib_dir _inc_dir
echo "[NKFADC500_mini_CLI] Environment initialized: NKFADC500_ROOT=${NKFADC500_ROOT}"
