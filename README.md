# tag-highlight.nvim
This plugin provides IDE-like semantic highlighting for various languages in Neovim. 

Currently, C, C++, and Go are supported, and ore languages will be added as time permits. Highlighting for other languages is approximated using ctags. This plugin doubles as an automatic tag manager.

Originally, I wrote this to be a simple tag highlighting plugin for Neovim, much like Neotags (https://github.com/c0r73x/neotags.nvim) implemented entirely in C, for no particular reason. Working with Neovim proved so simple and fast that it seemed natural to expand this project to implement true semantic highlighting.

## DISCLAIMER/PLEA
I have no idea if anyone is actually using this thing. If you are, or you've tried to and something has gone horribly wrong I'd be grateful for any feedback. Heck, it'd make my day even if all you were to say is that you hate my software and/or me. And I'm not even kidding. Please do complain if it doesn't work.

## Installing
If using dein, installing should be as easy as adding this to your .vimrc.
    `call dein#add('roflcopter4/tag-highlight.nvim', {'merged': v:false, 'build': 'sh build.sh'})`
The little shell script should build the binary and set everything up.

Disclaimer: 'should be'. Odds are it will fail and you'll have to run the script yourself. If that also fails, you'll have to build the cmake project yourself too. This isn't very likely to happen unless you're missing libraries. LLVM with libclang is required, and go must installed to build the go support. The C compiler must be either gcc or clang. Microsoft Visual C++, with it's non-compliant c-preprocessor, will have a stroke if it tries to build this. Therefore MinGW is required on Windows.

## Features
Project root directories can and should be marked via the command `:THLAddProject <DIR>`. They can be deleted again with `:THLRemoveProject`. Doing this makes it much easier to find the whole project, especially the 'compile_commands.json'. Go projects need to be installed for the highlighting to work. You'll also likely want to fiddle with the highlight linking to fit your favorite color scheme. Maybe someday I'll even document them.

## Why on earth is it written in C?
Because.

## No really, why on earth is it written in C?
Because reasons.

## Stability
No promises. It won't break anything though. If it crashes you can restart it with the command `:THLInit`. If it hangs, try killing it first with `:THLStop`.

## The fact that you wrote a section entitled "Stability" is why this shouldn't be in C.
Yeah, probably.
