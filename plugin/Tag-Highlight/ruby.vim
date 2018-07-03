if !exists('g:tag_highlight#ruby#order')
	let g:tag_highlight#ruby#order = 'mfc'
endif

let g:tag_highlight#ruby#m = { 'group': 'rubyModuleTag' }
let g:tag_highlight#ruby#c = { 'group': 'rubyClassTag' }
let g:tag_highlight#ruby#f = { 'group': 'rubyMethodTag' }

let g:tag_highlight#ruby#equivalent = { 'F': 'f' }

highlight def link rubyModuleTag	tag_highlight_ModuleTag
highlight def link rubyClassTag	tag_highlight_ClassTag
highlight def link rubyMethodTag	tag_highlight_MethodTag
