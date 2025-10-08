## Part1: PC Bootstrap

### The ROM BIOS
1. Excercise 2:
    - It is checking all the hardwares connected to this machine.

## Part2: The Boot Loader
1. At what point does the processor start executing 32-bit code? What exactly causes the switch from 16- to 32-bit mode?    
    - the processer enters protected mode after
    
            movl    %cr0, %eax
                7c24:	20 c0                	and    %al,%al
            orl     $CR0_PE_ON, %eax
                7c26:	66 83 c8 01          	or     $0x1,%ax
            movl    %eax, %cr0
                7c2a:	0f 22 c0             	mov    %eax,%cr0

    but the instruction decoder is still running in 16-bit mode until you reload CS.

    - the processe start executing 32-bit code after: `7c2d:	ea 32 7c 08 00 66 b8  ljmp  $0xb866,$0x87c32`

    - CR0 is one of the control registers, the obit of it controls real/protected mode switch. 
    - segment descriptors in GDT contains flag bits that indicates if the instructions should be executed in 16-bit or 32-bit mode.

2. What is the last instruction of the boot loader executed, and what is the first instruction of the kernel it just loaded?
    - last instruction of the boot loader: `0x7d63: call *0x10018`
    - first instruction of the kernel: `0x10000c: movw $0x1234,0x472`

3. Where is the first instruction of the kernel?
    - 0x10000c


4. How does the boot loader decide how many sectors it must read in order to fetch the entire kernel from disk? Where does it find this information?
    - it reads the number of segmentations from the GDT(Global Descriptor Table) to determine the number of sectors to read.

### Loding the Kernel
1. Excercise 5: 
    - any command that jmps to a certain address will fail, the first one should be: `7c22: 7c 0f jl 7c33 <protcseg+0x1>`

2. Excercise 6:
    - 8 words at the point BIOS enters boot loader: 

            0x7c00: 0xc031fcfa      0xc08ed88e      0x64e4d08e      0xfa7502a8
            0x7c10: 0x64e6d1b0      0x02a864e4      0xdfb0fa75      0x010f60e6

    - 8 words at the point boot loader enters kernel:

            0x10000c:       0x7205c766      0x34000004      0x7000b812      0x220f0011
            0x10001c:       0xc0200fd8      0x0100010d      0xc0220f80      0x10002fb8

    - 8 words at 0x100000:

            0x100000:       0x1badb002      0x00000000      0xe4524ffe      0x7205c766
            0x100010:       0x34000004      0x7000b812      0x220f0011      0xc0200fd8

---
## Part3: The Kernel

### Using virtual memory to work around position dependence
1. Exercises 7: the first instruction after `f010002d: ff e0 jmp *%eax` will fail because there is no code at that high address without address mapping.


### Formatted Printing to the Console
1. Explain the interface between printf.c and console.c. Specifically, what function does console.c export? How is this function used by printf.c?
    - console.c exports a function called `cputchar` which is used by printf.c to output characters to the console.
    - printf.c calls `putch` which wraps `cputchar`  to output each character of the formatted string.

2. Explain the following from console.c:
    ```c
    1  if (crt_pos >= CRT_SIZE) {
    2          int i;
    3          memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
    4          for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
    5                  crt_buf[i] = 0x0700 | ' ';
    6          crt_pos -= CRT_COLS;
    7  }
    ```
    this part is used to release a column from buffer and move the remaining columns up by one column.

4. He110 World. If change to big endian, than both 57616 and i should be changed to other number.

5. x=3, y=xxxx. Because y points to random memory.

6. make fmt argument comes at last, so that it will still be possible to locate the va-list based on the address of named argument: fmt.


### The Stack
1. Excercise 9: Determine where the kernel initializes its stack, and exactly where in memory its stack is located. How does the kernel reserve space for its stack? And at which "end" of this reserved area is the stack pointer initialized to point to?
    In entry.S, the kernel sets up stack in .data section: 

        .data
        	.p2align	PGSHIFT		# force page alignment
        	.globl		bootstack
        bootstack:
        	.space		KSTKSIZE
        	.globl		bootstacktop   
        bootstacktop:

    after linkage, the kernel is at 0xf0118000, which can be infered from the asm code:
    
    	movl	$(bootstacktop),%esp
        f0100034:	bc 00 80 11 f0       	mov    $0xf0118000,%esp

    The stack should actually be loaded at 0x110000 in physical memory and spans from 0x110000 to 0x117FFF, taking up 8 PGSIZE. The stack pointer is initialized to 0x118000.

2. Excercise 10:
    - 28 bytes are pushed to the stack: %ebp, %ebx, reserve 20 bytes for local variable and parameters to the nested test_backtrace function