if !exists('g:tag_highlight#java#order')
    let g:tag_highlight#java#order = 'cimegf'
endif

let g:tag_highlight#java#c = { 'group': 'javaClassTag' }
let g:tag_highlight#java#i = { 'group': 'javaInterfaceTag' }
let g:tag_highlight#java#m = { 'group': 'javaMethodTag' }
let g:tag_highlight#java#e = { 'group': 'javaEnumTag' }
let g:tag_highlight#java#g = { 'group': 'javaEnumTypeTag' }
let g:tag_highlight#java#f = { 'group': 'javaFieldTag' }


highlight def link javaClassTag	tag_highlight_TypeTag
highlight def link javaEnumTag	tag_highlight_EnumTag
highlight def link javaEnumTypeTag	tag_highlight_EnumTypeTag
highlight def link javaFieldTag	tag_highlight_FieldTag
highlight def link javaInterfaceTag	tag_highlight_InterfaceTag
highlight def link javaMethodTag	tag_highlight_MethodTag
