if !exists('g:mytags#sh#order')
	let g:mytags#sh#order = 'fa'
endif

let g:mytags#sh#f = { 'group': 'shFunctionTag' }
let g:mytags#sh#a = { 'group': 'shAliasTag' }

highlight def link shFunctionTag	mytags_FunctionTag
highlight def link shAliasTag	mytags_PreProcTag
