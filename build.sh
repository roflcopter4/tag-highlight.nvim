#!/bin/sh

bin_dir=
top_dir=
project_dir=
system_type=
link_cmd=
link_bin=false
_CC=''
progname=$(basename "$0")

################################################################################
# Utilities

Exists() {
    command -v "$@" >/dev/null 2>&1
}

warn() {
    if [ $# -eq 0 ]; then
        printf '\033[1;31m%s: Error encountered.\033[0m\n' "$progname"
    else
        printf '\033[1;31m%s: Error:\033[0m %s\n' "$progname" "$1" >&2
    fi
    return 1
}

die() {
    if [ $# -eq 0 ]; then
        printf '\033[1;31m%s: Fatal error encountered.\033[0m\n' "$progname"
    else
        printf '\033[1;31m%s: Fatal error:\033[0m %s\n' "$progname" "$1" >&2
    fi
    exit 1
}

make_link() {
    case "$system_type" in
        MinGW) _command_="$link_cmd '$(cygpath -wa "$1")' '$(cygpath -wa "$2")'" ;;
        *)     _command_="$link_cmd '$(realpath "$1")' '$(realpath "$2")'"       ;;
    esac

    printf '%s\n' "$_command_" >&2
    eval $_command_
}

################################################################################

check() {
    Exists 'cmake' || die 'Cmake not found'
    Exists 'llvm-config' || die 'llvm not found'
    Exists "$CC" || Exists 'gcc' || Exists 'clang' || die 'No appropriate compiler found.'
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

get_link_command() {
    case "$system_type" in
    MinGW)
        if Exists 'winln'; then
            echo 'winln -sf'
        else
            echo 'cp -L'
        fi
        ;;
    *)
        echo 'ln -sf'
        ;;
    esac
}

find_compiler() {
    local MY_CC
    MY_CC=''
    if [ "$1" ] && Exists "$1"; then
        MY_CC="$1"
    elif [ "$CC" ] && Exists "$CC"; then
        MY_CC="$CC"
    elif Exists 'gcc'; then
        MY_CC=gcc
    elif Exists 'clang'; then
        MY_CC=clang
    else
        die 'No valid compiler found'
    fi

    echo "$MY_CC"
    unset MY_CC
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

################################################################################
# Compile & Install

do_all() {
    init
    do_go_binary
    compile "$@"
    install
}

do_go_binary() {
    build_go_binary || warn "Failed to build the go binary"
}

build_go_binary() {
    cd "${project_dir}/src/lang/golang" || return 1
    go build ./main.go || return 2
    mv main "${top_dir}/bin/golang" || return 3
    return 0
}

compile() {
    # echo 'Compiling Tag-Highlight.nvim' >&2
    check
    cd "$project_dir" || die
    if [ -d 'build' ]; then
        mv build _build_bak || die
    fi
    { mkdir build && cd build; } || die

    cmake_make_system='Unix Makefiles'
    [ "$system_type" = 'MinGW' ] && cmake_make_system='MSYS Makefiles'

    cmake -DCMAKE_BUILD_TYPE="${2:-Release}" -DCMAKE_EXPORT_COMPILE_COMMANDS=YES \
          -DCMAKE_C_COMPILER="$(find_compiler "${_CC}")" -G "${cmake_make_system}" \
          -DUSE_LIBV=TRY .. ||
        die 'Cmake configuration failed.'

    num_jobs=$(nproc)
    [ "$num_jobs" ] || num_jobs=4

    make -j "$num_jobs" || die 'Compilation failed'
}

install() {
    # echo 'Installing Tag-Highlight.nvim' >&2
    cd "$top_dir" || die
    mkdir -p bin

    if [ -z "$binary_path" ] || [ -z "${final_path}" ]; then
        die 'Invalid'
    fi

    if $link_bin; then
        make_link "${binary_path}" "${bin_dir}"
    else
        make -C "${project_dir}/build" 'install/strip'
        rm -r "${project_dir}/build"
    fi
}

################################################################################
# The script

show_help() {
    cat <<EOF
Usage: $0 -[hl] <command>
Options:
  -h   Show this help
  -l   Link the binary rather than copy it
Valid commands:
  setup     Create the directory structure and the necessary vim files
  update    Rebuild the binary only
  install   Setup and install
  dirs      Create or update the vim directories and/or files
Defaults to 'install' if no command is specified.
EOF
}

Exists 'uname' || die <<'EOF'
A Posix build environment is required. How you have a working shell with no
`uname(1)' utility is beyond my ability to interpret.
EOF

top_dir=$(dirname "$(realpath "$0")")
project_dir="${top_dir}/tag_highlight"
bin_dir="${top_dir}/bin"
system_type=$(guess_system)
link_cmd=$(get_link_command)

mkdir -p "${top_dir}/autoload"
mkdir -p "${top_dir}/autoload/tag_highlight"
mkdir -p "${top_dir}/cache"
mkdir -p "${top_dir}/cache/tags"
mkdir -p "${top_dir}/bin"

while getopts 'lh' ARG "$@"; do
    case $ARG in
        l) link_bin=true ;;
        h) show_help; exit 0 ;;
        *) show_help; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

while [ "$1" ]; do
    _ARG="$1"
    shift 1
    case "$_ARG" in
    CC=*)
        _CC="$(echo "$1" | sed 's/CC=//')"
        ;;
    esac
done

if [ "$1" ]; then
    case "$1" in
        dirs | setup | info)
            init
            ;;
        update | make | all)
            do_all "$@"
            ;;
        install)
            if [ -d "${project_dir}/build" ]; then
                init
                install
            else
                do_all "$@"
            fi
            ;;
        go|golang)
            do_go_binary
            ;;
        *)
            die 'Invalid command'
    esac
else
    do_all
fi
