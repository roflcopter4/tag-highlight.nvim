#!/bin/sh
# shellcheck shell=ash

binary_path=
binary_install_path=
final_path=
cache_path=

bin_dir=
top_dir=
project_dir=
system_type=
link_cmd=
link_bin=false
_CC=''
progname=$(basename "$0")

ninja_flag=false
verbose_flag=false
quiet_flag=false
jemalloc_opt=NO
libev_opt=NO
sanitize_opt=''
buildtype_opt='Release'

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
    if [ "$1" ]; then
        MY_CC="$1"
    elif [ "$CC" ]; then
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

conf_cond() {
    if [ "$1" ] && [ "x$1" != "xfalse" ]; then
        printf -- "%s" "$2"
    elif [ "$3" ]; then
        printf "%s" "$3"
    fi
}

################################################################################

init() {
    git submodule update --init

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
    do_c_binary "$@"
}

do_go_binary() {
    build_go_binary || warn "Failed to build the go binary"
}

do_c_binary() {
    compile "$@"
    install
}

build_go_binary() {
    cd "${project_dir}/src/lang/golang/go_src" || return 1
    go build || return 2
    if [ -x './golang' ]; then
        mv 'golang' "${top_dir}/bin/golang" || return 3
    elif [ -x './golang.exe' ]; then
        mv 'golang.exe' "${top_dir}/bin/golang.exe" || return 3
    else
        return 4
    fi
    return 0
}

compile() {
    # echo 'Compiling Tag-Highlight.nvim' >&2
    check
    cd "$project_dir" || die
    if [ -d 'build' ]; then
        mv build _build_bak >/dev/null 2>&1 || rm -rf build || die
    fi
    { mkdir build && cd build; } || die

    local num_jobs config_cmd cmake_make_system build_command

    num_jobs=$(nproc)
    [ "$num_jobs" ] || num_jobs=4

    if [ "$system_type" = 'MinGW' ]; then
        cmake_make_system='MSYS Makefiles'
        build_command="make -j '$num_jobs' 'VERBOSE=$(conf_cond "$verbose_flag" 1 0)'"
    elif $ninja_flag; then
        cmake_make_system='Ninja'
        build_command="ninja -j '$num_jobs' $(conf_cond "$verbose_flag" -v)"
    else
        cmake_make_system='Unix Makefiles'
        build_command="make -j '$num_jobs' 'VERBOSE=$(conf_cond "$verbose_flag" 1 0)'"
    fi

    config_cmd="cmake                                                           \
                -DCMAKE_EXPORT_COMPILE_COMMANDS=YES                             \
                -DCMAKE_BUILD_TYPE='${buildtype_opt:-Release}'                  \
                -DCMAKE_C_COMPILER='$(find_compiler "${_CC}")'                  \
                -DUSE_LIBEV='${libev_opt}'                                      \
                -DUSE_JEMALLOC='${jemalloc_opt}'                                \
                -DSANITIZE='$(conf_cond "$sanitize_opt" "$sanitize_opt" 'OFF')' \
                -G '${cmake_make_system}' .."

    echo $config_cmd
    eval $config_cmd || die "Cmake configuration failed."

    if $quiet_flag; then
        eval $build_command >/dev/null 2>&1 || die 'Compilation failed'
    else
        eval $build_command || die 'Compilation failed'
    fi
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
        "$(conf_cond $ninja_flag ninja make)" -C "${project_dir}/build" 'install/strip'
        rm -r "${project_dir}/build"
    fi
}

################################################################################
# The script

show_help() {
    cat <<EOF
Usage: $0 -[hl] <command>
Options:
  -h      Show this help
  -l      Link the binary rather than copy it
  -j      Use jemalloc
  -e      Force use of libev (default action is to use if it is available)
  -E      Do not use libev even if it is available
  -q      Do not echo build
  -v      Enable verbose build
  -N      Use ninja(1) instead of make(1) for the build.
  -S str  Sanitizer setting
  -B str  Cmake build type setting
  -C str  C compiler to use
Valid commands:
  setup      Create the directory structure and the necessary vim files
  update     Rebuild the binary only
  install    Setup and install
  dirs       Create or update the vim directories and/or files
  go|golang  Build or rebuild the go binary
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

while getopts 'lhjeEvqNS:B:C:' ARG "$@"; do
    case $ARG in
        (l) link_bin=true ;;
        (h) show_help; exit 0 ;;
        (j) jemalloc_opt=YES ;;
        (e) libev_opt=YES ;;
        (E) libev_opt=NO ;;
        (v) verbose_flag=true ;;
        (q) quiet_flag=true ;;
        (N) ninja_flag=true ;;
        (S) sanitize_opt="$OPTARG" ;;
        (B) buildtype_opt="$OPTARG" ;;
        (C) _CC="$OPTARG" ;;
        (*) show_help; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

while [ "$1" ]; do
    case "$1" in
    (dirs | setup | info)  init;             exit 0 ;;
    (update | make | all)  do_all "$@";      exit 0 ;;
    (build)                do_c_binary "$@"; exit 0 ;;
    (go | golang)          do_go_binary;     exit 0 ;;

    (CC=*)
        _CC="$(echo "$1" | sed 's/CC=//')"
        ;;
    (install)
        if [ -d "${project_dir}/build" ]; then
            init
            install
            exit 0
        else
            do_all "$@"
            exit 0
        fi
        ;;
    (*)
        die 'Invalid command'
        ;;
    esac
    shift 1
done

do_all
