if !exists('g:mytags#cpp#order')
        let g:mytags#cpp#order = 'cgstuedfm'
endif

let g:mytags#cpp#c = { 'group': 'cppClassTag' }
let g:mytags#cpp#g = { 'group': 'cppEnumTypeTag' }
let g:mytags#cpp#s = { 'group': 'cppStructTag' }
let g:mytags#cpp#t = { 'group': 'cppTypeTag' }
let g:mytags#cpp#u = { 'group': 'cppUnionTag' }
let g:mytags#cpp#e = { 'group': 'cppEnumTag' }
let g:mytags#cpp#d = { 'group': 'cppPreProcTag' }
let g:mytags#cpp#m = {
            \   'group': 'cppMemberTag',
            \   'prefix': '\%(\%(\>\|\]\|)\)\%(\.\|->\)\)\@5<=',
            \ }
let g:mytags#cpp#f = {
            \   'group': 'cppFunctionTag',
            \   'suffix': '\>\%(\s*(\)\@='
            \ }
let g:mytags#cpp#v = { 'group': 'cppGlobalVar' }

let g:mytags#cpp#equivalent = { 'p': 'f' }

highlight def link cppClassTag		mytags_TypeTag
highlight def link cppEnumTypeTag	mytags_EnumTypeTag
highlight def link cppStructTag		mytags_StructTag
highlight def link cppUnionTag		mytags_UnionTag

highlight def link cppGlobalVar		mytags_GlobalVarTag
highlight def link cppEnumTag		mytags_EnumTag
highlight def link cppFunctionTag	mytags_FunctionTag
highlight def link cppMemberTag		mytags_MemberTag
highlight def link cppPreProcTag	mytags_PreProcTag
highlight def link cppTypeTag		mytags_TypeTag
