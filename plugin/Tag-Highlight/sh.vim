if !exists('g:tag_highlight#sh#order')
	let g:tag_highlight#sh#order = 'fa'
endif

let g:tag_highlight#sh#f = { 'group': 'shFunctionTag' }
let g:tag_highlight#sh#a = { 'group': 'shAliasTag' }

highlight def link shFunctionTag	tag_highlight_FunctionTag
highlight def link shAliasTag	tag_highlight_PreProcTag
