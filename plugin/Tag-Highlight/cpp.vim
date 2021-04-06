if !exists('g:tag_highlight#cpp#order')
        let g:tag_highlight#cpp#order = 'TncgstuedfmMO'
endif

let g:tag_highlight#cpp#T = { 'group': 'cppTemplateTag' }
let g:tag_highlight#cpp#c = { 'group': 'cppClassTag' }
let g:tag_highlight#cpp#g = { 'group': 'cppEnumTypeTag' }
let g:tag_highlight#cpp#s = { 'group': 'cppStructTag' }
let g:tag_highlight#cpp#t = { 'group': 'cppTypeTag' }
let g:tag_highlight#cpp#u = { 'group': 'cppUnionTag' }
let g:tag_highlight#cpp#e = { 'group': 'cppEnumTag' }
let g:tag_highlight#cpp#d = { 'group': 'cppPreProcTag' }
let g:tag_highlight#cpp#n = { 'group': 'cppNamespaceTag' }
let g:tag_highlight#cpp#M = { 'group': 'cppMethodTag' }
let g:tag_highlight#cpp#O = { 'group': 'cppOverloadedOperatorTag' }
let g:tag_highlight#cpp#m = {
            \   'group': 'cppMemberTag',
            \   'prefix': '\%(\%(\>\|\]\|)\)\%(\.\|->\)\)\@5<=',
            \ }
let g:tag_highlight#cpp#f = {
            \   'group': 'cppFunctionTag',
            \   'suffix': '\>\%(\s*(\)\@='
            \ }
let g:tag_highlight#cpp#v = { 'group': 'cppGlobalVar' }
let g:tag_highlight#cpp#q = { 'group': 'cppDunno' }

let g:tag_highlight#cpp#equivalent = { 'p': 'f',
                                   \   'x': 'v', }

highlight def link cppClassTag		tag_highlight_ClassTag
highlight def link cppEnumTypeTag	tag_highlight_EnumTypeTag
highlight def link cppStructTag		tag_highlight_StructTag
highlight def link cppUnionTag		tag_highlight_UnionTag
highlight def link cppTemplateTag	tag_highlight_TemplateTag

highlight def link cppGlobalVar		tag_highlight_GlobalVarTag
highlight def link cppEnumTag		tag_highlight_EnumTag
highlight def link cppFunctionTag	tag_highlight_FunctionTag
highlight def link cppMemberTag		tag_highlight_MemberTag
highlight def link cppPreProcTag	tag_highlight_PreProcTag
highlight def link cppTypeTag		tag_highlight_TypeTag
highlight def link cppNamespaceTag	tag_highlight_NamespaceTag
highlight def link cppMethodTag		tag_highlight_MethodTag

highlight def link cppOverloadedOperatorTag tag_highlight_OverloadedOperatorTag

highlight def link cppDunno Error
