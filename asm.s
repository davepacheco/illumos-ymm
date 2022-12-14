	.text
.globl  read_ymm0
	.type read_ymm0, @function
read_ymm0:
	vmovdqu %ymm0,(%rdi)

.globl  write_ymm0
	.type write_ymm0, @function
write_ymm0:
	pushq 	%rbp
	movq 	%rsp, %rbp
	pushq 	%rdi
	vlddqu  (%rsp),%ymm0
	leave
	ret
