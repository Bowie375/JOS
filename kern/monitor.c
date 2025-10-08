// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Track back all functions before this one", mon_backtrace },
	{ "showmappings", "Show virtual memory mappings", mon_showmappings },
	{ "setperm", "Set the permission of a virtual page", mon_setperm },
	{ "dumpmem", "Dump physical memory to console", mon_dumpmem }
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	
	uint32_t ebp, eip, arg;
	int ret;
	struct Eipdebuginfo info;

	cprintf("\033[33;44mStack backtrace:\033[0m\n");
	ebp = read_ebp();
	while(ebp){
		// print stack information
		eip = *(uint32_t*)(ebp + 4);
		cprintf("  ebp %08x eip %08x args", ebp, eip);
		for(int i = 0; i < 5; ++i){
			arg = *(uint32_t*)(ebp + 8 + i*4);
			cprintf(" %08x", arg);
		}
		cprintf("\n");

		// print function information
		ret = debuginfo_eip(eip, &info);
		cprintf("\t%s:%d: %.*s+%d\n", 
				info.eip_file, 
				info.eip_line, 
				info.eip_fn_namelen, 
				info.eip_fn_name, 
				eip - info.eip_fn_addr
			);

		ebp = *(uint32_t*)ebp;
	}

	return 0;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t va_start, va_end, va;
	pte_t *pte;
	struct PageInfo *pp;
	int pde_flags, pte_flags;

	if (argc != 3) {
		cprintf("Usage: showmappings <va_start> <va_end>\n");
		return 0;
	}

	va_start = ROUNDDOWN(strtol(argv[1], NULL, 0), PGSIZE);
	va_end = ROUNDUP(strtol(argv[2], NULL, 0), PGSIZE);

	cprintf("\033[33;44mVirtual memory mappings:\033[0m\n");
	cprintf("  VA start    VA end       PD      PDX      PT      PTX     PA     PDE flags  PTE flags\n");
	for (va = va_start; va < va_end; va += PGSIZE) {
		pte = pgdir_walk(kern_pgdir, (void*)va, 0);
		if (!pte) {
			cprintf("  %08x - %08x: not mapped\n", va, va + PGSIZE);
			continue;
		}
		pp = pa2page(PTE_ADDR(*pte));
		pde_flags = kern_pgdir[PDX(va)];
		pte_flags = *pte;
		cprintf("  %08x - %08x:  %08x   %03x   %08x   %03x  %08x     %c%c%c        %c%c%c\n",
			va, va + PGSIZE, kern_pgdir, PDX(va), PTE_ADDR(pde_flags), PTX(va), PTE_ADDR(*pte),
			pde_flags & 1 ? 'p' : '-', pde_flags & 2 ? 'w' : '-', pde_flags & 4 ? 'u' : '-',
			pte_flags & 1 ? 'p' : '-', pte_flags & 2 ? 'w' : '-', pte_flags & 4 ? 'u' : '-');
	}

	return 0;
}

int 
mon_setperm(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t va;
	uint32_t perm;
	pte_t *pte;

	if (argc != 3) {
		cprintf("Usage: setperm <va> <perm: [0, 7]>\n");
		return 0;
	}

	va = ROUNDDOWN(strtol(argv[1], NULL, 0), PGSIZE);
	perm = strtol(argv[2], NULL, 0) & 0x7;

	pte = pgdir_walk(kern_pgdir, (void*)va, 1);
	if (!pte) {
		cprintf("Failed to map virtual address %08x\n", va);
		return 0;
	}
	*pte = PTE_ADDR(*pte) | perm;
	cprintf("Set permission of virtual page %08x to %c%c%c\n", 
		va, perm & 1 ? 'p' : '-', perm & 2 ? 'w' : '-', perm & 4 ? 'u' : '-');

	return 0;
}

int
mon_dumpmem(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t start, end;

	if (argc != 4) {
		cprintf("Usage: dumpmem <mode: ['v' | 'p']> <start> <end>\n");
		return 0;
	}

	start = strtol(argv[2], NULL, 0);
	end = strtol(argv[3], NULL, 0);

	if (strcmp(argv[1], "p") == 0) {
		start = (uintptr_t)KADDR(start);
		end = (uintptr_t)KADDR(end);
	}
	for (uint32_t i = start; i < end; i += 16) {
		cprintf("%08x: ", i);
		for (uint32_t j = 0; j < 16; j+=4) {
			if (i + j >= end) {
				cprintf("\n");
				break;
			}

			cprintf(
				"%02x %02x %02x %02x", 
				*(unsigned char*)(i + j), *(unsigned char*)(i + j + 1), 
				*(unsigned char*)(i + j + 2), *(unsigned char*)(i + j + 3)
			);
			cprintf("%s", (j == 12) ? "\n" : " / ");
		}
	}

	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
