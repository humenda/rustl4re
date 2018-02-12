" Vim syntax file for objdump disassembly output
" Language:    Objdump output
" Maintainer:  Adam Lackorzynski <adam@lackorzynski.de>
" Last Change: 2008 Nov 08

if exists("b:current_syntax")
  finish
endif

syn clear
syn case match

" enable to have highlighted c code
"runtime! syntax/c.vim
"unlet b:current_syntax

syn match  objdumpInsn               /^\s*[0-9a-fA-F]\+:.*/
syn match  objdumpLabelFunc          /^[0-9a-fA-F]\+\s<.\+>:$/
syn match  objdumpLabelCode          /^\w.\+():$/
syn match  objdumpPath               /^\/.\+:[0-9]\+\( (discriminator [0-9]\+)\)\?$/

hi def link objdumpInsn      Type
hi def link objdumpLabelFunc Comment
hi def link objdumpLabelCode String
hi def link objdumpPath      Statement

let b:current_syntax = "objdump"
