## Questions

Q1: Compare `kern/mpentry.S` side by side with boot/boot.S. Bearing in mind that `kern/mpentry.S` is compiled and linked to run above KERNBASE just like everything else in the kernel, what is the purpose of macro MPBOOTPHYS? Why is it necessary in kern/mpentry.S but not in boot/boot.S? In other words, what could go wrong if it were omitted in kern/mpentry.S?

- `boot/boot.S` is both compiled and loaded at low physical memory (1 MB), so its link address == its load address. `kern/mpentry.S` is linked to kernel addresses (above KERNBASE) but is physically loaded at low memory so APs can start executing it. Because the link address ≠ load address, the code would jump/compute wrong pointers unless all addresses are converted from kernel virtual addresses to physical addresses. MPBOOTPHYS does that translation. If we remove it, the AP bootstrap code will dereference virtual addresses before paging is enabled, which will cause a instant crash.

Q2: It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock.

- Because interrupts, faults, and exceptions can occur asynchronously on any CPU—even when that CPU is not holding the big kernel lock. When an interrupt/trap occurs, the CPU immediately pushes register state onto its own kernel stack, before any locking happens. If multiple CPUs shared one kernel stack, their trap frames would overwrite each other, corrupting execution and crashing the system.

Q3: In your implementation of env_run() you should have called lcr3(). Before and after the call to lcr3(), your code makes references (at least it should) to the variable e, the argument to env_run. Upon loading the %cr3 register, the addressing context used by the MMU is instantly changed. But a virtual address (namely e) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer e be dereferenced both before and after the addressing switch?

- Because the `e` is in the kernel or at virtual address  `0xf-------`. This address space is mapped to the same region both for kernel and user address space. So addressing switch does not affect the manipulation of `e`.

Q4: Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?

- Because registers reflects the running state of the CPU, so they need to be saved on order to be restored latter. The registers will be saved in `trapentry.S` and restored in `env_pop_tf()`

---

## Challenge
I finished the challenge in _Implementing Copy-on-Write Fork_ section, which is:

    Challenge! Implement a shared-memory fork() called sfork(). This version should have the parent and child share all their memory pages (so writes in one environment appear in the other) except for pages in the stack area, which should be treated in the usual copy-on-write manner. Modify user/forktree.c to use sfork() instead of regular fork(). Also, once you have finished implementing IPC in part C, use your sfork() to run user/pingpongs. You will have to find a new way to provide the functionality of the global thisenv pointer.

- Implement details:
    - First, thisenv global variable now cannot identify differet envs, so I add a macro `THISENV` in `inc/lib.h` to get the current env:

        ```c
        #define THISENV (&envs[ENVX(sys_getenvid())])
        ``` 

    - Second, for most pages excluding pages in user stack and exception stack, I remove the `COW` flag and make it directly writable before mapping into both child and parent. Then modifying these pages won't trigger page fault:

        ```c
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
        ```

    - Third, I modify `ipc.c` to use `THISENV` instead of `thisenv` in `ipc_recv()`, so now it can support both fork()ed and sfork()ed envs:

        ```c
        int32_t
        ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
        {
        	// LAB 4: Your code here.
        
        	int errnum = 0;
        
        	if (pg == NULL)
        		pg = (void*)-1;
        
        	errnum = sys_ipc_recv(pg);
        
        	if (errnum < 0) {
        		if (from_env_store)
        			*from_env_store = 0;
        		if (perm_store)
        			*perm_store = 0;
        		return errnum;
        	}
        
        	if (from_env_store)
        		*from_env_store = THISENV->env_ipc_from;
        	if (perm_store)
        		*perm_store = THISENV->env_ipc_perm;
        
        	return THISENV->env_ipc_value;
        }
        ```