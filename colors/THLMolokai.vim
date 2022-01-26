" Author: Tomas Restrepo <tomas@winterdom.com>
" Note: Based on the monokai theme for textmate by Wimer Hazenberg and
"       its darker variant by Hamish Stuart Macpherson
"
" Note: Extensively edited by RoflCopter4

highlight clear
if exists('syntax_on')
    syntax reset
endif
let g:colors_name='THLMolokai'

function! s:Hl(group, guifg, guibg, guif)
    let l:histring = 'hi ' . a:group

    if a:guifg !=# ''
        let l:histring .= ' guifg= ' . a:guifg
    endif

    if a:guibg !=# ''
        let l:histring .= ' guibg= ' . a:guibg
    endif

    if a:guif !=# ''
        let l:histring .= ' gui= ' . a:guif
    endif

    execute l:histring
endfunction

" ---------------------------------------------------------------------
" Readable Color Definitions

let s:white_           = '#FFFFFF'
let s:black_           = '#000000'
let s:near_black_      = '#080808'
let s:red_             = '#FF0000'

let s:normal           = '#F8F8F2'
let s:lighter_bg       = '#232526'
let s:cursor           = '#F8F8F0'

let s:light_purple     = '#AE81FF'
let s:light_yellow     = '#E6DB74'
let s:bright_pink      = '#F92672'  
let s:debug_grey       = '#BCA3A3'
let s:cyan_            = '#66D9EF'
let s:lime_            = '#A6E22E'
let s:orange_          = '#FD971F'
let s:nova_deep_orange = '#FF7043'
let s:orange_red       = '#EF5939'
let s:dark_blue        = '#13354A'
let s:dark_magenta     = '#960050'
let s:very_dark_red    = '#1E0010'
let s:baby_blue        = '#7070F0'
let s:light_cyan       = '#70F0F0'
let s:salmon_          = '#EF5350'
let s:nova_light_green = '#66BB6A'
let s:palevioletred_   = '#D33682'
let s:newMagenta       = '#CD00CD'
let s:muted_yellow     = '#908B25'
let s:function_green   = '#16DB2A'
let s:new_yellow       = '#BBBD4F'

let s:beige_           = '#C4BE89'
let s:border_grey      = '#808080'
let s:comment_grey     = '#878787'
let s:cursorline_grey  = '#293739'
let s:dark_grey        = '#465457'
let s:diff_brown       = '#4C4745'
let s:diff_grey        = '#89807D'
let s:grey4            = '#658494'
let s:grey5            = '#B7BDC0' 
let s:grey6            = '#AEBBC5'
let s:grey7            = '#5F87AF'
let s:grey_brown       = '#403D3D'
let s:key_grey         = '#888A85'
let s:light_grey       = '#8F8F8F'
let s:shiny_grey       = '#7E8E91'
let s:very_dark_grey   = '#455354'
let s:very_light_grey  = '#BCBCBC'
let s:warning_grey     = '#333333'

let s:resharper_beige        = '#E6D8AD'
let s:nova_yellow            = '#FFEE58'
let s:nova_amber             = '#FFCA28'
let s:nova_orange            = '#FFA726'
let s:resharper_namespace    = '#ADD8E6'
let s:resharper_light_blue   = '#ADD8E6'
let s:resharper_light_purple = '#DDA0DD'
let s:lighter_pink           = '#E58CB9'
let s:nifty_pink             = '#E55CA1'
let s:idunno                 = '#F7A6DA'
let s:idunno2                = '#ED92C0'
let s:resharper_light_blue   = '#ADD8E6'
let s:darker_cyan            = '#39acc2'
let s:nova_cyan              = '#26C6DA'
let s:nova_light_blue        = '#29B6F6'
let s:nova_blue              = '#42A5F5'
let s:nova_teal              = '#26A69A'
let s:resharper_light_green  = '#90EE90'
let s:resharper_operator     = '#4EC9B0'
let s:MSVC_darker_green      = '#4EC94C'

