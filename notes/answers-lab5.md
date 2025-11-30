## Questions

Q1: Do you have to do anything else to ensure that this I/O privilege setting is saved and restored properly when you subsequently switch from one environment to another? Why?

- No, because the eflag register is preserved in the trapframe and will be restored when the process is resumed.

---

## Challenges
I finished the challenge in _Spawning Processes_ section, which is:

    Challenge! Implement mmap-style memory-mapped files and modify spawn to map pages directly from the ELF image when possible.

Implement details:

- In original `spawn.c`, there is a function called `map_segment`, which copies the segments into the new process's address space. Now I don't copy the read only parts anymore, instead, I directly map them from the ELF image, below is the function I use:

    ```c
    static int
    map_read_segment(envid_t child, uintptr_t va, size_t memsz,
    	int fd, size_t filesz, off_t fileoffset, int perm)
    {
    	int i, r;
    	static envid_t fsenv;
    	if (fsenv == 0)
    		fsenv = ipc_find_env(ENV_TYPE_FS);

    	if ((i = PGOFF(va))) {
    		va -= i;
    		memsz += i;
    		filesz += i;
    		fileoffset -= i;
    	}

    	for (i = 0; i < memsz; i += PGSIZE) {
    		if (i >= filesz) {
    			// allocate a blank page
    			if ((r = sys_page_alloc(child, (void*) (va + i), perm)) < 0)
    				return r;
    		} else {
    			// from file
    			if ((r = seek(fd, fileoffset + i)) < 0)
    				return r;
    			if ((r = read_map(fd, MIN(PGSIZE, filesz-i))) < 0) // <-- this is the core modification
    				return r;
    			if ((r = sys_page_map(fsenv, (void *)r, child, (void*) (va + i), perm)) < 0)
    				panic("spawn: sys_page_map data: %e", r);
    		}
    	}

    	return 0;
    }
    ```

- To achieve the `read_map` function used in the first step, I modified the `dev` struct to have another hook:

    ```c
    struct Dev {
    	int dev_id;
    	const char *dev_name;
    	ssize_t (*dev_read)(struct Fd *fd, void *buf, size_t len);
    	ssize_t (*dev_read_map)(struct Fd *fd, size_t len); // <-- this is the new hook
    	ssize_t (*dev_write)(struct Fd *fd, const void *buf, size_t len);
    	int (*dev_close)(struct Fd *fd);
    	int (*dev_stat)(struct Fd *fd, struct Stat *stat);
    	int (*dev_trunc)(struct Fd *fd, off_t length);
    };
    ```

- Then for the `file` dev type, I implemented the `devfile_read_map` function, which use ipc to ask the filesystem environment for the pointer to the block. This ipc will take in the current file, the bytes to read, and return a virtual address to the block containing the data.

    ```c
    static ssize_t
    devfile_read_map(struct Fd *fd, size_t n)
    {
    	int r;

    	fsipcbuf.read.req_fileid = fd->fd_file.id;
    	fsipcbuf.read.req_n = n;
    	if ((r = fsipc(FSREQ_READ_MAP, NULL)) < 0)
    		return r;
    	assert(r <= 0xD0000000);
    	assert(r >= 0x10000000);
    	return r;	
    }
    ```

- In detail, the server side of ipc is, which don't really read the block, but just use `file_get_block` to obtain the virtual address of this block:

    ```c
    int
    serve_read_map(envid_t envid, union Fsipc *ipc)
    {
    	struct Fsreq_read *req = &ipc->read;
    	struct Fsret_read *ret = &ipc->readRet;
    	struct OpenFile *o;
    	int r;
    	size_t count;
    	char * blk, blk0;

    	if (debug)
    		cprintf("serve_read_map %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

    	assert(req->req_n <= PGSIZE);

    	if ((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
    		return r;

    	count = MIN(req->req_n, o->o_file->f_size - o->o_fd->fd_offset);
    	if (count > PGSIZE) {
    		cprintf("serve_read: req_n too large\n");
    		return -E_INVAL;
    	} else if (count < 0) {
    		cprintf("serve_read: req_n is negative\n");
    		return -E_INVAL;
    	}

    	if ((r = file_get_block(o->o_file, o->o_fd->fd_offset / BLKSIZE, &blk)) < 0)
    		return r;
    	memcpy(blk, blk, 1); // make sure the block is mapped

    	return (int)blk;
    }
    ```

- After implementing these functions, I can still pass all the tests.