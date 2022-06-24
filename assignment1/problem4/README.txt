INSTRUCTIONS:
Complete the missing code parts marked by TODO 1,
TODO 2, TODO 3, TODO 4, and TODO 5

REQUIREMENTS:
1. A Linux distro OS like Ubuntu, fedora, or red hat
2. Type "uname -a" and make sure that the Linux kernel version
that you have is 5.0.0 or newer
3. You must have a sudo access in the machine.

INSTALLATION:
To install the kernel module, type the following commands:
1. make
2. sudo insmod my_module.ko

TESTING THE MODULE:
1. Compile the test_app.c file using your c compiler.
For example, if your compiler is gcc,
gcc test_app.c -o test_app

2. Run the compiled binary with the following command:
sudo ./test_app

Choose option 1 if you want to write a string to the memory
block in kernel space. Choose option 2 if you want to read
the content of the memory block. Choose option 3 if you want
to exit the program and thus frees the memory block.

UNINSTALLATION/REMOVAL:
To uninstall the kernel module, type the following command:
sudo rmmod my_module.ko

