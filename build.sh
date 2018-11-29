#!/bin/sh

top_dir=$(dirname "$(realpath "$0")")
project_dir="${top_dir}/tag_highlight"
bin_dir="${top_dir}/bin"
system_type=

################################################################################

die()
{
    if [ $# -eq 0 ]; then
        echo "\033[1;32mFatal error encountered.\033[0m"
    else
        echo "\033[1;32mFatal error\033[0m: $1" >&2
    fi
    exit 1
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
        *MSYS*|*MINGW*)
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
            final_path=$(cygpath -wa "${bin_dir}/tag_highlight.exe")
            binary_path_win32=$(cygpath -wa "${bin_dir}")
            ;;
        *)
            binary_path="${project_dir}/build/src/tag_highlight"
            final_path="${bin_dir}/tag_highlight"
            binary_path_win32="${bin_dir}"
            ;;
    esac

    cat > "${top_dir}/autoload/tag_highlight/install_info.vim" <<EOF
function! tag_highlight#install_info#Get_Binary_Name()
    return '${final_path}'
endfunction
function! tag_highlight#install_info#Get_Binary_Path()
    return '${binary_path_win32}'
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

            for libname in libclang.dll libgcc_s_seh-1.dll libgomp-1.dll libstdc++-6.dll libwinpthread-1.dll libz3.dll zlib1.dll
            do
                _command_="$link_cmd '$(cygpath -wa "/mingw64/bin/${libname}")' '$(cygpath -wa "${bin_dir}")'"
                /usr/bin/printf "%s\n" "$_command_" >&2
                eval $_command_
            done
            ;;
    esac

    if [ -z "$binary_path" ] || [ -z "${final_path}" ]; then
        die 'Invalid'
    fi

    /usr/bin/printf "mv '%s' '%s'\n" "${binary_path}" "${bin_dir}" >&2
    mv "${binary_path}" "${bin_dir}"
    rm -r "${project_dir}/build"

}

################################################################################

system_type=$(guess_system)
init_directories
compile "$@"
install
echo 'Done!' >&2
