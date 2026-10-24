.intel_syntax noprefix
.text
.global pangu_focused_start
.type pangu_focused_start,@function
pangu_focused_start:
 and rsp,-16
 call main
 mov edi,eax
 call probe_exit
 ud2
.size pangu_focused_start,.-pangu_focused_start
.section .note.GNU-stack,"",@progbits
