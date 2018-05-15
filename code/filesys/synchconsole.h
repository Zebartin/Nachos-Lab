// synchconsole.h 
//	Data structures to export a synchronous interface to a terminal
//	I/O device. 
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef SYNCHCONSOLE_H
#define SYNCHCONSOLE_H

#include "console.h"
#include "synch.h"

// The following class defines a "synchronous" console abstraction.

class SynchConsole {
  public:
    SynchConsole(char *readFile, char *writeFile);
				// initialize the hardware synch console device
    ~SynchConsole();			// clean up synch console emulation

// external interface -- Nachos kernel code can call these
    void PutChar(char ch);	// Write "ch" to the synch console display, 
				// and return immediately.  "writeHandler" 
				// is called when the I/O completes. 

    char GetChar();	   	// Poll the synch console input.  If a char is 
				// available, return it.  Otherwise, return EOF.
    				// "readHandler" is called whenever there is 
				// a char to be gotten
    void ReadAvail();
    void WriteDone();
  private:
    Console *console;             // Raw console device
    Semaphore *readAvail, *writeDone;       // To synchronize requesting thread 
                    // with the interrupt handler
    Lock *lock;             // Only one read/write request
                    // can be sent to the console at a time
};

#endif // SYNCHCONSOLE_H
