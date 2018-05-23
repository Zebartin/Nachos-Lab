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

extern void StartProcess(char *file);
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

void getStr(int addr, char *str, int &len)
{
	int tmp = 0;
	len = 0;
	while(TRUE){
		while(!machine->ReadMem(addr + len, 1, &tmp));
		if((str[len++] = char(tmp)) == 0)
			break;
	}
	len--;
}
typedef struct
{
	char *fileName;
	AddrSpace *space;
	int pc;
} ForkInfo;
void
ForkProcess(int arg)
{
	ForkInfo *forkinfo = (ForkInfo *)arg;
	AddrSpace *forkspace = new AddrSpace(*(forkinfo->space));   
	int curpc = forkinfo->pc;
    currentThread->space = forkspace;
    currentThread->setFileName(forkinfo->fileName, FALSE);

    forkspace->InitRegisters();		// set the initial register values
    forkspace->RestoreState();		// load page table register

    machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg)); //for debugging
    machine->WriteRegister(PCReg, curpc);
    machine->WriteRegister(NextPCReg, curpc + 4);

    machine->Run();
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

	    printf("Program(%s thread) exit with status %d.\n", currentThread->getName(), machine->ReadRegister(4));
	    printf("tlb access: %d, tlb miss: %d, miss rate: %f%%\n", 
	    	machine->tlbinfo.time, machine->tlbinfo.miss, machine->tlbinfo.miss/(double)machine->tlbinfo.time*100);

		machine->DeallocatePage();
		//fileSystem->Remove(currentThread->getFileName());
	    machine->AdvancePC(machine->ReadRegister(NextPCReg) + 4);	    
	    currentThread->Finish();
	}
	else if((which == SyscallException) && (type == SC_Create)) {
		DEBUG('a', "Create a file.\n");

		char fileName[256];
		int len;
		getStr(machine->ReadRegister(4), fileName, len);
		printf("Create file %s\n", fileName);
		fileSystem->Create(fileName, 0);
		machine->AdvancePC(machine->ReadRegister(NextPCReg) + 4);
	}
	else if((which == SyscallException) && (type == SC_Open)) {
		DEBUG('a', "Open a file.\n");
		
		char fileName[256];
		int len;
		getStr(machine->ReadRegister(4), fileName, len);
		printf("Open file %s\n", fileName);

		OpenFile *openfile = fileSystem->Open(fileName);
		machine->WriteRegister(2, (int)openfile);
		machine->AdvancePC(machine->ReadRegister(NextPCReg) + 4);
	}
	else if((which == SyscallException) && (type == SC_Close)) {
		DEBUG('a', "Close a file.\n");

		int fileid = machine->ReadRegister(4);
		OpenFile *openfile = (OpenFile *)fileid;
		delete openfile;
		machine->AdvancePC(machine->ReadRegister(NextPCReg) + 4);
	}
	else if((which == SyscallException) && (type == SC_Read)) {
		DEBUG('a', "Read a file.\n");

		int addr = machine->ReadRegister(4),
			size = machine->ReadRegister(5),
			fileid = machine->ReadRegister(6);
		int len;
		char content[size + 1];
		if (fileid == ConsoleInput){
			for (int i = 0; i < size; i++)
				scanf("%c", &content[i]);
			len = size;
		}
		else{
			OpenFile *openfile = (OpenFile *)fileid;
			len = openfile->Read(content, size);
		}
			content[len] = 0;
		// printf("reading...\n");
		// printf("str: %s\nlen: %d\n", content, size);
		for (int i = 0; i < len; i++)
			while(!(machine->WriteMem(addr + i, 1, (int)content[i])));
		machine->WriteRegister(2, len);
		machine->AdvancePC(machine->ReadRegister(NextPCReg) + 4);
	}
	else if((which == SyscallException) && (type == SC_Write)) {
		DEBUG('a', "Write a file.\n");

		int addr = machine->ReadRegister(4),
			size = machine->ReadRegister(5),
			fileid = machine->ReadRegister(6);
		int tmp;
		char content[size];
		for (int i = 0; i < size; i++){
			while(!(machine->ReadMem(addr + i, 1, &tmp)));
			content[i] = char(tmp);
		}
		content[size] = 0;
		if (fileid == ConsoleOutput)
			printf("%s", content);
		else{
			printf("writing...\n");
			printf("str: %s\nlen: %d\n", content, size);
			OpenFile *openfile = (OpenFile *)fileid;
			openfile->Write(content, size);
		}
		machine->AdvancePC(machine->ReadRegister(NextPCReg) + 4);		
	}
	else if((which == SyscallException) && (type == SC_Exec)) {
		DEBUG('a', "Exec a prog.\n");
		char fileName[256];
		int len;
		getStr(machine->ReadRegister(4), fileName, len);

		printf("Exec: %s\n", fileName);
		Thread *userThread = Thread::GenThread(fileName);
		userThread->Fork(StartProcess, fileName);

		machine->WriteRegister(2, userThread->getTid());
		machine->AdvancePC(machine->ReadRegister(NextPCReg) + 4);
	}
	else if((which == SyscallException) && (type == SC_Fork)) {
		DEBUG('a', "Fork a func.\n");
		printf("Fork thread with func...\n");
		int funcAddr = machine->ReadRegister(4);
		ForkInfo *forkinfo = new ForkInfo;
		forkinfo->fileName = currentThread->getFileName();
		forkinfo->space = currentThread->space;
		forkinfo->pc = funcAddr;

		Thread *userThread = Thread::GenThread("fork");
		userThread->Fork(ForkProcess, (int)(forkinfo));
		machine->AdvancePC(machine->ReadRegister(NextPCReg) + 4);
	}
	else if((which == SyscallException) && (type == SC_Yield)) {
		DEBUG('a', "Yield current thread.\n");
		//printf("%s thread yielding...\n", currentThread->getName());
		machine->AdvancePC(machine->ReadRegister(NextPCReg) + 4);
		currentThread->Yield();
	}
	else if((which == SyscallException) && (type == SC_Join)) {
		DEBUG('a', "Join a thread.\n");
		int tid = machine->ReadRegister(4);
		while(threadPtr[tid] != NULL)
			currentThread->Yield();
		machine->AdvancePC(machine->ReadRegister(NextPCReg) + 4);
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