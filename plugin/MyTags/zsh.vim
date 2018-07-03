runtime! 'plugin/mytags/sh.vim'

if !exists('g:mytags#zsh#order')
	let g:mytags#zsh#order = g:mytags#sh#order
endif

let g:mytags#zsh#f = g:mytags#sh#f
let g:mytags#zsh#a = g:mytags#sh#a
