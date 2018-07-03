if !exists('g:mytags#c#order')
        let g:mytags#c#order = 'guesmfvdt'
endif

" let g:mytags#c#g = { 'group': 'cEnumTypeTag' }
" let g:mytags#c#t = { 'group': 'cTypeTag' }

let g:mytags#c#g = {
            \ 'group': 'cEnumTypeTag',
            \ 'prefix': '\%(enum\s\+\)\@5<=',
            \ }
let g:mytags#c#s = {
            \   'group': 'cStructTag',
            \   'prefix': '\%(struct\s\+\)\@7<='
            \ }
let g:mytags#c#u = {
            \ 'group': 'cUnionTag',
            \ 'prefix': '\%(union\s\+\)\@6<=',
            \ }

let g:mytags#c#e = { 'group': 'cEnumTag' }
let g:mytags#c#d = { 'group': 'cPreProcTag' }

let g:mytags#c#m = {
            \   'group': 'cMemberTag',
            \   'prefix': '\%(\%(\>\|\]\|)\)\%(\.\|->\)\)\@5<=',
            \ }

" let g:mytags#c#f = {
            " \   'group': 'cFunctionTag',
            " \   'suffix': '\>\%(\s*(\)\@='
            " \ }
let g:mytags#c#f = { 'group': 'cFunctionTag' }

" let g:mytags#c#f = {
"             \   'group': 'cFunctionTag',
"             \   'suffix': '\>\%(\.\|->\)\@!'
"             \ }

let g:mytags#c#t = {
            \ 'group': 'cTypeTag',
            \ 'suffix': '\>\%(\.\|->\)\@!'
            \ }

let g:mytags#c#R = {
            \    'group': 'cFuncRef',
            \    'prefix': '\%(&\)\@1<='
            \ }

let g:mytags#c#v = { 'group': 'cGlobalVar' }

let g:mytags#c#equivalent = { 'p': 'f' }

highlight def link cClassTag	mytags_TypeTag
highlight def link cEnumTypeTag	mytags_EnumTypeTag
highlight def link cStructTag	mytags_StructTag
highlight def link cUnionTag	mytags_UnionTag
highlight def link cFuncRef	mytags_FunctionTag

highlight def link cGlobalVar	mytags_GlobalVarTag
highlight def link cEnumTag	mytags_EnumTag
highlight def link cFunctionTag	mytags_FunctionTag
highlight def link cMemberTag	mytags_MemberTag
highlight def link cPreProcTag	mytags_PreProcTag
highlight def link cTypeTag	mytags_TypeTag
