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

function! s:OnStderr(job_id, data, event) dict
    for l:str in a:data
        if len(l:str) && l:str !=# ' '
            echom string(l:str)
        endif
    endfor
endfunction

function! s:OnExit(job_id, data, event) dict
    echom 'Closing channel.'

    let s:job1 = 0
    let s:sock = 0
    let s:pipe = 0
    let s:init = 0
endfunction

"===============================================================================

function! s:NewBuf()
    sleep 50m
    if s:job1 !=# 0
        call writefile(['A'], s:pipe, '')
    endif
endfunction

function! s:UpdateTags()
    sleep 50m
    if s:job1 !=# 0
        call writefile(['B'], s:pipe, '')
    endif
endfunction

function! s:StopTagHighlight()
    if s:job1 != 0
        call writefile(['C'], s:pipe, '')
    endif
endfunction

function! s:BufChanged()
    sleep 50m
    let l:buf = nvim_get_current_buf()

    if index(s:seen_bufs, l:buf) ==# (-1)
        call add(s:seen_bufs, l:buf)
    elseif s:init && s:job1 !=# 0
        call writefile(['D'], s:pipe, '')
    endif
endfunction

function! s:ClearBuffer()
    sleep 50m
    if s:init && s:job1 !=# 0
        call writefile(['E'], s:pipe, '')
    endif
endfunction

"===============================================================================

let s:job1 = 0
let s:pipe = 0
let s:sock = 0
let s:init = 0
let s:seen_bufs = [1]
let s:rpc  = {  'rpc':       v:true, 
            \   'on_stderr': function('s:OnStderr'),
            \   'on_exit':   function('s:OnExit'),
            \ }

function! s:InitTagHighlight()
    let s:pipe = tempname()
    call system('mkfifo ' . shellescape(s:pipe))
    let s:job1 = jobstart(
                \     ['/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/mpack_ct/thing', s:pipe],
                \     s:rpc
                \ )

    sleep 1500m " Don't do anything until we're sure everything's finished initializing
    let s:init = 1
endfunction

"===============================================================================

command! InitTagHighlight call s:InitTagHighlight()
command! StopTagHighlight call s:StopTagHighlight()
command! TagHighlightClear call s:ClearBuffer()

augroup KillMe
    autocmd BufAdd * call s:NewBuf()
    autocmd BufWritePost * call s:UpdateTags()
    autocmd BufEnter * call s:BufChanged()
augroup END


let g:thingything#loaded = 1