let s:EMPTY       = ''
let s:None        = 'none'
let s:BOLD        = 'bold'
let s:ITALIC      = 'italic'
let s:BOLD_ITALIC = 'italic,bold'
let s:REVERSE     = 'reverse'
let s:UNDERCURL   = 'undercurl'
let s:UNDERLINE   = 'underline'
let s:FG          = 'fg'
let s:BG          = 'bg'


if exists('g:myMolokai_BG')
    if g:myMolokai_BG ==# 'lighter'
        let s:background = '#1B1D1E'
    elseif g:myMolokai_BG ==# 'custom'
        let s:background = g:myMolokai_CustomBG
    else
        let s:background = '#131515'
    endif
else
    let s:background = '#131515'
endif


if exists('g:myMolokai_FG')
    if g:myMolokai_FG ==# 'other'
        let s:foreground = '#F8F8F2'
    elseif g:myMolokai_FG ==# 'custom'
        let s:foreground = g:myMolokai_CustomFG
    else
        let s:foreground = '#ECEFF1'
    endif
else
    let s:foreground     = '#ECEFF1'
endif


if exists('g:myMolokaiComment')
    if g:myMolokaiComment ==# 'shiny'
        let s:comment = s:shiny_grey
    elseif g:myMolokaiComment ==# 'comment_grey'
        let s:comment = s:comment_grey
    elseif g:myMolokaiComment ==# 'custom'
        let s:comment = g:myMolokaiComment_Custom
    else
        let s:comment = s:grey4
    endif
else
    let g:comment = '#5F87AF'
endif


" ---------------------------------------------------------------------

