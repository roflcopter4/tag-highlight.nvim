if !exists('g:mytags#php#order')
	let g:mytags#php#order = 'fc'
endif

let g:mytags#php#c = { 'group': 'phpClassTag' }
let g:mytags#php#f = {
            \   'group': 'phpFunctionsTag',
            \   'suffix': '(\@='
            \ }

highlight def link phpClassTag	mytags_ClassTag
highlight def link phpFunctionsTag	mytags_FunctionTag
