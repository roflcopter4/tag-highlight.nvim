if !exists('g:mytags#python#order')
	let g:mytags#python#order = 'mfcv'
endif

let g:mytags#python#m = {
            \   'prefix': '\(\.\|\<def\s\+\)\@<=',
            \   'group': 'pythonMethodTag'
            \ }
let g:mytags#python#f = {
            \   'prefix': '\%(\<def\s\+\)\@<!\<',
            \   'group': 'pythonFunctionTag'
            \ }
let g:mytags#python#c = { 'group': 'pythonClassTag' }
let g:mytags#python#v = { 'group': 'pythonGlobalVarTag' }

highlight def link pythonMethodTag	mytags_MethodTag
highlight def link pythonMethodTag	mytags_MethodTag
highlight def link pythonFunctionTag	mytags_FunctionTag
highlight def link pythonClassTag	mytags_ClassTag
