## CONTENT IN THIS REPO

This repo adds a simple GUI interface to the xv6 system (the x86 version 
by October 2020). It is a single person final project for the course CS510
offered by Duke University.

## ACKNOWLEDGMENTS

Please see [xv6 public repo](https://github.com/mit-pdos/xv6-public) for the 
acknowledgments of the public repo. For this repo, it borrows code from 
[Themis_GUI](https://github.com/YueDayu/Themis_GUI) (characters.c, gui.c, mouse.c, kbd.c, msg.c). 

## BUILDING AND RUNNING XV6

To build xv6 on an x86 ELF machine (like Linux or FreeBSD), run
"make". On non-x86 or non-ELF machines (like OS X, even on x86), you
will need to install a cross-compiler gcc suite capable of producing
x86 ELF binaries (see https://pdos.csail.mit.edu/6.828/).
Then run "make TOOLPREFIX=i386-jos-elf-". Now install the QEMU PC
simulator and run "make qemu".

## GUI ARCHITECTURE

![Image of GUI arch](/xv6_gui_architecture.png)


