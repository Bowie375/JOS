## Questions

Q1: Do you have to do anything else to ensure that this I/O privilege setting is saved and restored properly when you subsequently switch from one environment to another? Why?

- No, because the eflag register is preserved in the trapframe and will be restored when the process is resumed.