call s:Hl('Boolean',        s:light_purple,    s:EMPTY,           s:BOLD)
call s:Hl('Character',      s:light_yellow,    s:EMPTY,           s:None)
call s:Hl('Comment',        s:comment,         s:EMPTY,           s:None)
call s:Hl('Conditional',    s:bright_pink,     s:EMPTY,           s:BOLD)
call s:Hl('Constant',       s:light_purple,    s:EMPTY,           s:BOLD)
call s:Hl('Cursor',         s:black_,          s:cursor,          s:None)
call s:Hl('CursorColumn',   s:EMPTY,           s:cursorline_grey, s:None)
call s:Hl('CursorLine',     s:EMPTY,           s:cursorline_grey, s:None)
call s:Hl('Debug',          s:debug_grey,      s:EMPTY,           s:BOLD)
call s:Hl('Define',         s:cyan_,           s:EMPTY,           s:None)
call s:Hl('Delimiter',      s:light_grey,      s:EMPTY,           s:None)
call s:Hl('DiffAdd',        s:EMPTY,           s:dark_blue,       s:None)
call s:Hl('DiffChange',     s:diff_grey,       s:diff_brown,      s:None)
call s:Hl('DiffDelete',     s:diff_grey,       s:salmon_,         s:None)
call s:Hl('DiffText',       s:EMPTY,           s:diff_brown,      s:BOLD_ITALIC)
call s:Hl('Directory',      s:lime_,           s:EMPTY,           s:BOLD)
call s:Hl('Error',          s:normal,          s:salmon_,         s:None)
call s:Hl('ErrorMsg',       s:bright_pink,     s:lighter_bg,      s:BOLD)
call s:Hl('Exception',      s:lime_,           s:EMPTY,           s:BOLD)
call s:Hl('Float',          s:light_purple,    s:EMPTY,           s:None)
call s:Hl('FoldColumn',     s:dark_grey,       s:black_,          s:None)
call s:Hl('Folded',         s:dark_grey,       s:black_,          s:None)
call s:Hl('Function',       s:lime_,           s:EMPTY,           s:None)
call s:Hl('Identifier',     s:orange_,         s:EMPTY,           s:None)
call s:Hl('Ignore',         s:border_grey,     s:BG,              s:None)
call s:Hl('IncSearch',      s:orange_,         s:black_,          s:None)
"call s:Hl('Include',        s:lime_,           s:EMPTY,           s:None)
call s:Hl('Include',        s:baby_blue,       s:EMPTY,          s:None)
call s:Hl('Keyword',        s:bright_pink,     s:EMPTY,           s:BOLD)
call s:Hl('Label',          s:light_yellow,    s:EMPTY,           s:None)
call s:Hl('LineNr',         s:very_light_grey, s:lighter_bg,      s:None)
call s:Hl('Macro',          s:beige_,          s:EMPTY,           s:None)
call s:Hl('MatchParen',     s:black_,          s:orange_,         s:None)
call s:Hl('ModeMsg',        s:light_yellow,    s:EMPTY,           s:None)
call s:Hl('MoreMsg',        s:light_yellow,    s:EMPTY,           s:None)
call s:Hl('NonText',        s:very_light_grey, s:lighter_bg,      s:None)
call s:Hl('Normal',         s:foreground,      s:background,      s:None)
call s:Hl('Number',         s:light_purple,    s:EMPTY,           s:None)
call s:Hl('Operator',       s:bright_pink,     s:EMPTY,           s:None)
call s:Hl('Pmenu',          s:cyan_,           s:black_,          s:None)
call s:Hl('PmenuSbar',      s:EMPTY,           s:near_black_,     s:None)
call s:Hl('PmenuSel',       s:EMPTY,           s:border_grey,     s:None)
call s:Hl('PmenuThumb',     s:cyan_,           s:EMPTY,           s:None)
call s:Hl('PreCondit',      s:beige_,          s:EMPTY,           s:None)
call s:Hl('PreProc',        s:lime_,           s:EMPTY,           s:None)
call s:Hl('Question',       s:cyan_,           s:EMPTY,           s:None)
call s:Hl('Repeat',         s:bright_pink,     s:EMPTY,           s:BOLD)
call s:Hl('Search',         s:black_,          s:beige_,          s:None)
call s:Hl('SignColumn',     s:lime_,           s:lighter_bg,      s:None)
call s:Hl('Special',        s:cyan_,           s:BG,              s:None)
call s:Hl('SpecialChar',    s:bright_pink,     s:EMPTY,           s:None)
call s:Hl('SpecialComment', s:shiny_grey,      s:EMPTY,           s:None)
call s:Hl('SpecialKey',     s:cyan_,           s:EMPTY,           s:None)
call s:Hl('Statement',      s:bright_pink,     s:EMPTY,           s:BOLD)
call s:Hl('StatusLine',     s:very_dark_grey,  s:FG,              s:None)
call s:Hl('StatusLineNC',   s:border_grey,     s:near_black_,     s:None)
call s:Hl('StorageClass',   s:orange_,         s:EMPTY,           s:None)
call s:Hl('String',         s:light_yellow,    s:EMPTY,           s:None)
call s:Hl('Structure',      s:cyan_,           s:EMPTY,           s:BOLD)
call s:Hl('Substitute',     s:orange_,         s:black_,          s:None)
call s:Hl('Tag',            s:bright_pink,     s:EMPTY,           s:ITALIC)
call s:Hl('Title',          s:orange_red,      s:EMPTY,           s:None)
call s:Hl('Todo',           s:white_,          s:BG,              s:BOLD)
call s:Hl('Type',           s:cyan_,           s:EMPTY,           s:None)
call s:Hl('Typedef',        s:cyan_,           s:EMPTY,           s:None)
call s:Hl('Underlined',     s:border_grey,     s:EMPTY,           s:UNDERLINE)
call s:Hl('VertSplit',      s:border_grey,     s:near_black_,     s:BOLD)
call s:Hl('Visual',         s:EMPTY,           s:grey_brown,      s:None)
call s:Hl('VisualNOS',      s:EMPTY,           s:grey_brown,      s:None)
call s:Hl('WarningMsg',     s:white_,          s:warning_grey,    s:BOLD)
call s:Hl('WildMenu',       s:cyan_,           s:black_,          s:None)

if has('spell')
    hi SpellBad    guisp=#FF0000  gui=undercurl
    hi SpellCap    guisp=#7070F0  gui=undercurl
    hi SpellLocal  guisp=#70F0F0  gui=undercurl
    hi SpellRare   guisp=#FFFFFF  gui=undercurl
endif

"==============================================================================
" NonStandard:
" Mishmash of randomly named junk


