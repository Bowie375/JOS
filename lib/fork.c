// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	pte_t pte;
	void *page = ROUNDDOWN(addr, PGSIZE);
	extern volatile pte_t uvpt[];
	extern volatile pte_t uvpd[];

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	if (uvpd[PDX(page)] & PTE_P)
		pte = uvpt[PGNUM(page)];
	else
		panic("pgfault: invalid address: %x", addr);
	
	if (!((err & FEC_WR) && ((uintptr_t)pte & PTE_COW))) {
		panic("pgfault: user page fault: addr: %x pte: %x err: %x", addr, pte, err);
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	if ((r = sys_page_alloc(0, (void *)PFTEMP, PTE_P | PTE_W | PTE_U)) < 0)
		panic("pgfault: sys_page_alloc: %e", r);
	
	memmove(PFTEMP, page, PGSIZE);
	
	if ((r = sys_page_map(0, (void *)PFTEMP, 0, (void *)page, PTE_P | PTE_W | PTE_U)) < 0)
		panic("pgfault: sys_page_map: %e", r);

	if ((r = sys_page_unmap(0, (void *)PFTEMP)) < 0)
		panic("pgfault: sys_page_unmap: %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r, perm;
	intptr_t va = (pn << 12);
	pte_t pte = 0;

	// LAB 4: Your code here.
	extern volatile pte_t uvpt[];
	extern volatile pte_t uvpd[];

	if (uvpd[PDX(va)] && (uvpd[PDX(va)] & PTE_P))
		pte = uvpt[pn];
	else
		return 0;

	if (!pte || !((uintptr_t)pte & PTE_P)) // The pagetable is not present
		return 0;

	// Shareable page
	if (pte & PTE_SHARE) {
		perm = pte & PTE_SYSCALL;
		if ((r = sys_page_map(0, (void *)va, envid, (void *)va, perm)) < 0)
			return r;

		return 0;
	}

	if ((pte & PTE_COW) || (pte & PTE_W)) {
		perm = ((pte & PTE_SYSCALL) & ~PTE_W) | PTE_COW | PTE_P;

		// map into child
		if ((r = sys_page_map(0, (void *)va, envid, (void *)va, perm)) < 0)
			return r;

		// map into parent
		if ((r = sys_page_map(0, (void *)va, 0, (void *)va, perm)) < 0)
			return r;

	} else {
		// Read-only (or non-writable) page; map it into the child
		// with the same user-visible permissions

		perm = (pte & PTE_SYSCALL) & ~PTE_W;
		if ((r = sys_page_map(0, (void *)va, envid, (void *)va, perm)) < 0)
			return r;

	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.

	envid_t envid;
	uint8_t *addr;
	int r;

	set_pgfault_handler(pgfault);

	envid = sys_exofork();
	if (envid < 0)
		panic("fork: sys_exofork: %e", envid);
	if (envid == 0) {
		// Child.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	for (addr = 0; (uintptr_t)addr < UTOP; addr += PGSIZE) {
		if ((uintptr_t)addr == (UXSTACKTOP - PGSIZE))
			continue;

		if ((r = duppage(envid, PGNUM(addr))) < 0)
			panic("fork: duppage: %e", r);
	}

    // Allocate new exception stack for child
    if ((r = sys_page_alloc(envid,
        (void *)(UXSTACKTOP - PGSIZE),
        PTE_U | PTE_W | PTE_P)) < 0)
        panic("fork: sys_page_alloc (child exception stack): %e", r);

    // Copy parent's pgfault upcall to child
    if ((r = sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall)) < 0)
        panic("fork: sys_env_set_pgfault_upcall: %e", r);

	// Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("fork: sys_env_set_status: %e", r);

	return envid;
}

// Challenge!
int
sfork(void)
{
	envid_t envid;
	uint8_t *addr;
	int r, perm;
	pte_t pte = 0;

	extern volatile pte_t uvpt[];
	extern volatile pte_t uvpd[];

	set_pgfault_handler(pgfault);

	envid = sys_exofork();
	if (envid < 0)
		panic("sfork: sys_exofork: %e", envid);
	if (envid == 0) {
		// Child.
		// thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	for (addr = 0; (uintptr_t)addr < UTOP; addr += PGSIZE) {
		if ((uintptr_t)addr == (UXSTACKTOP - PGSIZE))
			continue;

		if ((uintptr_t)addr == (USTACKTOP - PGSIZE)) {
			if ((r = duppage(envid, PGNUM(addr))) < 0)
				panic("sfork: duppage: %e", r);
			continue;
		}
	
		// directly share the page
		if (uvpd[PDX(addr)] && (uvpd[PDX(addr)] & PTE_P))
			pte = uvpt[PGNUM(addr)];
		else
			continue;

		if (!pte || !((uintptr_t)pte & PTE_P)) // The pagetable is not present
			continue;

		perm = pte & PTE_SYSCALL;

		if (pte & PTE_SHARE) {
			cprintf("sfork: share page %x\n", addr);
			if ((r = sys_page_map(0, (void *)addr, envid, (void *)addr, perm)) < 0)
				panic("sfork: sys_page_map: %e", r);
			continue;
		}

		if ((pte & PTE_COW) && !(pte & PTE_W)) {
            perm |= PTE_W;            // give write permission
            perm &= ~PTE_COW;         // clear COW in perms we will set
        }

        // Map into child first.
        if ((r = sys_page_map(0, (void *)addr, envid, (void *)addr, perm)) < 0)
            panic("sfork: map->child failed: %e", r);

        if (((pte & PTE_COW) && !(pte & PTE_W)) || ((pte & PTE_COW) && (perm & PTE_W))) {
            if ((r = sys_page_map(0, (void *)addr, 0, (void *)addr, perm)) < 0)
                panic("sfork: remap parent failed: %e", r);
        }

	}

    // Allocate new exception stack for child
    if ((r = sys_page_alloc(envid,
        (void *)(UXSTACKTOP - PGSIZE),
        PTE_U | PTE_W | PTE_P)) < 0)
        panic("sfork: sys_page_alloc (child exception stack): %e", r);

    // Copy parent's pgfault upcall to child
    if ((r = sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall)) < 0)
        panic("sfork: sys_env_set_pgfault_upcall: %e", r);

	// Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sfork: sys_env_set_status: %e", r);

	return envid;

}
