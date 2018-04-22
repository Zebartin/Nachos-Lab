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
	for(int i = 0; i < TLBSize; ++i)
		if(machine->tlb[i].lrutime > max){
			max = machine->tlb[i].lrutime;
			pos = i;
		}
	return pos;
#endif
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
    	for (int i = 0; i < machine->pageTableSize; i++) {
    	 	if(machine->pageTable[i].valid){
    	 		machine->pageTable[i].valid = FALSE;
    	 		machine->bitmap->Clear(machine->pageTable[i].physicalPage);
    	 		printf("phys page %d deallocated.\n", machine->pageTable[i].physicalPage);
    	 	}
    	}
	    // Advance program counters.
	    machine->registers[PrevPCReg] = machine->registers[PCReg];	// for debugging, in case we
															// are jumping into lala-land
	    machine->registers[PCReg] = machine->registers[NextPCReg];
	    machine->registers[NextPCReg] = machine->registers[NextPCReg] + 4;
	    printf("Program(%s thread) exit.\n", currentThread->getName());
	    currentThread->Finish();
	}
    else if (which == PageFaultException) {
    	if (machine->tlb == NULL) {
    		OpenFile *openfile = fileSystem->Open(currentThread->getFileName());
    		ASSERT(openfile != NULL);
		    unsigned int vpn = (unsigned) machine->registers[BadVAddrReg] / PageSize;
		    int fileAddr, pos = machine->bitmap->Find();
    		printf("Page fault at vpn %d.\n", vpn);
		    // Find an entry to replace
		    if(pos == -1){
		    	pos = 0;
		    	for(int i = 0; i < machine->pageTableSize; ++i)
		    		if(machine->pageTable[i].physicalPage == pos)
		    			if(machine->pageTable[i].dirty){
		    				openfile->WriteAt(&(machine->mainMemory[pos * PageSize]), PageSize, machine->pageTable[i].virtualPage * PageSize);
		    				break;
		    			}
		    }
		    if(currentThread->fileInfo.codeSize > 0 && 
		    	vpn >= currentThread->fileInfo.codeBegin &&
		    	vpn <= currentThread->fileInfo.codeBegin + currentThread->fileInfo.codeSize/PageSize)
		    	fileAddr = currentThread->fileInfo.codeFAddr;
		    else if(currentThread->fileInfo.initDataSize > 0 && 
		    	vpn >= currentThread->fileInfo.initDataBegin &&
		    	vpn <= currentThread->fileInfo.initDataBegin + currentThread->fileInfo.initDataSize/PageSize)
		    	fileAddr = currentThread->fileInfo.initDataFAddr;
		    else if(currentThread->fileInfo.uninitDataSize > 0 && 
		    	vpn >= currentThread->fileInfo.uninitDataBegin &&
		    	vpn <= currentThread->fileInfo.uninitDataBegin + currentThread->fileInfo.uninitDataSize/PageSize)
		    	fileAddr = currentThread->fileInfo.uninitDataFAddr;
		    else
		    	fileAddr = 0;
		    openfile->ReadAt(&(machine->mainMemory[pos * PageSize]), PageSize, vpn * PageSize + fileAddr);

		    machine->pageTable[vpn].virtualPage = vpn;
		    machine->pageTable[vpn].physicalPage = pos;
		    machine->pageTable[vpn].valid = TRUE;
		    machine->pageTable[vpn].readOnly = FALSE;
		    machine->pageTable[vpn].use = FALSE;
		    machine->pageTable[vpn].dirty = FALSE;
		    delete openfile;
    	}
    	else {
		    unsigned int vpn = (unsigned) machine->registers[BadVAddrReg] / PageSize;
		    if(!machine->pageTable[vpn].valid){
	    		OpenFile *openfile = fileSystem->Open(currentThread->getFileName());
	    		ASSERT(openfile != NULL);
			    int phys = machine->bitmap->Find();
	    		printf("Page fault at vpn %d.\n", vpn);
			    // Find an entry to replace
			    if(phys == -1){
			    	phys = 0;
			    	for(int i = 0; i < machine->pageTableSize; ++i)
			    		if(machine->pageTable[i].physicalPage == 0)
			    			if(machine->pageTable[i].dirty){
			    				openfile->WriteAt(&(machine->mainMemory[phys * PageSize]), PageSize, machine->pageTable[i].virtualPage * PageSize);
			    				break;
			    			}
			    }
			    openfile->ReadAt(&(machine->mainMemory[phys * PageSize]), PageSize, vpn * PageSize);
			    machine->pageTable[vpn].virtualPage = vpn;
			    machine->pageTable[vpn].physicalPage = phys;
			    machine->pageTable[vpn].valid = TRUE;
			    machine->pageTable[vpn].readOnly = FALSE;
			    machine->pageTable[vpn].use = FALSE;
			    machine->pageTable[vpn].dirty = FALSE;
			    delete openfile;
		    }
	    	int pos = TLBVictim();
	    	machine->tlb[pos] = machine->pageTable[vpn];
		}
	}
	else {
		printf("Unexpected user mode exception %d %d\n", which, type);
		ASSERT(FALSE);
    }
}