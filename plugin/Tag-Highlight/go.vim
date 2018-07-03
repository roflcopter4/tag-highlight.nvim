if !exists('g:tag_highlight#go#order')
        let g:tag_highlight#go#order = 'pftsicmv'
endif

let g:tag_highlight#go#p = { 'group': 'goPackageTag', }
let g:tag_highlight#go#c = { 'group': 'goConstantTag', }
let g:tag_highlight#go#t = { 'group': 'goTypeTag', }
let g:tag_highlight#go#s = { 'group': 'goStructTag', }
let g:tag_highlight#go#i = { 'group': 'goInterfaceTag', }
let g:tag_highlight#go#v = { 'group': 'goGlobalVarTag', }
let g:tag_highlight#go#f = {
            \   'group': 'goFunctionTag',
            \   'suffix': '\>\%(\s*(\|\s*:\?=\s*func\)\@='
            \ }
let g:tag_highlight#go#m = {
            \   'group': 'goMemberTag',
            \   'prefix': '\%(\%(\>\|\]\|)\)\.\)\@5<='
            \ }

highlight def link goConstantTag	tag_highlight_ConstantTag
highlight def link goFunctionTag	tag_highlight_FunctionTag
highlight def link goInterfaceTag	tag_highlight_InterfaceTag
highlight def link goMemberTag		tag_highlight_MemberTag
highlight def link goPackageTag		tag_highlight_PreProcTag
highlight def link goStructTag		tag_highlight_StructTag
highlight def link goTypeTag		tag_highlight_TypeTag
highlight def link goGlobalVarTag	tag_highlight_PreProcTag
