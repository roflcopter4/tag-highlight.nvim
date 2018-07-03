let g:mytags#javascript#order = 'cCfmpo'

let g:mytags#javascript#c = {
            \   'group': 'javascriptClassTag',
            \   'notin': [
            \       'jsx.*',
            \       '.*Comment.*',
            \       '.*String.*',
            \       'javascriptTemplate'
            \   ]
            \ }

let g:mytags#javascript#C = {
            \   'group': 'javascriptConstantTag',
            \   'notin': [
            \       'jsx.*',
            \       '.*Comment.*',
            \       '.*String.*',
            \       'javascriptTemplate'
            \   ]
            \ }

let g:mytags#javascript#f = {
            \   'group': 'javascriptFunctionTag',
            \   'notin': [
            \       'jsx.*',
            \       '.*Comment.*',
            \       '.*String.*',
            \       'javascriptTemplate',
            \       'javascriptConditional',
            \       'javascriptRepeat'
            \   ]
            \ }

let g:mytags#javascript#m = {
            \   'group': 'javascriptMethodTag',
            \   'notin': [
            \       'jsx.*',
            \       '.*Comment.*',
            \       '.*String.*',
            \       'javascriptTemplate',
            \       'javascriptConditional',
            \       'javascriptRepeat'
            \   ]
            \ }

let g:mytags#javascript#o = {
            \   'group': 'javascriptObjectTag',
            \   'notin': [
            \       'jsx.*',
            \       '.*Comment.*',
            \       '.*String.*',
            \       'javascriptTemplate',
            \       'javascriptConditional',
            \       'javascriptRepeat'
            \   ]
            \ }

let g:mytags#javascript#p = {
            \   'group': 'javascriptPropTag',
            \   'notin': [
            \       'jsx.*',
            \       '.*Comment.*',
            \       '.*String.*',
            \       'javascriptTemplate',
            \       'javascriptConditional',
            \       'javascriptRepeat'
            \   ]
            \ }

highlight def link javascriptClassTag	mytags_ClassTag
highlight def link javascriptConstantTag	mytags_ConstantTag
highlight def link javascriptFunctionTag	mytags_FunctionTag
highlight def link javascriptMethodTag	mytags_MethodTag
highlight def link javascriptObjectTag	mytags_ObjectTag
highlight def link javascriptPropTag	mytags_PreProcTag
