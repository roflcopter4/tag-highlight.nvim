#!/bin/sh

bin_dir=
top_dir=
project_dir=
system_type=

################################################################################

Exists() {
    command -v "$@" >/dev/null 2>&1
}

die() {
    if [ $# -eq 0 ]; then
        printf '\033[1;31mFatal error encountered.\033[0m\n'
    else
        printf '\033[1;31mFatal error:\033[0m %s\n' "$1" >&2
    fi
    exit 1
}

check() {
    Exists 'cmake' || die 'Cmake not found'
    Exists 'llvm-config' || die 'llvm not found'
    Exists 'gcc' || Exists 'clang' || die 'No appropriate compiler found.'
}

guess_system() {
    _output=$(uname)
    case "$_output" in
    *MSYS* | *MINGW*)
        echo 'MinGW'
        ;;
    FreeBSD)
        echo 'FreeBSD'
        ;;
    Linux)
        echo 'linux'
        ;;
    *)
        die 'Unable to identify system type'
        ;;
    esac
}

################################################################################

init() {
    case "$system_type" in
    MinGW)
        binary_path="${project_dir}/build/src/tag_highlight.exe"
        final_path=$(cygpath -wa "${bin_dir}/tag_highlight.exe")
        binary_install_path=$(cygpath -wa "${bin_dir}")
        cache_path=$(cygpath -wa "${top_dir}/cache")
        ;;
    *)
        binary_path="${project_dir}/build/src/tag_highlight"
        final_path="${bin_dir}/tag_highlight"
        binary_install_path="${bin_dir}"
        cache_path="${top_dir}/cache"
        ;;
    esac

    mkdir -p "${top_dir}/autoload/tag_highlight"
    mkdir -p "${top_dir}/cache"

    cat >"${top_dir}/autoload/tag_highlight/install_info.vim" <<EOF
function! tag_highlight#install_info#GetBinaryName()
    return '${final_path}'
endfunction

function! tag_highlight#install_info#GetBinaryPath()
    return '${binary_install_path}'
endfunction

function! tag_highlight#install_info#GetCachePath()
    return '${cache_path}'
endfunction
EOF
}

compile() {
    echo 'Compiling Tag-Highlight.nvim' >&2
    check
    cd "$project_dir" || die
    if [ -d 'build' ]; then
        # rm -r build || die
        mv build _build_bak
    fi
    { mkdir build && cd build; } || die

    cmake_make_system='Unix Makefiles'
    [ "$system_type" = 'MinGW' ] && cmake_make_system='MSYS Makefiles'

    cmake -DCMAKE_BUILD_TYPE="${2:-Release}" -DCMAKE_EXPORT_COMPILE_COMMANDS=YES \
        -DCMAKE_C_COMPILER="${1:-gcc}" -G "${cmake_make_system}" .. ||
        die 'Cmake configuration failed.'

    num_jobs=$(nproc)
    [ "$num_jobs" ] || num_jobs=4

    make -j "$num_jobs" || die 'Compilation failed'
}

install() {
    echo 'Installing Tag-Highlight.nvim' >&2
    cd "$top_dir" || die
    mkdir -p bin

    case "$system_type" in
    MinGW)
        if Exists 'winln'; then
            link_cmd='winln -s'
        else
            link_cmd='cp -L'
        fi

        for libname in libclang.dll libgcc_s_seh-1.dll libgomp-1.dll libstdc++-6.dll libwinpthread-1.dll libz3.dll zlib1.dll; do
            _command_="$link_cmd '$(cygpath -wa "/mingw64/bin/${libname}")' '${binary_install_path}'"
            printf '%s\n' "$_command_" >&2
            eval $_command_
        done
        ;;
    esac

    if [ -z "$binary_path" ] || [ -z "${final_path}" ]; then
        die 'Invalid'
    fi

    printf "mv '%s' '%s'\\n" "${binary_path}" "${bin_dir}" >&2
    mv "${binary_path}" "${bin_dir}"
    rm -r "${project_dir}/build"

    mkdir -p "${HOME}/.vim_tags"
    mkdir -p "${HOME}/.tag_highlight"
    mkdir -p "${HOME}/.tag_highlight_log"
}

################################################################################

Exists 'printf' || echo "A Posix build environment is required"
Exists 'uname' || die 'A Posix build environment is required'

top_dir=$(dirname "$(realpath "$0")")
project_dir="${top_dir}/tag_highlight"
bin_dir="${top_dir}/bin"
system_type=$(guess_system)

_all() {
    init
    compile "$@"
    install
}

if [ $# -gt 0 ]; then
    for ARG in "$@"; do
        case "$ARG" in
        dirs | setup | info)
            init
            ;;
        install | update | make)
            _all "$@"
            ;;
        *)
            exit 1
            ;;
        esac
    done
else
    _all "$@"
fi
