if !exists('g:tag_highlight#python#order')
	let g:tag_highlight#python#order = 'mfciIx'
endif

            " \   'prefix': '\(\.\|\<def\s\+\)\@<=',
let g:tag_highlight#python#m = {
            \   'group': 'pythonMethodTag'
            \ }
let g:tag_highlight#python#x = {
            \   'group': 'pythonMethodTag'
            \ }

            " \   'prefix': '\%(\<def\s\+\)\@<!\<',
let g:tag_highlight#python#f = {
            \   'group': 'pythonFunctionTag'
            \ }
let g:tag_highlight#python#c = { 'group': 'pythonClassTag' }
let g:tag_highlight#python#v = { 'group': 'pythonGlobalVarTag' }
let g:tag_highlight#python#i = { 'group': 'pythonModuleTag' }
let g:tag_highlight#python#I = { 'group': 'pythonModuleTag' }

" let g:tag_highlight#python#equivalent = { 'I': 'i' }

highlight def link pythonMethodTag	tag_highlight_MethodTag
highlight def link pythonGlobalVarTag	tag_highlight_GlobalVarTag
highlight def link pythonFunctionTag	tag_highlight_FunctionTag
highlight def link pythonClassTag	tag_highlight_ClassTag
highlight def link pythonModuleTag	tag_highlight_NamespaceTag
