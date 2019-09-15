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
    let g:tag_highlight#ignored_tags = { 'c': ['bool', 'static_assert', '__attribute__', 'TRUE', 'FALSE'] }
    let g:tag_highlight#ignored_tags['cpp'] = g:tag_highlight#ignored_tags['c']
endif
call s:InitVar('ignored_dirs', [])

call s:InitVar('directory',     tag_highlight#install_info#GetCachePath())
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
                \     'c':   ['cConstant', 'cStorageClass', 'cType', 'cStatement'],
                \     'cpp': ['cConstant', 'cStorageClass', 'cType', 'cStatement',
                \             'cppStorageClass', 'cppType',],
                \ })
 
call s:InitVar('norecurse_dirs', [
                \ expand('~'),
                \ '/',
                \ '/lib',
                \ '/include',
                \ '/usr/lib/',
                \ '/usr/share',
                \ '/usr/include',
                \ '/usr/local/lib',
                \ '/usr/local/share',
                \ '/usr/local/include',
                \ 'C:/'
                \ ])

if g:tag_highlight#run_ctags
    let s:ctags_bin = 0

    function! s:test_ctags(str)
        return executable(a:str) && system(a:str.' --version') =~# 'Universal'
    endfunction

    if s:test_ctags(g:tag_highlight#ctags_bin)
        let s:ctags_bin = g:tag_highlight#ctags_bin
    elseif s:test_ctags('ctags')
        let s:ctags_bin = 'ctags'
    elseif s:test_ctags('uctags')
        let s:ctags_bin = 'uctags'
    elseif s:test_ctags('/usr/local/bin/ctags')
        let s:ctags_bin = '/usr/local/bin/ctags'
    elseif s:test_ctags('/usr/bin/ctags')
        let s:ctags_bin = '/usr/bin/ctags'
    elseif s:test_ctags('/opt/bin/ctags')
        let s:ctags_bin = '/opt/bin/ctags'
    elseif s:test_ctags(expand('~/.local/bin/ctags'))
        let s:ctags_bin = expand('~/.local/bin/ctags')
    else
        unlet s:ctags_bin
    endif

    if exists('s:ctags_bin')
        let g:tag_highlight#ctags_bin = s:ctags_bin
        call s:InitVar('ctags_args', [
                \   '--fields=+l',
                \   '--c-kinds=+px',
                \   '--c++-kinds=+px',
                \   '--sort=yes',
                \   '--regex-go=/^\s*(var)?\s*(\w*)\s*:?=\s*func/\2/f/',
                \   '--languages=-Pod',
                \ ])
    else
        let g:tag_highlight#enabled = 0
        unlet g:tag_highlight#ctags_bin
        echohl ErrorMsg
        echom 'tag_highlight: Universal Ctags not found, cannot run ctags.'
        echohl None
        finish
    endif
endif

if g:tag_highlight#run_ctags && g:tag_highlight#no_autoconf == 1
    call extend(g:tag_highlight#ctags_args, [
                \   '--exclude=*Makefile',
                \   '--exclude=*Makefile.in',
                \   '--exclude=*aclocal.m4',
                \   '--exclude=*config.guess',
                \   '--exclude=*config.h.in',
                \   '--exclude=*config.log',
                \   '--exclude=*config.status',
                \   '--exclude=*configure',
                \   '--exclude=*depcomp',
                \   '--exclude=*install-sh',
                \   '--exclude=*missing',
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

call s:InitVar('allbut', '.*Comment.*,.*String.*,.*Quote.*')

" }}}

if !isdirectory(g:tag_highlight#directory)
    call mkdir(g:tag_highlight#directory)
endif

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

func! s:Do_Add(path, str)
    echom 'Adding project dir "' . a:path . '" for language `' . &filetype . "'"
    call writefile([a:str], g:tag_highlight#settings_file, 'a')
endf

function! s:Add_Project(path)
    let l:str = a:path . "\t" . &filetype

    if file_readable(g:tag_highlight#settings_file)
        let l:lines = readfile(g:tag_highlight#settings_file)
        if index(l:lines, l:str) ==# (-1)
            call s:Do_Add(a:path, l:str)
        else
            echoerr 'Project dir "' . a:path . '" for language `' . &filetype . "' is already recorded."
        endif
    else
        call s:Do_Add(a:path, l:str)
    endif
endfunction

function! s:Index_Path_And_Lang(lines, path)
    let l:lst   = []
    let l:index = (-1)

    if &filetype ==# 'c'
        let l:lst = [a:path . "\tc", a:path . "\tcpp"]
    elseif &filetype ==# 'cpp'
        let l:lst = [a:path . "\tcpp", a:path . "\tc"]
    else
        let l:lst = [a:path . "\t" . &filetype]
    endif

    for l:str in l:lst
        let l:index = index(a:lines, l:str)
        if l:index !=# (-1)
            break
        endif
    endfor

    return l:index
endfunction

function! s:Remove_Project(path)
    if file_readable(g:tag_highlight#settings_file)
        let l:lines = readfile(g:tag_highlight#settings_file)
        
        for l:ln in l:lines
            let l:ln = substitute(l:ln, "\n\|\r", '', '')
        endfor

        let l:index = s:Index_Path_And_Lang(l:lines, a:path)

        if l:index !=# (-1)
            echom 'Removing ' . &filetype . ' language project dir "'.a:path.'"'
            call remove(l:lines, l:index)
            call writefile(l:lines, g:tag_highlight#settings_file)
        else
            echoerr 'Project dir "' . a:path . '" for language `' . &filetype . "' is not recorded."
        endif
    endif
endfunction

command! -nargs=? -complete=file THLAddProject call s:Add_Remove_Project(0, <q-args>)
command! -nargs=? -complete=file THLRemoveProject call s:Add_Remove_Project(1, <q-args>)

runtime! plugin/Tag_Highlight/*.vim

" command! tag_highlightToggle call Tag_HighlightToggle()
" command! tag_highlightVerbosity call tag_highlight#Toggle_Verbosity()
" command! tag_highlightBinaryToggle call tag_highlight#Toggle_C_Binary()

" nnoremap <unique> <Plug>tag_highlightToggle :call tag_highlightToggle()<CR>
" nmap <silent> <leader>tag <Plug>tag_highlightToggle

"============================================================================= 
highlight def link tag_highlight_TemplateTag		tag_highlight_ClassTag
highlight def link tag_highlight_ClassTag		tag_highlight_TypeTag
highlight def link tag_highlight_EnumTypeTag		tag_highlight_TypeTag
highlight def link tag_highlight_StructTag		tag_highlight_TypeTag
highlight def link tag_highlight_UnionTag		tag_highlight_TypeTag
highlight def link tag_highlight_MethodTag		tag_highlight_FunctionTag
highlight def link tag_highlight_VariableTag		tag_highlight_ObjectTag
highlight def link tag_highlight_FieldTag		tag_highlight_MemberTag
highlight def link tag_highlight_NamespaceTag		tag_highlight_ModuleTag

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

"============================================================================= 
" Shim for C code.
"============================================================================= 

function! s:OnStderr(job_id, data, event) dict
    for l:str in a:data
        if len(l:str) && l:str !=# ' '
            echom l:str
            " if g:tag_highlight#verbose
                try
                    call writefile([l:str], expand(g:tag_highlight#directory . '/stderr.log'), 'a')
                endtry
            " endif
        endif
    endfor
endfunction

function! s:OnExit(job_id, data, event) dict
    echom 'Closing channel.'
    let g:tag_highlight#pid = 0
    let s:sock = 0
    let s:pipe = 0
    let s:seen_bufs = []
    let s:new_bufs  = []
endfunction

function! s:Wait()
    sleep 25m
endfunction

"===============================================================================
"
let s:msg_types = {
            \         'BufNew':          0,
            \         'BufChanged':      1,
            \         'SyntaxChanged':   2,
            \         'UpdateTags':      3,
            \         'UpdateTagsForce': 4,
            \         'ClearBuffer':     5,
            \         'Stop':            6,
            \     }

function! s:NewBuf()
    " call s:Wait()
    if g:tag_highlight#pid >=# 0
        let l:buf = nvim_get_current_buf()
        if index(s:new_bufs, l:buf) == (-1)
            call rpcnotify(g:tag_highlight#pid, 'vim_event_update', s:msg_types['BufNew'])
            call add(s:new_bufs, l:buf)
        endif
    endif
endfunction

function! s:BufChanged()
    " call s:Wait()
    let l:buf = nvim_get_current_buf()
    if index(s:new_bufs, l:buf) == (-1)
        call rpcnotify(g:tag_highlight#pid, 'vim_event_update', s:msg_types['BufNew'])
        call add(s:new_bufs, l:buf)
        call add(s:seen_bufs, l:buf)
    elseif index(s:seen_bufs, l:buf) ==# (-1)
        call add(s:seen_bufs, l:buf)
    elseif g:tag_highlight#pid >=# 0
        call rpcnotify(g:tag_highlight#pid, 'vim_event_update', s:msg_types['BufChanged'])
    endif
endfunction

function! s:UpdateTags()
    if g:tag_highlight#pid !=# 0
        call rpcnotify(g:tag_highlight#pid, 'vim_event_update', s:msg_types['UpdateTags'])
    endif
endfunction

function! s:ForceUpdateTags()
    if g:tag_highlight#pid !=# 0
        call rpcnotify(g:tag_highlight#pid, 'vim_event_update', s:msg_types['UpdateTagsForce'])
    endif
endfunction

function! s:ClearBuffer()
    if g:tag_highlight#pid >=# 0
        call rpcnotify(g:tag_highlight#pid, 'vim_event_update', s:msg_types['ClearBuffer'])
    endif
endfunction

" function! s:ModeChange()
"     if g:tag_highlight#pid >=# 0
"         call rpcnotify(g:tag_highlight#pid, 'vim_event_update', 'M')
"     endif
" endfunction

function! s:DeleteBuf()
    let l:buf = nvim_get_current_buf()
    let l:ind = index(s:new_bufs, l:buf)
    if l:ind !=# (-1)
        call remove(s:new_bufs, l:ind)
    endif

    let l:ind = index(s:seen_bufs, l:buf)
    if l:ind !=# (-1)
        call remove(s:seen_bufs, l:ind)
    endif
endfunction

function! s:StopTagHighlight()
    if g:tag_highlight#pid >= 0
        call rpcnotify(g:tag_highlight#pid, 'vim_event_update', s:msg_types['Stop'])
    endif
endfunction

function! s:ExitKill()
    if exists('g:tag_highlight#binpid') 
        echom system('kill -INT ' . g:tag_highlight#binpid) 
    endif
endfunction

"===============================================================================

function! s:getchan(job_id, data, event) dict
    echo a:data
    let l:pnam = ''
    for l:str in a:data
        if len(l:str) && l:str !=# ' '
            let l:pnam .= l:str
        endif
    endfor
    if l:pnam ==# '' || l:pnam ==# ' '
        return
    endif
    echom 'Connecting to ' . l:pnam
    let g:tag_highlight#pid = sockconnect('pipe', l:pnam, {'rpc': v:true})
endfunction

let g:tag_highlight#pid = 0
let s:pipe = 0
let s:chid = 0
let s:sock = 0
let s:seen_bufs = []
let s:new_bufs = []

let s:rpc  = {  'rpc':       v:true, 
            \   'on_stderr': function('s:OnStderr'),
            \   'on_exit':   function('s:OnExit'),
            \ }

function! s:InitTagHighlight()
    let s:chid = 0
    let s:sock = 0
    let l:cur  = nvim_get_current_buf()
    let s:seen_bufs = [l:cur]
    let s:new_bufs = [l:cur]

    try
        call system('rm -f '.expand('~/.tag_highlight_log/stderr.log'))
    endtry
        
    let l:binary = tag_highlight#install_info#GetBinaryName()
    echom 'Opening ' . l:binary . ' with pipe ' . s:pipe
    let g:tag_highlight#pid = jobstart([l:binary], s:rpc)
    
    sleep 500m " Don't do anything until we're sure everything's finished initializing
endfunction

"===============================================================================

command! THLInit call s:InitTagHighlight()
command! THLStop call s:StopTagHighlight()
command! THLClear call s:ClearBuffer()
command! THLUpdate call s:ForceUpdateTags()

command! TestExitKill call s:ExitKill()

if exists('g:tag_highlight#enabled') && g:tag_highlight#enabled
    augroup Tag_Highlight_Init
        autocmd VimEnter * call s:InitTagHighlight()
    augroup END
endif

augroup TagHighlightAu
    autocmd BufAdd * call s:NewBuf()
    " autocmd BufRead * call s:NewBuf()
    autocmd BufWritePost * call s:UpdateTags()
    autocmd BufEnter * call s:BufChanged()
    autocmd BufDelete * call s:DeleteBuf()
    autocmd VimLeavePre * call s:ExitKill()
augroup END

"===============================================================================

" call s:InitTagHighlight()
let g:tag_highlight#loaded = 1

" vim:fdm=marker
