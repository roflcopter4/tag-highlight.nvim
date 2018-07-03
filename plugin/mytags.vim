" ============================================================================
" File:        mytags.vim
" Author:      Christian Persson <c0r73x@gmail.com>
" Repository:  https://github.com/c0r73x/mytags.nvim
"              Released under the MIT license
" ============================================================================

" finish

" Options {{{
if exists('g:mytags#loaded')
    finish
endif

function! InitVar(varname, val)
    let l:varname = 'g:mytags#' . a:varname
    if !exists(l:varname)
        execute 'let ' . l:varname . ' = ' . string(a:val)
    endif
endfunction

call InitVar('file', '')
call InitVar('ctags_bin', 'ctags')

call InitVar('ignored_tags', {})
call InitVar('ignored_dirs', [])

call InitVar('directory',     expand('~/.vim_tags'))
call InitVar('bin',           expand(g:mytags#directory . '/bin/mytags'))
call InitVar('settings_file', expand(g:mytags#directory . '/mytags.txt'))

if file_readable(g:mytags#bin)
    call InitVar('use_binary',  1)
else
    call InitVar('use_binary',  0)
endif

call InitVar('use_compression',   1)
call InitVar('compression_level', 9)
call InitVar('compression_type', 'gzip')

call InitVar('enabled',     1)
call InitVar('appendpath',  1)
call InitVar('find_tool',   0)
call InitVar('highlight',   1)
call InitVar('no_autoconf', 1)
call InitVar('recursive',   1)
call InitVar('run_ctags',   1)
call InitVar('verbose',     1)
call InitVar('strip_comments', 1)
call InitVar('silent_timeout', 0)
call InitVar('ctags_timeout',  240)
call InitVar('patternlength',  2048)

" People often make annoying #defines for C and C++ keywords, types, etc. Avoid
" highlighting these by default, leaving the built in vim highlighting intact.
call InitVar('restored_groups', {
                \     'c':   ['cConstant', 'cStorageClass', 'cConditional', 'cRepeat', 'cType'],
                \     'cpp': ['cConstant', 'cStorageClass', 'cConditional', 'cRepeat', 'cType',
                \             'cppStorageClass', 'cppType'],
                \ })
 
call InitVar('norecurse_dirs', [
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

call InitVar('events_update', [
                \   'BufWritePost'
                \ ])

call InitVar('events_highlight', [
                \   'BufReadPre',
                \   'BufEnter',
                \ ])

call InitVar('events_rehighlight', [
                \   'FileType',
                \   'Syntax',
                \ ])

if g:mytags#run_ctags
    let s:found = 0

    if executable(g:mytags#ctags_bin) && system(g:mytags#ctags_bin . ' --version') =~# 'Universal'
        let s:found = 1
    elseif executable('ctags') && system('ctags --version') =~# 'Universal'
        let g:mytags#ctags_bin = 'ctags'
        let s:found = 1
    elseif executable('uctags') && system('uctags --version') =~# 'Universal'
        let g:mytags#ctags_bin = 'uctags'
        let s:found = 1
    endif

    if s:found
        call InitVar('ctags_args', [
                \   '--fields=+l',
                \   '--c-kinds=+p',
                \   '--c++-kinds=+p',
                \   '--sort=yes',
                \   "--exclude='.mypy_cache'",
                \   '--regex-go=''/^\s*(var)?\s*(\w*)\s*:?=\s*func/\2/f/'''
                \ ])
    else
        echohl ErrorMsg
        echom 'Mytags: Universal Ctags not found, cannot run ctags.'
        echohl None
        let g:mytags#run_ctags = 0
    endif
endif

if g:mytags#run_ctags && g:mytags#no_autoconf == 1
    call extend(g:mytags#ctags_args, [
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

call InitVar('global_notin', [
                \   '.*String.*',
                \   '.*Comment.*',
                \   'cIncluded',
                \   'cCppOut2',
                \   'cCppInElse2',
                \   'cCppOutIf2',
                \   'pythonDocTest',
                \   'pythonDocTest2',
                \ ])

call InitVar('ignore', [
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

call InitVar('ft_conv', {
                \   "C++": 'cpp',
                \   'C#': 'cs',
                \   'Sh': 'zsh',
                \ })

if !isdirectory(g:mytags#directory)
    call mkdir(g:mytags#directory)
endif

if !isdirectory(g:mytags#directory . '/bin')
    call mkdir(g:mytags#directory . '/bin')
endif

" }}}

runtime! plugin/MyTags/*.vim

let g:mytags#loaded = 1

if v:vim_did_enter
    call MytagsInit()
else
    augroup MyTags
        autocmd VimEnter * call MytagsInit()
    augroup END
endif

function! s:Add_Remove_Project(operation, ctags, ...)
    if exists('a:1')
        let l:path = a:1
    else
        let l:path = getcwd()
    endif
    if a:operation ==# 0
        call MytagsRemoveProject(a:ctags, l:path)
    elseif a:operation ==# 1
        call MytagsAddProject(a:ctags, l:path)
    endif
endfunction

command! -nargs=1 -complete=file MytagsAddProject call MytagsAddProject(<f-args>)
command! -nargs=1 -complete=file MytagsRemoveProject call MytagsRemoveProject(<f-args>)
command! -nargs=? -complete=file MytagsToggleProject call s:Add_remove_Project(2, <q-args>)
command! -nargs=? -complete=file MytagsAddProject call s:Add_Remove_Project(1, <q-args>, 1)
command! -nargs=? -complete=file MytagsAddProjectNoCtags call s:Add_Remove_Project(1, <q-args>, 0)
command! -nargs=? -complete=file MytagsRemoveProject call s:Add_Remove_Project(0, <q-args>, 0)
command! MytagsToggle call MytagsToggle()
command! MytagsVerbosity call mytags#Toggle_Verbosity()
command! MytagsBinaryToggle call mytags#Toggle_C_Binary()

nnoremap <unique> <Plug>MytagsToggle :call MytagsToggle()<CR>
nmap <silent> <leader>tag <Plug>MytagsToggle


"============================================================================= 


highlight def link mytags_ClassTag		mytags_TypeTag
highlight def link mytags_EnumTypeTag		mytags_TypeTag
highlight def link mytags_StructTag		mytags_TypeTag
highlight def link mytags_UnionTag		mytags_TypeTag
highlight def link mytags_MethodTag		mytags_FunctionTag
highlight def link mytags_VariableTag		mytags_ObjectTag
highlight def link mytags_FieldTag		mytags_MemberTag

highlight def link mytags_GlobalVarTag		PreCondit
highlight def link mytags_ConstantTag		Constant
highlight def link mytags_EnumTag		Define
highlight def link mytags_FunctionTag		Function
highlight def link mytags_InterfaceTag		Identifier
highlight def link mytags_MemberTag		Identifier
highlight def link mytags_ObjectTag		Identifier
highlight def link mytags_ModuleTag		PreProc
highlight def link mytags_PreProcTag		PreProc
highlight def link mytags_TypeTag		Type


" vim:fdm=marker
