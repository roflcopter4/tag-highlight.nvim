if !exists('g:mytags#java#order')
    let g:mytags#java#order = 'cimegf'
endif

let g:mytags#java#c = { 'group': 'javaClassTag' }
let g:mytags#java#i = { 'group': 'javaInterfaceTag' }
let g:mytags#java#m = { 'group': 'javaMethodTag' }
let g:mytags#java#e = { 'group': 'javaEnumTag' }
let g:mytags#java#g = { 'group': 'javaEnumTypeTag' }
let g:mytags#java#f = { 'group': 'javaFieldTag' }


highlight def link javaClassTag	mytags_TypeTag
highlight def link javaEnumTag	mytags_EnumTag
highlight def link javaEnumTypeTag	mytags_EnumTypeTag
highlight def link javaFieldTag	mytags_FieldTag
highlight def link javaInterfaceTag	mytags_InterfaceTag
highlight def link javaMethodTag	mytags_MethodTag
