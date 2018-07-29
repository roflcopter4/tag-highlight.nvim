if exists('g:thingything#loaded')
    finish
endif

"===============================================================================

function! s:WriteFifo(msg)
    if a:msg && s:pipe
        call writefile([a:msg], s:pipe, '')
    endif
endfunction

"===============================================================================

let s:errdata = ''
function! s:OnStderr(job_id, data, event) dict
    for l:str in a:data
        if len(l:str) && l:str !=# ' '
            "let s:errdata .= l:str
            echom l:str
        endif
    endfor
    "echom s:errdata
    "if s:errdata =~# '\r'
    "    echom s:errdata
    "    let s:errdata = ''
    "endif
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

"===============================================================================

function! s:NewBuf()
    sleep 50m
    if s:job1 !=# 0
        let l:buf = nvim_get_current_buf()
        "if nvim_get_current_buf() ==# 1 && s:init < 2
        "    let s:init = 2
        "else
        if index(s:new_bufs, l:buf) == (-1)
            call rpcnotify(s:job1, 'vim_event_update', 'A')
            call add(s:new_bufs, l:buf)
        endif
    endif
endfunction

function! s:UpdateTags()
    sleep 50m
    if s:job1 !=# 0
        call rpcnotify(s:job1, 'vim_event_update', 'B')
    endif
endfunction

function! s:StopTagHighlight()
    if s:job1 != 0
        call rpcnotify(s:job1, 'vim_event_update', 'C')
    endif
endfunction

function! s:BufChanged()
    sleep 50m
    let l:buf = nvim_get_current_buf()

    "if s:init == 2
    "    let s:init = 3
    "elseif index(s:seen_bufs, l:buf) ==# (-1)
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
    sleep 50m
    if s:init && s:job1 !=# 0
        call rpcnotify(s:job1, 'vim_event_update', 'E')
    endif
endfunction

"===============================================================================

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

"let s:rpc  = { 'rpc': v:true, 'on_exit': function('s:OnExit') }

function! s:InitTagHighlight()
    let s:job1 = 0
    let s:pipe = 0
    let s:chid = 0
    let s:sock = 0
    let s:init = 0
    let l:cur  = nvim_get_current_buf()
    let s:seen_bufs = [l:cur]
    let s:new_bufs = [l:cur]

    "let s:pipe = tempname()
    "call system('mkfifo ' . shellescape(s:pipe))
    " let s:pipe = serverstart()
    "let s:chid = sockconnect('pipe', s:pipe, {'rpc': v:true})

    if filereadable(expand('~/.vim_tags/bin/tag_highlight'))
        let l:binary = expand('~/.vim_tags/bin/tag_highlight')
    elseif filereadable(expand('~/.vim_tags/bin/tag_highlight.exe'))
        let l:binary = expand('~/.vim_tags/bin/tag_highlight.exe')
    else
        echom 'Failed to find binary'
        return
    endif
    echom 'Opening ' . l:binary . ' with pipe ' . s:pipe
    
    let s:job1 = jobstart(
                \     [l:binary, s:pipe],
                \     s:rpc
                \ )

    sleep 500m " Don't do anything until we're sure everything's finished initializing
    let s:init = 1
endfunction

"===============================================================================

command! InitTagHighlight call s:InitTagHighlight()
command! StopTagHighlight call s:StopTagHighlight()
command! TagHighlightClear call s:ClearBuffer()

if exists('g:tag_highlight#enabled') && g:tag_highlight#enabled
    augroup Tag_Highlight_Init
        autocmd VimEnter * call s:InitTagHighlight()
    augroup END
endif

augroup TagHighlightAu
    autocmd BufAdd * call s:NewBuf()
    autocmd BufWritePost * call s:UpdateTags()
    autocmd BufEnter * call s:BufChanged()
augroup END

let g:thingything#loaded = 1
