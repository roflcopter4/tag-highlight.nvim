if exists('g:tag_highlight#shim_loaded')
    finish
endif

"===============================================================================

function! s:OnStderr(job_id, data, event) dict
    for l:str in a:data
        if len(l:str) && l:str !=# ' '
            echom l:str
            call vimproc#write('/home/bml/fuck.log', l:str . "\n", 'a')
        endif
    endfor
endfunction

function! s:OnExit(job_id, data, event) dict
    echom 'Closing channel.'
    let s:job1 = 0
    let s:sock = 0
    let s:pipe = 0
    let s:init = 0
    let s:seen_bufs = []
    let s:new_bufs  = []
endfunction

function! s:Wait()
    sleep 25m
endfunction

"===============================================================================

function! s:NewBuf()
    call s:Wait()
    if s:job1 !=# 0
        let l:buf = nvim_get_current_buf()
        if index(s:new_bufs, l:buf) == (-1)
            call rpcnotify(s:job1, 'vim_event_update', 'A')
            call add(s:new_bufs, l:buf)
        endif
    endif
endfunction

function! s:UpdateTags()
    if s:job1 !=# 0
        call rpcnotify(s:job1, 'vim_event_update', 'B')
    endif
endfunction

function! s:ForceUpdateTags()
    if s:job1 !=# 0
        call rpcnotify(s:job1, 'vim_event_update', 'F')
    endif
endfunction

function! s:StopTagHighlight()
    if s:job1 != 0
        call rpcnotify(s:job1, 'vim_event_update', 'C')
    endif
endfunction

function! s:BufChanged()
    call s:Wait()
    let l:buf = nvim_get_current_buf()
    if index(s:new_bufs, l:buf) == (-1)
        call rpcnotify(s:job1, 'vim_event_update', 'A')
        call add(s:new_bufs, l:buf)
        call add(s:seen_bufs, l:buf)
    elseif index(s:seen_bufs, l:buf) ==# (-1)
        call add(s:seen_bufs, l:buf)
    elseif s:init && s:job1 !=# 0
        call rpcnotify(s:job1, 'vim_event_update', 'D')
    endif
endfunction

function! s:ClearBuffer()
    if s:init && s:job1 !=# 0
        call rpcnotify(s:job1, 'vim_event_update', 'E')
    endif
endfunction

function! s:ModeChange()
    if s:init && s:job1 !=# 0
        call rpcnotify(s:job1, 'vim_event_update', 'I')
    endif
endfunction

function! s:CursorHold()
    if s:init && s:job1 !=# 0
        call rpcnotify(s:job1, 'vim_event_update', 'H')
    endif
endfunction

function! s:ExitKill()
    if exists('g:tag_highlight#binpid') 
        echom system('kill -INT ' . g:tag_highlight#binpid) 
    endif
endfunction

"===============================================================================

let g:tag_highlight#pid = 0
let s:job1 = 0
let s:pipe = 0
let s:chid = 0
let s:sock = 0
let s:init = 0
let s:seen_bufs = []
let s:new_bufs = []

let s:rpc  = {  'rpc':       v:true, 
            \   'on_stderr': function('s:OnStderr'),
            \   'on_exit':   function('s:OnExit'),
            \ }

function! s:InitTagHighlight()
    let s:job1 = 0
    let s:chid = 0
    let s:sock = 0
    let s:init = 0
    let l:cur  = nvim_get_current_buf()
    let s:seen_bufs = [l:cur]
    let s:new_bufs = [l:cur]

    call system('rm /home/bml/fuck.log')

    if filereadable(expand('~/.vim_tags/bin/tag_highlight'))
        let l:binary = expand('~/.vim_tags/bin/tag_highlight')
    elseif filereadable(expand('~/.vim_tags/bin/tag_highlight.exe'))
        let l:binary = expand('~/.vim_tags/bin/tag_highlight.exe')
    else
        echom 'Failed to find binary'
        return
    endif
    echom 'Opening ' . l:binary . ' with pipe ' . s:pipe
    
    let g:tag_highlight#pid = jobstart([l:binary], s:rpc)
    let s:job1 = g:tag_highlight#pid

    sleep 500m " Don't do anything until we're sure everything's finished initializing
    let s:init = 1
endfunction

"===============================================================================

" function! s:FirstInit()
"     call s:InitTagHighlight()
" endfunction

command! TaghighlightInit call s:InitTagHighlight()
command! TaghighlightStop call s:StopTagHighlight()
command! TaghighlightClear call s:ClearBuffer()
command! TaghighlightUpdate call s:ForceUpdateTags()

command! TestExitKill call s:ExitKill()

if exists('g:tag_highlight#enabled') && g:tag_highlight#enabled
    augroup Tag_Highlight_Init
        autocmd VimEnter * call s:InitTagHighlight()
    augroup END
endif

augroup TagHighlightAu
    autocmd BufAdd * call s:NewBuf()
    autocmd BufWritePost * call s:UpdateTags()
    autocmd BufEnter * call s:BufChanged()
    autocmd VimLeavePre * call s:ExitKill()
    autocmd CursorHold,CursorHoldI *.{c,cc,cpp,cxx,h,hh,hpp} call s:CursorHold()
    " autocmd InsertEnter,InsertLeave *.{c,cc,cpp,cxx,h,hh,hpp} call s:ModeChange()
augroup END

let g:tag_highlight#shim_loaded = 1
