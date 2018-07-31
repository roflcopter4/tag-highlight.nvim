" ============================================================================
" File:        tag_highlight.vim
" Author:      Christian Persson <c0r73x@gmail.com>
" Repository:  https://github.com/c0r73x/tag_highlight.nvim
"              Released under the MIT license
" ============================================================================

" Options {{{
if exists('g:tag_highlight#loaded')
    finish
endif

function! s:InitVar(varname, val)
    let l:varname = 'g:tag_highlight#' . a:varname
    if !exists(l:varname)
        execute 'let ' . l:varname . ' = ' . string(a:val)
    endif
endfunction

call s:InitVar('file', '')
call s:InitVar('ctags_bin', 'ctags')

if !exists('g:tag_highlight#ignored_tags')
    let g:tag_highlight#ignored_tags = { 'c': ['_Bool', 'static_assert', '__attribute__', 'TRUE', 'FALSE'] }
    let g:tag_highlight#ignored_tags['cpp'] = g:tag_highlight#ignored_tags['c']
endif
call s:InitVar('ignored_dirs', [])

call s:InitVar('directory',     expand('~/.vim_tags'))
call s:InitVar('bin',           expand(g:tag_highlight#directory . '/bin/tag_highlight'))
call s:InitVar('settings_file', expand(g:tag_highlight#directory . '/tag_highlight.txt'))
call s:InitVar('use_compression',   1)
call s:InitVar('compression_level', 9)
call s:InitVar('compression_type', 'gzip')
call s:InitVar('enabled',     1)
call s:InitVar('no_autoconf', 1)
call s:InitVar('recursive',   1)
call s:InitVar('verbose',     1)

" People often make annoying #defines for C and C++ keywords, types, etc. Avoid
" highlighting these by default, leaving the built in vim highlighting intact.
call s:InitVar('restored_groups', {
                \     'c':   ['cConstant', 'cStorageClass', 'cConditional', 'cRepeat', 'cType', 'cStatement'],
                \     'cpp': ['cConstant', 'cStorageClass', 'cConditional', 'cRepeat', 'cType', 'cStatement',
                \             'cppStorageClass', 'cppType',],
                \ })
 
call s:InitVar('norecurse_dirs', [
                \ $HOME,
                \ '/',
                \ '/lib',
                \ '/include',
                \ '/usr/lib/',
                \ '/usr/share',
                \ '/usr/include',
                \ '/usr/local/lib',
                \ '/usr/local/share',
                \ '/usr/local/include',
                \ ])

if g:tag_highlight#run_ctags
    let s:found = 0

    if executable(g:tag_highlight#ctags_bin) && system(g:tag_highlight#ctags_bin . ' --version') =~# 'Universal'
        let s:found = 1
    elseif executable('ctags') && system('ctags --version') =~# 'Universal'
        let g:tag_highlight#ctags_bin = 'ctags'
        let s:found = 1
    elseif executable('uctags') && system('uctags --version') =~# 'Universal'
        let g:tag_highlight#ctags_bin = 'uctags'
        let s:found = 1
    endif

    if s:found
        call s:InitVar('ctags_args', [
                \   '--fields=+l',
                \   '--c-kinds=+px',
                \   '--c++-kinds=+px',
                \   '--sort=yes',
                \   "--exclude='.mypy_cache'",
                \   '--regex-go=''/^\s*(var)?\s*(\w*)\s*:?=\s*func/\2/f/''',
                \   '--languages=-Pod',
                \ ])
    else
        echohl ErrorMsg
        echom 'tag_highlight: Universal Ctags not found, cannot run ctags.'
        echohl None
        let g:tag_highlight#run_ctags = 0
    endif
endif

if g:tag_highlight#run_ctags && g:tag_highlight#no_autoconf == 1
    call extend(g:tag_highlight#ctags_args, [
                \   "--exclude='*Makefile'",
                \   "--exclude='*Makefile.in'",
                \   "--exclude='*aclocal.m4'",
                \   "--exclude='*config.guess'",
                \   "--exclude='*config.h.in'",
                \   "--exclude='*config.log'",
                \   "--exclude='*config.status'",
                \   "--exclude='*configure'",
                \   "--exclude='*depcomp'",
                \   "--exclude='*install-sh'",
                \   "--exclude='*missing'",
                \])
endif

call s:InitVar('ignore', [
                \   'cfg',
                \   'conf',
                \   'help',
                \   'mail',
                \   'markdown',
                \   'nerdtree',
                \   'nofile',
                \   'qf',
                \   'text',
                \   'man',
                \   'preview',
                \   'fzf',
                \ ])

call s:InitVar('global_notin', [
                \   '.*String.*',
                \   '.*Comment.*',
                \   'cIncluded',
                \   'cCppOut2',
                \   'cCppInElse2',
                \   'cCppOutIf2',
                \   'pythonDocTest',
                \   'pythonDocTest2',
                \ ])

call s:InitVar('ft_conv', {
                \   "C++": 'cpp',
                \   'C#': 'cs',
                \   'Sh': 'zsh',
                \ })

if !isdirectory(g:tag_highlight#directory)
    call mkdir(g:tag_highlight#directory)
endif

if !isdirectory(g:tag_highlight#directory . '/bin')
    call mkdir(g:tag_highlight#directory . '/bin')
endif

" }}}

runtime! plugin/Tag_Highlight/*.vim

let g:tag_highlight#loaded = 1

function! s:Add_Remove_Project(operation, ...)
    if exists('a:1')
        let l:path = a:1
    else
        let l:path = getcwd()
    endif
    let l:path = resolve(fnamemodify(expand(l:path), ':p:h'))

    if a:operation ==# 0
        call s:Add_Project(l:path)
    elseif a:operation ==# 1
        call s:Remove_Project(l:path)
    endif
endfunction

function! s:Add_Project(path)
    if file_readable(g:tag_highlight#settings_file)
        let l:lines = readfile(g:tag_highlight#settings_file)
        if index(l:lines, a:path) ==# (-1)
            echom 'Adding project dir "'.a:path.'"'
            call writefile([a:path], g:tag_highlight#settings_file, 'a')
        endif
    else
        echom 'Adding project dir "'.a:path.'"'
        call writefile([a:path], g:tag_highlight#settings_file)
    endif
endfunction

function! s:Remove_Project(path)
    if file_readable(g:tag_highlight#settings_file)
        let l:lines = readfile(g:tag_highlight#settings_file)
        let l:index = index(l:lines, a:path)
        if l:index !=# (-1)
            echom 'Removing project dir "'.a:path.'"'
            call remove(l:lines, (l:index - 1))
            call writefile(l:lines, g:tag_highlight#settings_file)
        endif
    endif
endfunction

command! -nargs=? -complete=file TagHighlightAddProject call s:Add_Remove_Project(0, <q-args>)
command! -nargs=? -complete=file TagHighlightRemoveProject call s:Add_Remove_Project(1, <q-args>)

" command! tag_highlightToggle call Tag_HighlightToggle()
" command! tag_highlightVerbosity call tag_highlight#Toggle_Verbosity()
" command! tag_highlightBinaryToggle call tag_highlight#Toggle_C_Binary()

" nnoremap <unique> <Plug>tag_highlightToggle :call tag_highlightToggle()<CR>
" nmap <silent> <leader>tag <Plug>tag_highlightToggle

"============================================================================= 

highlight def link tag_highlight_ClassTag		tag_highlight_TypeTag
highlight def link tag_highlight_EnumTypeTag		tag_highlight_TypeTag
highlight def link tag_highlight_StructTag		tag_highlight_TypeTag
highlight def link tag_highlight_UnionTag		tag_highlight_TypeTag
highlight def link tag_highlight_MethodTag		tag_highlight_FunctionTag
highlight def link tag_highlight_VariableTag		tag_highlight_ObjectTag
highlight def link tag_highlight_FieldTag		tag_highlight_MemberTag

highlight def link tag_highlight_GlobalVarTag		PreCondit
highlight def link tag_highlight_ConstantTag		Constant
highlight def link tag_highlight_EnumTag		Define
highlight def link tag_highlight_FunctionTag		Function
highlight def link tag_highlight_InterfaceTag		Identifier
highlight def link tag_highlight_MemberTag		Identifier
highlight def link tag_highlight_ObjectTag		Identifier
highlight def link tag_highlight_ModuleTag		PreProc
highlight def link tag_highlight_PreProcTag		PreProc
highlight def link tag_highlight_TypeTag		Type


" vim:fdm=marker
