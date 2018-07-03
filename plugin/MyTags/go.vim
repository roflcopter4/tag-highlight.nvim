if !exists('g:mytags#go#order')
        let g:mytags#go#order = 'pftsicmv'
endif

let g:mytags#go#p = { 'group': 'goPackageTag', }
let g:mytags#go#c = { 'group': 'goConstantTag', }
let g:mytags#go#t = { 'group': 'goTypeTag', }
let g:mytags#go#s = { 'group': 'goStructTag', }
let g:mytags#go#i = { 'group': 'goInterfaceTag', }
let g:mytags#go#v = { 'group': 'goGlobalVarTag', }
let g:mytags#go#f = {
            \   'group': 'goFunctionTag',
            \   'suffix': '\>\%(\s*(\|\s*:\?=\s*func\)\@='
            \ }
let g:mytags#go#m = {
            \   'group': 'goMemberTag',
            \   'prefix': '\%(\%(\>\|\]\|)\)\.\)\@5<='
            \ }

highlight def link goConstantTag	mytags_ConstantTag
highlight def link goFunctionTag	mytags_FunctionTag
highlight def link goInterfaceTag	mytags_InterfaceTag
highlight def link goMemberTag		mytags_MemberTag
highlight def link goPackageTag		mytags_PreProcTag
highlight def link goStructTag		mytags_StructTag
highlight def link goTypeTag		mytags_TypeTag
highlight def link goGlobalVarTag	mytags_PreProcTag
