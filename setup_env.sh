#!/usr/bin/env bash
# ==============================================================================
# NKFADC500_mini_CLI — Universal Environment Setup Script
# ==============================================================================
# [사용법 / Usage]
#   1) 현재 열린 터미널 창에 즉시 적용 (Apply to current terminal):
#        source ./setup_env.sh
#        (또는 프로젝트 폴더 안에서: source $(pwd)/setup_env.sh)
#
#   2) ~/.bashrc에 현재 경로를 자동으로 영구 등록 (Auto-register to ~/.bashrc):
#        echo "source $(pwd)/setup_env.sh" >> ~/.bashrc && source ~/.bashrc
#      또는 중복 방지 자동 설치 옵션:
#        source ./setup_env.sh --install
#
#   3) 환경 변수 및 공유 라이브러리(.so) 연결 상태 자가 진단 (Self-check):
#        source ./setup_env.sh --check
# ==============================================================================

_NKFADC_IS_SOURCED=0
if [ -n "${BASH_VERSION:-}" ] && [ "${BASH_SOURCE[0]}" != "${0}" ]; then
    _NKFADC_IS_SOURCED=1
fi

_NKFADC_SRC="${BASH_SOURCE[0]:-$0}"
_NKFADC_REAL="$(readlink -f "${_NKFADC_SRC}" 2>/dev/null || realpath "${_NKFADC_SRC}" 2>/dev/null || echo "${_NKFADC_SRC}")"
export NKFADC500_ROOT="$(cd "$(dirname "${_NKFADC_REAL}")" && pwd)"

# 1. CERN ROOT environment (if not already loaded in current shell)
if ! command -v root-config >/dev/null 2>&1; then
    for _root_sh in \
        /usr/local/bin/thisroot.sh \
        /opt/root/bin/thisroot.sh \
        "${HOME}/root/bin/thisroot.sh" \
        /usr/bin/thisroot.sh; do
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

# 5. Optional Modes: --install (idempotent ~/.bashrc registration) / --check (diagnostic check)
_NKFADC_ARG="${1:-}"

if [ "${_NKFADC_ARG}" = "--install" ] || [ "${_NKFADC_ARG}" = "--bashrc" ]; then
    _BASHRC_FILE="${HOME}/.bashrc"
    touch "${_BASHRC_FILE}"
    # Remove any previous/broken setup_env.sh lines for NKFADC500_mini_CLI to prevent duplicates
    if grep -q "setup_env.sh" "${_BASHRC_FILE}" 2>/dev/null; then
        sed -i '/# >>> NKFADC500_mini_CLI Environment Setup >>>/,/# <<< NKFADC500_mini_CLI Environment Setup <<</d' "${_BASHRC_FILE}"
        sed -i '\|NKFADC500_mini_CLI/setup_env.sh|d' "${_BASHRC_FILE}"
    fi
    cat << EOF >> "${_BASHRC_FILE}"

# >>> NKFADC500_mini_CLI Environment Setup >>>
if [ -f "${NKFADC500_ROOT}/setup_env.sh" ]; then
    source "${NKFADC500_ROOT}/setup_env.sh"
fi
# <<< NKFADC500_mini_CLI Environment Setup <<<
EOF
    echo "======================================================================"
    echo " [NKFADC500_mini_CLI] ~/.bashrc 영구 등록 완료!"
    echo "  • 등록된 경로 : ${NKFADC500_ROOT}/setup_env.sh"
    echo "  • 대상 파일   : ${_BASHRC_FILE}"
    echo "======================================================================"
    unset _BASHRC_FILE
fi

if [ "${_NKFADC_ARG}" = "--check" ] || [ "${_NKFADC_ARG}" = "--status" ]; then
    echo "======================================================================"
    echo " [NKFADC500_mini_CLI] 환경 변수 및 실행 파일 자가 진단 (Self-Check)"
    echo "======================================================================"
    echo "  1. NKFADC500_ROOT    : ${NKFADC500_ROOT}"
    echo "  2. NKHOME            : ${NKHOME:-'(내장 ./lib 번들 드라이버 사용)'}"
    echo "  3. CERN ROOT         : $(command -v root-config >/dev/null 2>&1 && root-config --version || echo '미감지 (thisroot.sh 확인 필요)')"
    echo "  4. fadc500_gui 위치  : $(command -v fadc500_gui 2>/dev/null || echo '미빌드 (cmake 빌드 필요)')"
    echo "  5. frontend 위치     : $(command -v frontend_nkfadc500 2>/dev/null || echo '미빌드 (cmake 빌드 필요)')"
    echo "  6. production 위치   : $(command -v production_nkfadc_500 2>/dev/null || echo '미빌드 (cmake 빌드 필요)')"
    if [ -f "${NKFADC500_ROOT}/lib/libFADC500Objects.so" ]; then
        echo "  7. ROOT Dictionary   : 정상 (${NKFADC500_ROOT}/lib/libFADC500Objects.so)"
    else
        echo "  7. ROOT Dictionary   : 미생성 (mkdir -p build && cd build && cmake .. && make -j 실행 필요)"
    fi
    echo "======================================================================"
elif [ "${_NKFADC_IS_SOURCED}" -eq 0 ] && [ "${_NKFADC_ARG}" != "--install" ] && [ "${_NKFADC_ARG}" != "--bashrc" ]; then
    echo "======================================================================"
    echo " [주의 / Notice] './setup_env.sh'를 단독 실행하셨습니다!"
    echo " 리눅스에서는 'source' 없이 스크립트를 실행하면 자식 셸(Subshell)에서만"
    echo " 환경 변수가 설정되고 현재 터미널 창에는 남지 않습니다."
    echo ""
    echo " ▶ 현재 터미널에 적용하려면 반드시 앞에 'source'를 붙여 실행하세요:"
    echo "     source ${NKFADC500_ROOT}/setup_env.sh"
    echo ""
    echo " ▶ 새 터미널을 열 때마다 자동 적용되게 하려면 (그대로 복사·붙여넣기):"
    echo "     echo \"source \$(pwd)/setup_env.sh\" >> ~/.bashrc && source ~/.bashrc"
    echo "======================================================================"
else
    echo "[NKFADC500_mini_CLI] Environment initialized: NKFADC500_ROOT=${NKFADC500_ROOT}"
fi

unset _NKFADC_IS_SOURCED _NKFADC_SRC _NKFADC_REAL _NKFADC_ARG _root_sh _lib_dir _inc_dir