call s:Hl('CFuncTag',           s:MSVC_darker_green,      s:EMPTY, s:None)
call s:Hl('CMember',            s:grey5,                  s:EMPTY, s:None)
call s:Hl('Enum',               s:nova_blue,              s:EMPTY, s:None)
call s:Hl('GlobalVarTag',       s:muted_yellow,           s:EMPTY, s:None)
call s:Hl('Method',             s:resharper_light_green,  s:EMPTY, s:None)
call s:Hl('Namespace',          s:resharper_light_blue,   s:EMPTY, s:None)
call s:Hl('NewClassColor',      s:resharper_light_purple, s:EMPTY, s:None)
call s:Hl('NewTemplateColor',   s:resharper_light_purple, s:EMPTY, s:BOLD)
call s:Hl('NonTypeTemplParam',  s:muted_yellow,           s:EMPTY, s:None)
call s:Hl('OverloadedDecl',     s:nova_amber,             s:EMPTY, s:None)
call s:Hl('OverloadedOperator', s:resharper_operator,     s:EMPTY, s:None)

call s:Hl('NamespaceB',         s:resharper_light_blue,   s:EMPTY, s:BOLD)
call s:Hl('PreProcB',           s:lime_,                  s:EMPTY, s:BOLD)

call s:Hl('Bold',               s:EMPTY,                  s:EMPTY, s:BOLD)
call s:Hl('BoldGrey',           s:debug_grey,             s:EMPTY, s:BOLD)
call s:Hl('BoldRed',            s:red_,                   s:EMPTY, s:BOLD)
call s:Hl('LightPinkR',         s:bright_pink,            s:EMPTY, s:None)
call s:Hl('magentaIGuess',      s:resharper_light_purple, s:EMPTY, s:None)
call s:Hl('boldOrange',         s:orange_,                s:EMPTY, s:BOLD)

call s:Hl('c_preproc',          s:baby_blue,              s:EMPTY, s:None)
call s:Hl('C_Struct',           s:cyan_,                  s:EMPTY, s:BOLD)
call s:Hl('mutedFunc',          s:grey5,                  s:EMPTY, s:BOLD)

call s:Hl('CommaSemicolon',     s:EMPTY,                  s:EMPTY, s:BOLD)
call s:Hl('DelimiterBold',      s:light_grey,             s:EMPTY, s:BOLD)
call s:Hl('DereferenceStar',    s:debug_grey,             s:EMPTY, s:BOLD)
call s:Hl('NegationChar',       s:red_,                   s:EMPTY, s:BOLD)
" call s:Hl('OperatorChars',      s:debug_grey,             s:EMPTY, s:BOLD)

call s:Hl('cMiscFuncs',         s:orange_red,             s:EMPTY, s:BOLD)
call s:Hl('cNumberPrefix',      s:nova_light_green,       s:EMPTY, s:None)
call s:Hl('cNumberSuffix',      s:nova_light_green,       s:EMPTY, s:None)
call s:Hl('cSpecial',           s:light_cyan,             s:EMPTY, s:None)

call s:Hl('PerlMulti',          s:cyan_,                  s:EMPTY, s:BOLD)
call s:Hl('PerlSpecialChar',    s:bright_pink,            s:EMPTY, s:BOLD)
call s:Hl('PerlSpecialChar2',   s:orange_red,             s:EMPTY, s:BOLD)
call s:Hl('perlMatchSE',        s:lime_,                  s:EMPTY, s:BOLD)
call s:Hl('perlQuoteSE',        s:lime_,                  s:EMPTY, s:BOLD)
call s:Hl('perlTypeSpec',       s:cyan_,                  s:EMPTY, s:BOLD)
call s:Hl('pythonDocstring',    s:comment,                s:EMPTY, s:None)

call s:Hl('ExperimentalColor01r', s:idunno, s:EMPTY, s:None)
call s:Hl('ExperimentalColor01b', s:idunno, s:EMPTY, s:BOLD)
call s:Hl('ExperimentalColor02r', s:idunno2, s:EMPTY, s:None)
call s:Hl('ExperimentalColor02b', s:idunno2, s:EMPTY, s:BOLD)
call s:Hl('ExperimentalColor03r', s:lighter_pink, s:EMPTY, s:None)
call s:Hl('ExperimentalColor03b', s:lighter_pink, s:EMPTY, s:BOLD)

set background=dark
