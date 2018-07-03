if !exists('g:mytags#vim#order')
	let g:mytags#vim#order = 'acfv'
endif

let g:mytags#vim#v = { 'group': 'vimVariableTag' }
let g:mytags#vim#a = { 'group': 'vimAutoGroupTag' }
let g:mytags#vim#c = {
            \   'group': 'vimCommandTag',
            \   'prefix': '\(\(^\|\s\):\?\)\@<=',
            \   'suffix': '\(!\?\(\s\|$\)\)\@='
            \ }

" Use :set iskeyword+=: for vim to make s:/<sid> functions to show correctly
" let g:mytags#vim#f = { 'group': 'vimFuncNameTag', }
let g:mytags#vim#f = {
            \ 'group': 'vimFuncNameTag',
            \ 'prefix': '\%(\%(g\|s\|l\):\)\=',
            \ }
" let g:mytags#vim#f = {
"             \   'group': 'vimFuncNameTag',
"             \   'prefix': '\%(\<s:\|<[sS][iI][dD]>\)\@<!\<',
"             \   'filter': {
"             \       'pattern': '(?i)(<sid>|\bs:)',
"             \       'group': 'vimvimScriptFuncNameTag',
"             \       'prefix': '\C\%(\<s:\|<[sS][iI][dD]>\)\?',
"             \   }
"             \ }


highlight def link vimFuncNameTag		mytags_FunctionTag
highlight def link vimScriptFuncNameTag	mytags_FunctionTag
highlight def link vimCommandTag		mytags_PreProcTag
highlight def link vimAutoGroupTag		mytags_PreProcTag
highlight def link vimVariableTag		mytags_VariableTag
