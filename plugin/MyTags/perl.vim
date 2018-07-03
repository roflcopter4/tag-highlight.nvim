if !exists('g:mytags#perl#order')
	let g:mytags#perl#order = 'cps'
endif

let g:mytags#perl#c = { 'group': 'perlConstantTag' }
let g:mytags#perl#p = { 'group': 'perlPackageTag' }
let g:mytags#perl#s = {
            \   'group': 'perlFunctionTag',
            \   'prefix': '\%(\<sub\s\*\)\@<!\%(>\|\s\|&\|^\)\@<=\<',
            \ }

highlight def link perlConstantTag	mytags_ConstantTag
highlight def link perlFunctionTag	mytags_FunctionTag
highlight def link perlPackageTag	mytags_ModuleTag
