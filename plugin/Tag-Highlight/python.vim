if !exists('g:tag_highlight#python#order')
	let g:tag_highlight#python#order = 'mfcv'
endif

let g:tag_highlight#python#m = {
            \   'prefix': '\(\.\|\<def\s\+\)\@<=',
            \   'group': 'pythonMethodTag'
            \ }
let g:tag_highlight#python#f = {
            \   'prefix': '\%(\<def\s\+\)\@<!\<',
            \   'group': 'pythonFunctionTag'
            \ }
let g:tag_highlight#python#c = { 'group': 'pythonClassTag' }
let g:tag_highlight#python#v = { 'group': 'pythonGlobalVarTag' }

highlight def link pythonMethodTag	tag_highlight_MethodTag
highlight def link pythonMethodTag	tag_highlight_MethodTag
highlight def link pythonFunctionTag	tag_highlight_FunctionTag
highlight def link pythonClassTag	tag_highlight_ClassTag
