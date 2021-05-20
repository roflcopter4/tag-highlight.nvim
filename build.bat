@echo off
setlocal


set SRCDIR=%CD%\tag-highlight

REM Really gotta admire the dedication to inconsistency.
where /Q sh
if !errorlevel! EQU 0 (
    set PATH=!PATH:C:\Program Files\Git\usr\bin=!

    where /Q sh
    if !errorlevel! EQU 0 (
        echo Error: cannot have a UNIX shell in your PATH >&2
        goto leave
    )
)

rem echo "!PATH!"

if defined CC (
    where /Q !CC!
    if errorlevel 1 (
        goto bad_compiler
    )
) else (
:bad_compiler
    where /Q gcc
    if %errorlevel% GEQ 1 (
        set CC=gcc
        goto found_cc
    )
    where /Q clang
    if %errorlevel% GEQ 1 (
        set CC=clang
        goto found_cc
    )
    where /Q cc
    if %errorlevel% GEQ 1 (
        set CC=cc
        goto found_cc
    )
    
    echo Error: No compatible compiler found >&2
    goto leave
)

:found_cc

mkdir "%CD%\autoload" 2>NUL
mkdir "%CD%\autoload\tag-highlight" 2>NUL
mkdir "%CD%\cache" 2>NUL

echo %SRCDIR%\

if exist "%SRCDIR%\build" (
    if exist "%SRCDIR%\_build_backup" (
        rmdir /Q /S "%SRCDIR%\_build_backup"
    )
    move /Y "%SRCDIR%\build" "%SRCDIR%\_build_backup"
)

mkdir "%SRCDIR%\build"

echo function! tag_highlight#install_info#GetBinaryName()  > "%CD%\autoload\tag-highlight\install_info.vim"
echo     return '%CD%/bin/tag-highlight.exe'              >> "%CD%\autoload\tag-highlight\install_info.vim"
echo endfunction                                          >> "%CD%\autoload\tag-highlight\install_info.vim"

echo function! tag_highlight#install_info#GetBinaryPath() >> "%CD%\autoload\tag-highlight\install_info.vim"
echo     return '%CD%/bin'                                >> "%CD%\autoload\tag-highlight\install_info.vim"
echo endfunction                                          >> "%CD%\autoload\tag-highlight\install_info.vim"

echo function! tag_highlight#install_info#GetCachePath()  >> "%CD%\autoload\tag-highlight\install_info.vim"
echo     return '%CD%/cache'                              >> "%CD%\autoload\tag-highlight\install_info.vim"
echo endfunction                                          >> "%CD%\autoload\tag-highlight\install_info.vim"

set OutputDir=%CD%\bin
cd %SRCDIR%\build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=YES "-DCMAKE_C_COMPILER=%CC%" -G "MinGW Makefiles" ..

if %errorlevel% GEQ 1 goto leave
mingw32-make -j%NUMBER_OF_PROCESSORS% -C "%SRCDIR%\build"
if %errorlevel% GEQ 1 goto leave

move /Y "%SRCDIR%\build\src\tag-highlight.exe" "%OutputDir%\tag-highlight.exe"
cd ..
rmdir /Q /S "%SRCDIR%\build"

:leave

endlocal
