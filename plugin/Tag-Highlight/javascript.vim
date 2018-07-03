let g:tag_highlight#javascript#order = 'cCfmpo'

let g:tag_highlight#javascript#c = {
            \   'group': 'javascriptClassTag',
            \   'notin': [
            \       'jsx.*',
            \       '.*Comment.*',
            \       '.*String.*',
            \       'javascriptTemplate'
            \   ]
            \ }

let g:tag_highlight#javascript#C = {
            \   'group': 'javascriptConstantTag',
            \   'notin': [
            \       'jsx.*',
            \       '.*Comment.*',
            \       '.*String.*',
            \       'javascriptTemplate'
            \   ]
            \ }

let g:tag_highlight#javascript#f = {
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

let g:tag_highlight#javascript#m = {
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

let g:tag_highlight#javascript#o = {
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

let g:tag_highlight#javascript#p = {
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

highlight def link javascriptClassTag	tag_highlight_ClassTag
highlight def link javascriptConstantTag	tag_highlight_ConstantTag
highlight def link javascriptFunctionTag	tag_highlight_FunctionTag
highlight def link javascriptMethodTag	tag_highlight_MethodTag
highlight def link javascriptObjectTag	tag_highlight_ObjectTag
highlight def link javascriptPropTag	tag_highlight_PreProcTag
