
[BITS 32]

[section .multiboot]
mboot:
	MULTIBOOT_MAGIC	equ	0x1BADB002
	dd	MULTIBOOT_MAGIC

[extern start64]

[section .text]
[global start]
start:
	; Enable PAE
	mov eax, cr4
	or eax, 0x80|0x20
	mov cr4, eax

	; Load PDP4
	mov eax, gInitialPML4
	mov cr3, eax

	; Enable long/compatability mode
	mov ecx, 0xC0000080
	rdmsr
	or ax, 0x100
	wrmsr

	; Enable paging
	mov eax, cr0
	or eax, 0x80000000
	mov cr0, eax

	; Load GDT
	lgdt [gGDTPtr]
	mov ax, 0x10
	mov ss, ax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	jmp 0x08:start64

[section .data]
gGDT:
	dd	0,0
	dd	0x00000000, 0x00209800	; 64-bit Code
	dd	0x00000000, 0x00009000	; 64-bit Data
gGDTPtr:
	dw	$-gGDT-1
	dd	gGDT
	dd	0

[section .padata]
gInitialPML4:	; 256 TiB
	dd	gInitialPDP + 3, 0	; Identity Map Low 4Mb
	times 256-1 dq	0
	dd	gInitialPDP + 3, 0	; Identity Map Low 4Mb to kernel base
	times 256-1 dq 0

gInitialPDP:	; 512 GiB
	dd	gInitialPD + 3, 0
	times 511 dq	0

gInitialPD:	; 1 GiB
	dd	gInitialPT1 + 3, 0
	dd	gInitialPT2 + 3, 0

gInitialPT1:	; 2 MiB
	%assign i 1
	%rep 512
	dq	i*4096+3
	%assign i i+1
	%endrep
gInitialPT2:	; 2 MiB
	%assign i 512
	%rep 512
	dq	i*4096+3
	%assign i i+1
	%endrep


