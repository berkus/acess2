/*
 * Acess2 x86_64 Project
 * - Error Handling
 */
#include <acess.h>
#include <proc.h>

// === PROTOTYPES ===
void	Error_Handler(tRegs *Regs);

// === GLOBALS ==
const char * const csaERROR_NAMES[] = {
	"Divide By Zero", "Debug", "NMI Exception", "INT3",
	"INTO", "Out of Bounds", "Invalid Opcode", "Coprocessor not avaliable",
	"Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
	"Stack Fault Exception", "GPF", "#PF", "Reserved",
	"Floating Point Exception", "Alignment Check Exception", "Machine Check Exception",	"Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved"
	};

// === CODE ===
void Error_Handler(tRegs *Regs)
{
	Uint	cr;
	
	Debug_KernelPanic();
	
	Warning("CPU Error %i - %s, Code: 0x%x",
		Regs->IntNum, csaERROR_NAMES[Regs->IntNum], Regs->ErrorCode);
	Warning(" CS:RIP = 0x%04x:%016x", Regs->CS, Regs->RIP);
	Warning(" SS:RSP = 0x%04x:%016x", Regs->SS, Regs->RIP);
	Warning(" RFLAGS = 0x%016x", Regs->RFlags);
	Warning(" EAX %016x ECX %016x EDX %016x EBX %016x",
		Regs->RAX, Regs->RCX, Regs->RDX, Regs->RBX);
	Warning(" ESP %016x EBP %016x ESI %016x EDI %016x",
		Regs->RSP, Regs->RBP, Regs->RSP, Regs->RDI);
	
	Warning(" FS %04x GS %04x", Regs->FS, Regs->GS);
	
	
	// Control Registers
	__asm__ __volatile__ ("mov %%cr0, %0":"=r"(cr));
	Warning(" CR0 0x%08x", cr);
	__asm__ __volatile__ ("mov %%cr2, %0":"=r"(cr));
	Warning(" CR2 0x%08x", cr);
	__asm__ __volatile__ ("mov %%cr3, %0":"=r"(cr));
	Warning(" CR3 0x%08x", cr);
	__asm__ __volatile__ ("mov %%cr4, %0":"=r"(cr));
	Warning(" CR4 0x%08x", cr);
	
	switch( Regs->IntNum )
	{
	case 6:	// #UD
		Warning(" Offending bytes: %02x %02x %02x %02x",
			*(Uint8*)Regs->RIP+0, *(Uint8*)Regs->RIP+1,
			*(Uint8*)Regs->RIP+2, *(Uint8*)Regs->RIP+3);
		break;
	}
	
	for(;;)
		__asm__ __volatile__ ("hlt");
}
