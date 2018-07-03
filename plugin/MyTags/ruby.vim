if !exists('g:mytags#ruby#order')
	let g:mytags#ruby#order = 'mfc'
endif

let g:mytags#ruby#m = { 'group': 'rubyModuleTag' }
let g:mytags#ruby#c = { 'group': 'rubyClassTag' }
let g:mytags#ruby#f = { 'group': 'rubyMethodTag' }

let g:mytags#ruby#equivalent = { 'F': 'f' }

highlight def link rubyModuleTag	mytags_ModuleTag
highlight def link rubyClassTag	mytags_ClassTag
highlight def link rubyMethodTag	mytags_MethodTag
