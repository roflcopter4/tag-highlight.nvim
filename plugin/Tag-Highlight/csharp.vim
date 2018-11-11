if !exists('g:tag_highlight#cs#order')
        let g:tag_highlight#cs#order = 'pifEncgstedm'
endif

let g:tag_highlight#cs#c = { 'group': 'csClassTag' }
let g:tag_highlight#cs#g = { 'group': 'csEnumTypeTag' }
let g:tag_highlight#cs#s = { 'group': 'csStructTag' }
let g:tag_highlight#cs#t = { 'group': 'csTypeTag' }
let g:tag_highlight#cs#e = { 'group': 'csEnumTag' }
let g:tag_highlight#cs#d = { 'group': 'csMacroTag' }
let g:tag_highlight#cs#n = { 'group': 'csNamespaceTag' }
let g:tag_highlight#cs#E = { 'group': 'csEventTag' }
let g:tag_highlight#cs#i = { 'group': 'csInterfaceTag' }
let g:tag_highlight#cs#p = { 'group': 'csPropertyTag' }
let g:tag_highlight#cs#f = {
            \   'group': 'csFieldTag',
            \   'prefix': '\%(\%(\>\|\]\|)\)\%(\.\|->\)\)\@5<=',
            \ }
let g:tag_highlight#cs#m = {
            \   'group': 'csMethodTag',
            \   'suffix': '\>\%(\s*(\)\@='
            \ }

highlight def link csClassTag		tag_highlight_ClassTag
highlight def link csEnumTag		tag_highlight_EnumTag
highlight def link csEnumTypeTag	tag_highlight_EnumTypeTag
highlight def link csEventTag		tag_highlight_InterfaceTag
highlight def link csFieldTag		tag_highlight_MemberTag
highlight def link csInterfaceTag	tag_highlight_InterfaceTag
highlight def link csMacroTag		tag_highlight_PreProcTag
highlight def link csMethodTag		tag_highlight_FunctionTag
highlight def link csNamespaceTag	tag_highlight_NamespaceTag
highlight def link csPropertyTag	tag_highlight_PreProcTag
highlight def link csStructTag		tag_highlight_StructTag
highlight def link csTypeTag		tag_highlight_TypeTag
