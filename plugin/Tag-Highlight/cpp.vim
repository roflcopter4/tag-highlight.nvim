if !exists('g:tag_highlight#cpp#order')
        let g:tag_highlight#cpp#order = 'cgstuedfm'
endif

let g:tag_highlight#cpp#c = { 'group': 'cppClassTag' }
let g:tag_highlight#cpp#g = { 'group': 'cppEnumTypeTag' }
let g:tag_highlight#cpp#s = { 'group': 'cppStructTag' }
let g:tag_highlight#cpp#t = { 'group': 'cppTypeTag' }
let g:tag_highlight#cpp#u = { 'group': 'cppUnionTag' }
let g:tag_highlight#cpp#e = { 'group': 'cppEnumTag' }
let g:tag_highlight#cpp#d = { 'group': 'cppPreProcTag' }
let g:tag_highlight#cpp#m = {
            \   'group': 'cppMemberTag',
            \   'prefix': '\%(\%(\>\|\]\|)\)\%(\.\|->\)\)\@5<=',
            \ }
let g:tag_highlight#cpp#f = {
            \   'group': 'cppFunctionTag',
            \   'suffix': '\>\%(\s*(\)\@='
            \ }
let g:tag_highlight#cpp#v = { 'group': 'cppGlobalVar' }

let g:tag_highlight#cpp#equivalent = { 'p': 'f' }

highlight def link cppClassTag		tag_highlight_TypeTag
highlight def link cppEnumTypeTag	tag_highlight_EnumTypeTag
highlight def link cppStructTag		tag_highlight_StructTag
highlight def link cppUnionTag		tag_highlight_UnionTag

highlight def link cppGlobalVar		tag_highlight_GlobalVarTag
highlight def link cppEnumTag		tag_highlight_EnumTag
highlight def link cppFunctionTag	tag_highlight_FunctionTag
highlight def link cppMemberTag		tag_highlight_MemberTag
highlight def link cppPreProcTag	tag_highlight_PreProcTag
highlight def link cppTypeTag		tag_highlight_TypeTag
