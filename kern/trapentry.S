/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num, dpl)						\
	.text;		/* start of code segment */			\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps;									\
	.data;		 									\
	.long num, name, dpl, 0							\

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num, dpl)			\
	.text;									\
	.globl name;							\
	.type name, @function;					\
	.align 2;								\
	name:									\
	pushl $0;								\
	pushl $(num);							\
	jmp _alltraps;							\
	.data;									\
	.long num, name, dpl, 0					\

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

.data
.globl idt_handlers
idt_handlers:

TRAPHANDLER_NOEC(trap_divide, T_DIVIDE, 0)
TRAPHANDLER_NOEC(trap_debug, T_DEBUG, 0)
TRAPHANDLER_NOEC(trap_nmi, T_NMI, 0)
TRAPHANDLER_NOEC(trap_breakpoint, T_BRKPT, 3)
TRAPHANDLER_NOEC(trap_oflow, T_OFLOW, 0)
TRAPHANDLER_NOEC(trap_bound, T_BOUND, 0)
TRAPHANDLER_NOEC(trap_illop, T_ILLOP, 0)
TRAPHANDLER_NOEC(trap_device, T_DEVICE, 0)
TRAPHANDLER(trap_dblflt, T_DBLFLT, 0)
TRAPHANDLER(trap_tss, T_TSS, 0)
TRAPHANDLER(trap_segnp, T_SEGNP, 0)
TRAPHANDLER(trap_stack, T_STACK, 0)
TRAPHANDLER(trap_gpflt, T_GPFLT, 0)
TRAPHANDLER(trap_pgflt, T_PGFLT, 0)
TRAPHANDLER_NOEC(trap_fperr, T_FPERR, 0)
TRAPHANDLER(trap_align, T_ALIGN, 0)
TRAPHANDLER_NOEC(trap_mchk, T_MCHK, 0)
TRAPHANDLER_NOEC(trap_simderr, T_SIMDERR, 0)
TRAPHANDLER_NOEC(trap_syscall, T_SYSCALL, 3)

.data
.globl idt_handlers_end
idt_handlers_end:

/*
 * Lab 3: Your code here for _alltraps
 */

.text
.globl _alltraps
_alltraps:
	# Save es and ds registers
	pushl %ds
	pushl %es

    # movw %ds, %ax
    # pushl %eax
    # movw %es, %ax
    # pushl %eax

	# Save registers
	pushal

	# Set kernel data segment
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es

	# Build trap frame and call trap()
	pushl %esp
	call trap
