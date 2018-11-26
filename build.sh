#!/bin/sh

top_dir=$(dirname "$(realpath "$0")")
project_dir="${top_dir}/tag_highlight"
bin_dir="${top_dir}/bin"
system_type=

################################################################################

die()
{
    if [ $# -eq 0 ]; then
        exit 1
    elif [ $# -eq 1 ]; then
        echo "Fatal error: $1" >&2
        exit 1
    else
        echo "Fatal error: $2" >&2
        exit "$1"
    fi
}

Exists() {
    command -v "$@" >/dev/null 2>&1
}

check() {
    Exists 'cmake' || die 'Cmake not found'    
    Exists 'llvm-config' || die 'llvm not found'    
    Exists 'gcc' || Exists 'clang' || die 'No appropriate compiler found.'
}

guess_system() {
    _output=$(uname)
    case "$_output" in 
        *msys*|*mingw*)
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
    # if echo "$_output" | grep -Eq '(msys|mingw)'; then
    #     echo 'MinGW'
    # elif echo "$_output" | grep -Eq 'FreeBSD'; then
    #     echo 'FreeBSD'
    # elif echo "$_output" | grep -Eq 'Linux'; then
    #     echo 'linux'
    # else
    #     die 'Unable to identify system type'
    # fi
}

################################################################################

init_directories()
{
    mkdir -p "${top_dir}/autoload/tag_highlight"

    case "$system_type" in
        MinGW)
            binary_path="${project_dir}/build/src/tag_highlight.exe"
            final_path="${bin_dir}/tag_highlight.exe"
            ;;
        *)
            binary_path="${project_dir}/build/src/tag_highlight"
            final_path="${bin_dir}/tag_highlight"
            ;;
    esac

    cat > "${top_dir}/autoload/tag_highlight/install_info.vim" <<EOF
function! tag_highlight#install_info#Get_Binary_Name()
    return '${final_path}'
endfunction
EOF
}

compile()
{
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
        -DCMAKE_C_COMPILER="${1:-gcc}" -G "${cmake_make_system}" .. \
        || die 'Cmake configuration failed.'

    num_jobs=$(nproc)
    [ "$num_jobs" ] || num_jobs=4

    make -j "$num_jobs" || die 'Compilation failed'
}

install()
{
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

            $link_cmd '/mingw64/bin/libwinpthread.dll' "${bin_dir}"
            $link_cmd '/mingw64/bin/libz.dll' "${bin_dir}"
            $link_cmd '/mingw64/bin/libstdc++.dll' "${bin_dir}"
            $link_cmd "/mingw64/bin/libclang.dll" "${bin_dir}"
            ;;
    esac

    if [ -z "$binary_path" ] || [ -z "${final_path}" ]; then
        die 'Invalid'
    fi

    mv "${binary_path}" "${bin_dir}"
    rm -r "${project_dir}/build"

}

################################################################################

system_type=$(guess_system)
init_directories
compile "$@"
install
echo 'Done!' >&2
