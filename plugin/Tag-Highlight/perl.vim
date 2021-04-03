if !exists('g:tag_highlight#perl#order')
	let g:tag_highlight#perl#order = 'cps'
endif

let g:tag_highlight#perl#c = { 'group': 'perlConstantTag' }
let g:tag_highlight#perl#p = { 'group': 'perlPackageTag' }
let g:tag_highlight#perl#s = {
            \   'group': 'perlFunctionTag',
            \   'prefix': '\%(\<sub\s\*\)\@<!\%(>\|\s\|&\|^\)\@<=\<',
            \   'notin':  [
            \       '.*String.*',
            \       '.*Comment.*',
            \       '.*Match.*',
            \       '.*HereDoc.*',
            \   ]
            \ }

let g:tag_highlight#perl#allbut = '.*HereDoc.*,perlMatch.*,perl.*DQ.*,perl.*SQ.*,perlQQ'

highlight def link perlConstantTag	tag_highlight_ConstantTag
highlight def link perlFunctionTag	tag_highlight_FunctionTag
highlight def link perlPackageTag	tag_highlight_TypeTag
