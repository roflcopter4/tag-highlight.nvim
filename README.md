# tag-highlight.nvim
This was originally intended to be a simple tag highlighting plugin for Neovim, much like Neotags (https://github.com/c0r73x/neotags.nvim) implemented entirely in C. It would run ctags on a project (or individual file), scan through the generated tags, and create highlight definitions for Neovim based that data. In addition, it would give the tags to Neovim directly, doubling as a tag file manager. This functionality is now fully implemented and largely working.

After having gotten this working I decided it wasn't really good enough to justify all the work put into writing this in C. Ctags isn't perfect at parsing languages, and it can generate conflicting tags, especially for languages with many different sorts of namespaces. Furthermore, it is relatively difficult to determine where the tags ought to be visible. Scanning a whole directory for tags will generate many that should only appear in some files, but knowing which of these are valid would require something close to actually parsing the language, which is much more complicated than this plugin was capable of. With that problem in mind, I decided that it should be capable of properly parsing languages to offer much more detailed and correct highlighting.

I'm not crazy enough to write my own parsers. At the moment, only implementations for C and C++ are functional, using libclang to do the parsing. With the full might of clang behind it, the highlighting is always correct, detailed, and never fooled by tricky namespace problems. It is a proper semantic highlighter of the sort you'd expect from an IDE.

## Why on earth is it written in C?
Because.

## No really, why on earth is it written in C?
Basically because I thought it would be a good way to learn a lot about C in general, and handling multithreading in particular. It was exactly that. Also because I like a challange, and writing something like a vim plugin in one of the least appropriate languages imaginable qualifies.

## Why so many files?
I genuinely couldn't figure out how to use the msgpack-c library. Maybe I'm just an idiot, but after hours of reading manuals and fiddling I just gave up and wrote my own implementation of the msgpack protocol. A lot of the code here is devoted to that, and to wrappers for Neovim api functions that handle creating and reading the msgpack messages.

## Stability
At present, it mostly works. Most of the obvious problems have been solved, so it at least shouldn't randomly segfault, but I can't guarentee that rather abhorrent bugs still exist. In particular, if libclang ever gets terribly confused and throws a general error, it is at present easiest to kill the plugin and start over, so any serious errors within libclang will trigger a crash. Synchronization has also been a problem that I believe is mostly solved.

## Compatibility
The plugin works only with Neovim. It should compile in any unix-ish environment fairly easily, although it works best in Linux due to using the Linux-specific `futex` system call. Workarounds are available for other systems though. At least version 7 of libclang is required to compile. It may be necessary to provide the location of `libclang.so` if it is installed somewhere odd enough that cmake can't find it.

The plugin has also successfully been compiled in Windows, under MinGW. Visual Studio is not supported. I used gcc extensions fairly liberally, so gcc or clang are required to compile. Under MinGW it should work without much fuss.
