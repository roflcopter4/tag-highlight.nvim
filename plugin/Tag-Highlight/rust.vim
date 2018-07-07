if !exists('g:tag_highlight#rust#order')
        let g:tag_highlight#rust#order = 'nsicgtmefM'
endif

let g:tag_highlight#rust#n = { 'group': 'rustModuleTag' }
let g:tag_highlight#rust#s = { 'group': 'rustStructTag' }
let g:tag_highlight#rust#i = { 'group': 'rustTraitInterfaceTag' }
let g:tag_highlight#rust#c = { 'group': 'rustImplementationTag' }
let g:tag_highlight#rust#g = { 'group': 'rustEnumTag' }
let g:tag_highlight#rust#t = { 'group': 'rustTypeTag' }
let g:tag_highlight#rust#m = { 'group': 'rustMemberTag' }
let g:tag_highlight#rust#e = { 'group': 'rustEnumTypeTag' }
" let g:tag_highlight#rust#P = { 'group': 'rustMethodTag' }
let g:tag_highlight#rust#f = { 'group': 'rustFunctionTag' }
let g:tag_highlight#rust#M = { 'group': 'rustMacroTag' }

let g:tag_highlight#rust#equivalent = { 'P': 'f' }

highlight def link rustImplementationTag	tag_highlight_TypeTag
highlight def link rustTraitInterfaceTag	tag_highlight_TypeTag
highlight def link rustStructTag		tag_highlight_TypeTag
highlight def link rustUnionTag			tag_highlight_UnionTag
highlight def link rustEnumTypeTag		tag_highlight_EnumTypeTag
highlight def link rustModuleTag		tag_highlight_PreProcTag

highlight def link rustEnumTag		tag_highlight_EnumTag
highlight def link rustFunctionTag	tag_highlight_FunctionTag
highlight def link rustMemberTag	tag_highlight_MemberTag
highlight def link rustMacroTag		tag_highlight_PreProcTag
highlight def link rustTypeTag		tag_highlight_TypeTag
