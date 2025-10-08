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


#### Challenges
I finished the second challenge in lab2.

1. Challenge 2: Extend the JOS kernel monitor with commands to:
	- Display in a useful and easy-to-read format all of the physical page mappings (or lack thereof) that apply to a particular range of virtual/linear addresses in the currently active address space. For example, you might enter 'showmappings 0x3000 0x5000' to display the physical page mappings and corresponding permission bits that apply to the pages at virtual addresses 0x3000, 0x4000, and 0x5000.

		- For every page in the given address range, I utilize `page_walk` to get the page table entry, and display relative informations such as PD, PDX, PT, PTX, PA and permission bits, exmaple output:

			```
			K> showmappings 0xf0100fec 0xf0101007
			Virtual memory mappings:
			  VA start    VA end       PD      PDX      PT      PTX     PA     PDE flags  PTE flags
			  f0100000 - f0101000:  f0121000   3c0   003ff000   100  00100000     pwu        pw-
			  f0101000 - f0102000:  f0121000   3c0   003ff000   101  00101000     pwu        pw-
			```

	- Explicitly set, clear, or change the permissions of any mapping in the current address space.

		- I utilize `page_walk` to get the page table entry, and then set or clear the permission bits by directly modify the last few bits of the entry. Example output:

			```
			K> showmappings 0xf0100fec 0xf0101007
			Virtual memory mappings:
			  VA start    VA end       PD      PDX      PT      PTX     PA     PDE flags  PTE flags
			  f0100000 - f0101000:  f0121000   3c0   003ff000   100  00100000     pwu        pw-
			  f0101000 - f0102000:  f0121000   3c0   003ff000   101  00101000     pwu        pw-
			
			K> setperm 0xf0100fec 4
			Set permission of virtual page f0100000 to --u
			
			K> showmappings 0xf0100fec 0xf0101007
			Virtual memory mappings:
			  VA start    VA end       PD      PDX      PT      PTX     PA     PDE flags  PTE flags
			  f0100000 - f0101000:  f0121000   3c0   003ff000   100  00100000     pwu        --u
			  f0101000 - f0102000:  f0121000   3c0   003ff000   101  00101000     pwu        pw-
			```

	- Dump the contents of a range of memory given either a virtual or physical address range. Be sure the dump code behaves correctly when the range extends across page boundaries!

		- For physical address range, I first use `PADDR` to convert it to a virtual address, then I dereference the address range byte by byte using `*(char *)va`. In this way, address range across page boundaries can be correcly handled. For virtual address range, I simply dereference the address range byte by byte using `*(char *)va`. Example output:

			```
			K> dumpmem v 0xf0100000 0xf0100013
			f0100000: 02 b0 ad 1b / 00 00 00 00 / fe 4f 52 e4 / 66 c7 05 72
			f0100010: 04 00 00 34 /
			```