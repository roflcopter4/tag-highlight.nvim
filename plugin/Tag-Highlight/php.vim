if !exists('g:tag_highlight#php#order')
	let g:tag_highlight#php#order = 'fc'
endif

let g:tag_highlight#php#c = { 'group': 'phpClassTag' }
let g:tag_highlight#php#f = {
            \   'group': 'phpFunctionsTag',
            \   'suffix': '(\@='
            \ }

highlight def link phpClassTag	tag_highlight_ClassTag
highlight def link phpFunctionsTag	tag_highlight_FunctionTag
