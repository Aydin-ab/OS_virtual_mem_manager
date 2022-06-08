# OS scheduler
Homework for OS course at NYU. Basic virtual memory manager to replicate OS behavior in C++

**Grade : 100/100 A**

## HOW TO USE
Compile the code with the ```make``` command
Execute the program with ```mmu –f<num_frames> -a<algo> [-o<options>] inputfile randomfile```.  
The algorithms available are FIFO(-aF), Random(-aR), Clock(-aC), Enhanced Second Chance/NRU(-aE), Aging(-aA) and Working Set(-aW).  
The -o flag has options O (print output), P (print page table), F (print frame table), S (print statistics).  
 e.g. ./mmu -f4 -ac –oOPFS infile rfile selects the Clock Algorithm and creates output for operations, final page table content and final frame table content and summary line

The output goes to the standard output.
Given a list of input files and a random file, you can use the ```runit.sh``` script to run the program on each of them and put the outputs in a output directory. Use the runit.sh script inside the script directory like that : ```./runit.sh <inputs_dir> <output_dir> mmu``` and change the arguments of the program inside the script


## CONTEXT
In this repo I implement/simulate the operation of an Operating System’s Virtual Memory Manager which maps the virtual address spaces of multiple processes onto physical frames using page table translation. 

I will assume multiple processes, each with its own virtual address space of exactly 64 virtual pages (yes this is small compared to the 1M entries for a full 32-address architecture), but the principal counts. As the sum of all virtual pages in all virtual address spaces may exceed the number of physical frames of the simulated system, paging needs to be implemented. 

The number of physical page frames varies and is specified by a program option, It supports up to 128 frames. Implementation is in C/C++.

The input to the program will be a comprised of:
1. the number of processes (processes are numbered starting from 0)
2. a specification for each process’ address space is comprised of  
    i. the number of virtual memory areas / segments (aka VMAs)  
    ii. specification for each said VMA comprised of 4 numbers:  
    starting_virtual_page - ending_virtual_page - write_protected[0/1] - filemapped[0/1]”

Since it is required that the VMAs of a single address space do not overlap, this property is guaranteed for all provided input files. However, there can potentially be holes between VMAs, which means that not all virtual pages of an address space are valid (i.e. assigned to a VMA). Each VMA is comprised of 4 numbers.
start_vpage, end_vpage, write_protected, file_mapped
(note the VMA has (end_vpage – start_vpage + 1) virtual pages ) binary whether the VMA is write protected or not
binary to indicate whether the VMA is mapped to a file or not

The process specification is followed by a sequence of “instructions” and optional comment lines (see following example). An instruction line is comprised of a character (‘c’, ‘r’, ‘w’ or ‘e’) followed by a number.
- “c <procid>”: specifies that a context switch to process #<procid> is to be performed. It is guaranteed that the first instruction will always be a context switch instruction, since I must have an active pagetable in the MMU (in real systems). 
- “r <vpage>”: implies that a load/read operation is performed on virtual page <vpage> of the currently running process.
- “w <vpage>”: implies that a store/write operation is performed on virtual page <vpage> of the currently running process. 
- “e <procid>”: current process exits

##### example of an instruction sequence ###### c0
  r 32  
  w9  
  r0 r 20 r 12
