if !exists('g:tag_highlight#c#order')
        let g:tag_highlight#c#order = 'guesmfvdt'
endif

" let g:tag_highlight#c#g = { 'group': 'cEnumTypeTag' }
let g:tag_highlight#c#t = { 'group': 'cTypeTag' }

let g:tag_highlight#c#g = {
            \ 'group': 'cEnumTypeTag',
            \ 'prefix': '\%(enum\s\+\)\@5<=',
            \ }
let g:tag_highlight#c#s = {
            \   'group': 'cStructTag',
            \   'prefix': '\%(struct\s\+\)\@7<='
            \ }
let g:tag_highlight#c#u = {
            \ 'group': 'cUnionTag',
            \ 'prefix': '\%(union\s\+\)\@6<=',
            \ }

let g:tag_highlight#c#e = { 'group': 'cEnumTag' }
let g:tag_highlight#c#d = { 'group': 'cPreProcTag' }

let g:tag_highlight#c#m = {
            \   'group': 'cMemberTag',
            \   'prefix': '\%(\%(\>\|\]\|)\)\%(\.\|->\)\)\@5<=',
            \ }

" let g:tag_highlight#c#f = {
            " \   'group': 'cFunctionTag',
            " \   'suffix': '\>\%(\s*(\)\@='
            " \ }
let g:tag_highlight#c#f = { 'group': 'cFunctionTag' }

" let g:tag_highlight#c#f = {
"             \   'group': 'cFunctionTag',
"             \   'suffix': '\>\%(\.\|->\)\@!'
"             \ }

" let g:tag_highlight#c#t = {
"             \ 'group': 'cTypeTag',
"             \ 'suffix': '\>\%(\.\|->\)\@!'
"             \ }

let g:tag_highlight#c#R = {
            \    'group': 'cFuncRef',
            \    'prefix': '\%(&\)\@1<='
            \ }

let g:tag_highlight#c#v = { 'group': 'cGlobalVar' }

let g:tag_highlight#c#equivalent = { 'p': 'f',
                                 \   'x': 'v', }

highlight def link cClassTag	tag_highlight_TypeTag
highlight def link cEnumTypeTag	tag_highlight_EnumTypeTag
highlight def link cStructTag	tag_highlight_StructTag
highlight def link cUnionTag	tag_highlight_UnionTag
highlight def link cFuncRef	tag_highlight_FunctionTag

highlight def link cGlobalVar	tag_highlight_GlobalVarTag
highlight def link cEnumTag	tag_highlight_EnumTag
highlight def link cFunctionTag	tag_highlight_FunctionTag
highlight def link cMemberTag	tag_highlight_MemberTag
highlight def link cPreProcTag	tag_highlight_PreProcTag
highlight def link cTypeTag	tag_highlight_TypeTag
