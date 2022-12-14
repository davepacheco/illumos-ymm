	.text
.globl  read_ymm0
	.type read_ymm0, @function
read_ymm0:
	pushq 	%rbp
	movq 	%rsp, %rbp
	vmovdqu %ymm0,(%rdi)
	leave
	ret

.globl  write_ymm0
	.type write_ymm0, @function
write_ymm0:
	pushq 	%rbp
	movq 	%rsp, %rbp
	vlddqu 	(%rdi),%ymm0
	leave
	ret
