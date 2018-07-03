if !exists('g:tag_highlight#vim#order')
	let g:tag_highlight#vim#order = 'acfv'
endif

let g:tag_highlight#vim#v = { 'group': 'vimVariableTag' }
let g:tag_highlight#vim#a = { 'group': 'vimAutoGroupTag' }
let g:tag_highlight#vim#c = {
            \   'group': 'vimCommandTag',
            \   'prefix': '\(\(^\|\s\):\?\)\@<=',
            \   'suffix': '\(!\?\(\s\|$\)\)\@='
            \ }

" Use :set iskeyword+=: for vim to make s:/<sid> functions to show correctly
" let g:tag_highlight#vim#f = { 'group': 'vimFuncNameTag', }
let g:tag_highlight#vim#f = {
            \ 'group': 'vimFuncNameTag',
            \ 'prefix': '\%(\%(g\|s\|l\):\)\=',
            \ }
" let g:tag_highlight#vim#f = {
"             \   'group': 'vimFuncNameTag',
"             \   'prefix': '\%(\<s:\|<[sS][iI][dD]>\)\@<!\<',
"             \   'filter': {
"             \       'pattern': '(?i)(<sid>|\bs:)',
"             \       'group': 'vimvimScriptFuncNameTag',
"             \       'prefix': '\C\%(\<s:\|<[sS][iI][dD]>\)\?',
"             \   }
"             \ }


highlight def link vimFuncNameTag		tag_highlight_FunctionTag
highlight def link vimScriptFuncNameTag	tag_highlight_FunctionTag
highlight def link vimCommandTag		tag_highlight_PreProcTag
highlight def link vimAutoGroupTag		tag_highlight_PreProcTag
highlight def link vimVariableTag		tag_highlight_VariableTag
