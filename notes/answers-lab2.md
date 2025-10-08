#### Part 1: Physical Page Management

#### Part 2: Virtual Memory

1. Question 1: Assuming that the following JOS kernel code is correct, what type should variable x have, `uintptr_t` or `physaddr_t`?

    ```c
    	mystery_t x;
    	char* value = return_a_pointer();
    	*value = 10;
    	x = (mystery_t) value;
    ```

    - `uintptr_t`, because `value` is a virtual address, and we cast it to `mystery_t` to store it in `x`.

#### Part 3: Kernel Address Space

1. Question 2: What entries (rows) in the page directory have been filled in at this point? What addresses do they map and where do they point? In other words, fill out this table as much as possible:

	-   |Entry|Base Virtual Address|Physical Pages|Points to (logically)|
		|-----|--------------------|--------------|------------|
		|956  |0xef000000		   |00122-00161	  |pages|
		|957  |0x00000000		   |all pages     |kern_pgdir itself|
		|959  |0xefc00000		   |00116-0011d	  |kernel stack|
		|960-1023  |0xf0000000		   |00000-0ffff	  |Remapped Memory|


2. Question 3: We have placed the kernel and user environment in the same address space. Why will user programs not be able to read or write the kernel's memory? What specific mechanisms protect the kernel memory?
	- Because of permission bits. The kernel memory is marked as RW / --, so user programs cannot access it.

3. Question 4: What is the maximum amount of physical memory that this operating system can support? Why?
	- The maximum amount of physical memory that JOS can support is 4GB. Because a page directory has 1024 entrys, a page table has 1024 entrys and a page is 4KB, so kernel can manage 1024 * 1024 * 4KB = 4GB of physical memory.

4. Question 5: How much space overhead is there for managing memory, if we actually had the maximum amount of physical memory? How is this overhead broken down?
	- we need 1024 page tables and a page directory. This overhead is broken down by only create the page table when that physical page is used.

5. Question 6: Revisit the page table setup in kern/entry.S and kern/entrypgdir.c. Immediately after we turn on paging, EIP is still a low number (a little over 1MB). At what point do we transition to running at an EIP above KERNBASE? What makes it possible for us to continue executing at a low EIP between when we enable paging and when we begin running at an EIP above KERNBASE? Why is this transition necessary?
	- The transition happens after:

		```c
			mov	$relocated, %eax
			jmp	*%eax

		relocated:
		```

		this is possible because the entry_pagedir maps [0, 4MB), and [KERNBASE, KERNBASE+4MB) to the same physical page. So when we enable paging, we can access the kernel code at both low address and high kernel address.