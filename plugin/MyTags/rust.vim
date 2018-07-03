if !exists('g:mytags#rust#order')
        let g:mytags#rust#order = 'nsicgtmePfM'
endif

let g:mytags#rust#n = { 'group': 'rustModuleTag' }
let g:mytags#rust#s = { 'group': 'rustStructTag' }
let g:mytags#rust#i = { 'group': 'rustTraitInterfaceTag' }
let g:mytags#rust#c = { 'group': 'rustImplementationTag' }
let g:mytags#rust#g = { 'group': 'rustEnumTag' }
let g:mytags#rust#t = { 'group': 'rustTypeTag' }
let g:mytags#rust#m = { 'group': 'rustMemberTag' }
let g:mytags#rust#e = { 'group': 'rustEnumTypeTag' }
" let g:mytags#rust#P = { 'group': 'rustMethodTag' }
let g:mytags#rust#f = { 'group': 'rustFunctionTag' }
let g:mytags#rust#M = { 'group': 'rustMacroTag' }

let g:mytags#rust#equivalent = { 'P': 'f' }

highlight def link rustImplementationTag	mytags_TypeTag
highlight def link rustTraitInterfaceTag	mytags_TypeTag
highlight def link rustStructTag		mytags_TypeTag
highlight def link rustUnionTag			mytags_UnionTag
highlight def link rustEnumTypeTag		mytags_EnumTypeTag
highlight def link rustModuleTag		mytags_PreProcTag

highlight def link rustEnumTag		mytags_EnumTag
highlight def link rustFunctionTag	mytags_FunctionTag
highlight def link rustMemberTag	mytags_MemberTag
highlight def link rustMacroTag		mytags_PreProcTag
highlight def link rustTypeTag		mytags_TypeTag
