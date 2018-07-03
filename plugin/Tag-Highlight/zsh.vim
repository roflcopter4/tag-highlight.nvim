runtime! 'plugin/tag_highlight/sh.vim'

if !exists('g:tag_highlight#zsh#order')
	let g:tag_highlight#zsh#order = g:tag_highlight#sh#order
endif

let g:tag_highlight#zsh#f = g:tag_highlight#sh#f
let g:tag_highlight#zsh#a = g:tag_highlight#sh#a
