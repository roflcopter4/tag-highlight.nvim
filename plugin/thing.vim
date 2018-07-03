if exists('g:thingything#loaded')
    finish
endif

"===============================================================================

function! s:OnStderr(job_id, data, event) dict
    " if a:data[0][0:2] ==# '|||'
    "     let s:sock = a:data[0][3:]
    " endif
    for l:str in a:data
        if len(l:str) && l:str !=# ' '
            echom string(l:str)
        endif
    endfor
endfunction

function! s:OnExit(job_id, data, event) dict
    echom 'Closing channel.'
    " call serverstop(s:sock)
    " call delete(s:pipe)

    let s:job1 = 0
    let s:sock = 0
    let s:pipe = 0
endfunction

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

    call rpcnotify(s:job1, 'cocksucker_event', ['you suck cock'])
endfunction

"===============================================================================

let s:job1 = 0
let s:pipe = 0
let s:sock = 0
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
    " let s:job1 = jobstart(
    "             \     ['/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight/mpack_ct/thing'],
    "             \     s:rpc
    "             \ )
    sleep 500m
    call writefile(['The fifo is working!'], s:pipe, '')
    " echom 'printing to ' . s:pipe
    " let l:jobprint = jobstart(['echo', s:pipe, '>', s:pipe], {})
endfunction

function! s:StopTagHighlight()
    if s:job1 != 0
        call writefile(['C'], s:pipe, '')
    endif
endfunction

command! InitTagHighlight call s:InitTagHighlight()
command! StopTagHighlight call s:StopTagHighlight()

augroup KillMe
    " autocmd BufAdd *.{c,cpp,python,java,perl,go,vim,cs,rust,sh,javascript,php} call s:NewBuf()
    autocmd BufAdd * call s:NewBuf()
    autocmd BufWritePost,FileType * call s:UpdateTags()
augroup END


let g:thingything#loaded = 1
