// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user initData:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "machine.h"
#include "system.h"
#include "syscall.h"
#include "noff.h"

//#define TLB_FIFO
#define TLB_LRU

int TLBVictim(){
	for(int i = 0; i < TLBSize; ++i)
		if(!machine->tlb[i].valid)
			return i;

#ifdef TLB_FIFO
	for(int i = TLBSize - 1; i > 0; --i)
    	machine->tlb[i] = machine->tlb[i - 1];  
	return 0;
#endif

#ifdef TLB_LRU
	int pos = 0, max = 0;
	for(int i = 0; i < TLBSize; ++i){
		if(machine->tlb[i].lrutime > max){
			max = machine->tlb[i].lrutime;
			pos = i;
		}
	}
	return pos;
#endif
}
void PageTableFetch(int vpn){
	OpenFile *openfile = fileSystem->Open(currentThread->getFileName());
    ASSERT(openfile != NULL);
    int fileAddr, tsize, pos = machine->bitmap->Find();
    if(pos == -1){
    	int max = 0;
    	pos = 0;
		for(int i = 0; i < machine->pageTableSize; ++i){
			if(machine->pageTable[i].valid && machine->pageTable[i].lrutime > max){
				max = machine->pageTable[i].lrutime;
				pos = i;
			}
		}
		machine->pageTable[pos].valid = FALSE;
		for(int i = 0; i < TLBSize; ++i)
			if(machine->tlb[i].physicalPage == machine->pageTable[pos].physicalPage){
				if(machine->tlb[i].dirty)
					machine->pageTable[pos].dirty = TRUE;
				machine->tlb[i].valid = FALSE;
				break;
			}
		if(machine->pageTable[pos].dirty){
    		fileAddr = machine->pageTable[pos].virtualPage * PageSize;
    		tsize = fileAddr + PageSize < currentThread->fileInfo.size ?
    			PageSize:currentThread->fileInfo.size - fileAddr;
    		openfile->WriteAt(
    			&(machine->mainMemory[machine->pageTable[pos].physicalPage * PageSize]),
    			tsize, fileAddr);
    	}
    	pos = machine->pageTable[pos].physicalPage;
    }
    fileAddr = vpn * PageSize;
    tsize = fileAddr + PageSize < currentThread->fileInfo.size ?
    	PageSize:currentThread->fileInfo.size - fileAddr;
  	//printf("Page fault at vpn %d, phys page %d allocated.\n", vpn, pos);
    openfile->ReadAt(&(machine->mainMemory[pos * PageSize]), tsize, fileAddr);
    machine->pageTable[vpn].virtualPage = vpn;
    machine->pageTable[vpn].physicalPage = pos;
    machine->pageTable[vpn].lrutime = 0;
    machine->pageTable[vpn].valid = TRUE;
    machine->pageTable[vpn].readOnly = FALSE;
    machine->pageTable[vpn].use = FALSE;
    machine->pageTable[vpn].dirty = FALSE;
    delete openfile;
}
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt)) {
		DEBUG('a', "Shutdown, initiated by user program.\n");
   		interrupt->Halt();
    }
    else if((which == SyscallException) && (type == SC_Exit)) {
		DEBUG('a', "Exit.\n");
		OpenFile *openfile = fileSystem->Open(currentThread->getFileName());
    	for (int i = 0; i < machine->pageTableSize; i++) {
    	 	if(machine->pageTable[i].valid){
    	 		machine->pageTable[i].valid = FALSE;
    	 		if(machine->pageTable[i].dirty){
    	 			int tsize, fileAddr;
    	 			fileAddr = machine->pageTable[i].virtualPage * PageSize;
		    		tsize = fileAddr + PageSize < currentThread->fileInfo.size ?
	    				PageSize:currentThread->fileInfo.size - fileAddr;
	    			openfile->WriteAt(
	    				&(machine->mainMemory[machine->pageTable[i].physicalPage * PageSize]),
	    				tsize, fileAddr);
    	 		}
    	 		machine->bitmap->Clear(machine->pageTable[i].physicalPage);
    	 		printf("phys page %d deallocated.\n", machine->pageTable[i].physicalPage);
    	 	}
    	}
    	delete openfile;
	    // Advance program counters.
	    machine->registers[PrevPCReg] = machine->registers[PCReg];	// for debugging, in case we
															// are jumping into lala-land
	    machine->registers[PCReg] = machine->registers[NextPCReg];
	    machine->registers[NextPCReg] = machine->registers[NextPCReg] + 4;
	    printf("Program(%s thread) exit.\n", currentThread->getName());
	    printf("tlb access: %d, tlb miss: %d, miss rate: %f%%\n", 
	    	machine->tlbinfo.time, machine->tlbinfo.miss, machine->tlbinfo.miss/(double)machine->tlbinfo.time*100);
	    currentThread->Finish();
	}
    else if (which == PageFaultException) {
    	if (machine->tlb == NULL) {
    		int vpn = (unsigned) machine->registers[BadVAddrReg] / PageSize;
    		PageTableFetch(vpn);
    	}
    	else {
    		for(int i = 0; i < machine->pageTableSize; ++i)
            	machine->pageTable[i].lrutime++;
		    unsigned int vpn = (unsigned) machine->registers[BadVAddrReg] / PageSize;
		    if(!machine->pageTable[vpn].valid)
    			PageTableFetch(vpn);
	    	int pos = TLBVictim();
	    	if(machine->tlb[pos].valid && machine->tlb[pos].dirty){
	    		for(int i = 0; i < machine->pageTableSize; ++i)
	    			if(machine->pageTable[i].valid && machine->pageTable[i].physicalPage == machine->tlb[pos].physicalPage){
	    				//printf("physic: %d\n", machine->pageTable[i].physicalPage);
	    				//printf("pt: %d, tlb: %d\n", machine->pageTable[i].virtualPage, machine->tlb[pos].virtualPage);
	    				ASSERT(machine->pageTable[i].virtualPage == machine->tlb[pos].virtualPage);
	    				machine->pageTable[i] = machine->tlb[pos];
	    			}
	    	}
	    	machine->tlb[pos] = machine->pageTable[vpn];
	    	machine->tlb[pos].valid = TRUE;
	    	machine->tlb[pos].use = FALSE;
	    	machine->tlb[pos].dirty = FALSE;
	    	machine->tlb[pos].readOnly = FALSE;
	    	machine->tlb[pos].lrutime = 0;
		}
	}
	else {
		printf("Unexpected user mode exception %d %d\n", which, type);
		ASSERT(FALSE);
    }
}