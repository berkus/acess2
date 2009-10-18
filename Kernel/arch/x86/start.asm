; AcessOS Microkernel Version
; Start.asm

[bits 32]

KERNEL_BASE	equ 0xC0000000

[section .multiboot]
mboot:
    ; Multiboot macros to make a few lines later more readable
    MULTIBOOT_PAGE_ALIGN	equ 1<<0
    MULTIBOOT_MEMORY_INFO	equ 1<<1
    MULTIBOOT_HEADER_MAGIC	equ 0x1BADB002
    MULTIBOOT_HEADER_FLAGS	equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO
    MULTIBOOT_CHECKSUM	equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
	
    ; This is the GRUB Multiboot header. A boot signature
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_CHECKSUM
	dd mboot - KERNEL_BASE	;Location of Multiboot Header
	
[section .text]
[extern kmain]
[global start]
start:
	; Set up stack
	mov esp, Kernel_Stack_Top
	
	; Start Paging
	mov ecx, gaInitPageDir - KERNEL_BASE
	mov cr3, ecx
	
	mov ecx, cr0
	or	ecx, 0x80010000	; PG and WP
	mov cr0, ecx
	
	lea ecx, [.higherHalf]
	jmp ecx
.higherHalf:

	mov DWORD [gaInitPageDir], 0

	; Call the kernel
	push ebx	; Multiboot Info
	push eax	; Multiboot Magic Value
	call kmain

	; Halt the Machine
	cli
.hlt:
	hlt
	jmp .hlt

[global GetEIP]
GetEIP:
	mov eax, [esp]
	ret

[extern Proc_Clone]
[extern Threads_Exit]
[global SpawnTask]
SpawnTask:
	; Call Proc_Clone with Flags=0
	xor eax, eax
	push eax
	push eax
	call Proc_Clone
	add esp, 8	; Remove arguments from stack
	
	test eax, eax
	jnz .parent
	
	; In child, so now set up stack frame
	mov ebx, [esp+4]	; Child Function
	mov edx, [esp+8]	; Argument
	; Child
	push edx	; Argument
	call ebx	; Function
	call Threads_Exit	; Kill Thread
	
.parent:
	ret

[section .initpd]
[global gaInitPageDir]
[global gaInitPageTable]
align 0x1000
gaInitPageDir:
	dd	gaInitPageTable-KERNEL_BASE+3	; 0x00
	times 1024-256-1	dd	0
	dd	gaInitPageTable-KERNEL_BASE+3	; 0xC0
	times 256-1	dd	0
align 0x1000
gaInitPageTable:
	%assign i 0
	%rep 1024
	dd	i*0x1000+3
	%assign i i+1
	%endrep
[global Kernel_Stack_Top]
ALIGN 0x1000
	times 1024	dd	0
Kernel_Stack_Top:
	
