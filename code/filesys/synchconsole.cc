// console.cc 
//	Routines to simulate a serial port to a console device.
//	A console has input (a keyboard) and output (a display).
//	These are each simulated by operations on UNIX files.
//	The simulated device is asynchronous,
//	so we have to invoke the interrupt handler (after a simulated
//	delay), to signal that a byte has arrived and/or that a written
//	byte has departed.
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synchconsole.h"

static void ConsoleReadAvail(int c) 
{ SynchConsole *console = (SynchConsole *)c; console->ReadAvail(); }
static void ConsoleWriteDone(int c)
{ SynchConsole *console = (SynchConsole *)c; console->WriteDone(); }

//----------------------------------------------------------------------
// SynchConsole::SynchConsole
//----------------------------------------------------------------------

SynchConsole::SynchConsole(char *readFile, char *writeFile)
{
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    lock = new Lock("synch console lock");
    console = new Console(readFile, writeFile, ConsoleReadAvail, ConsoleWriteDone, (int) this);
}

//----------------------------------------------------------------------
// SynchConsole::~SynchConsole
// 	Clean up Synchconsole emulation
//----------------------------------------------------------------------

SynchConsole::~SynchConsole()
{
    delete console;
    delete readAvail;
    delete writeDone;
    delete lock;
}

//----------------------------------------------------------------------
// SynchConsole::ReadAvail()
//----------------------------------------------------------------------

void
SynchConsole::ReadAvail()
{
    readAvail->V();
}

//----------------------------------------------------------------------
// SynchConsole::WriteDone()
//----------------------------------------------------------------------

void
SynchConsole::WriteDone()
{
    writeDone->V();
}

//----------------------------------------------------------------------
// SynchConsole::GetChar()
//----------------------------------------------------------------------

char
SynchConsole::GetChar()
{
   lock->Acquire();
   readAvail->P();
   char ch = console->GetChar();
   lock->Release();
   return ch;
}

//----------------------------------------------------------------------
// SynchConsole::PutChar()
//----------------------------------------------------------------------

void
SynchConsole::PutChar(char ch)
{
   lock->Acquire();
   console->PutChar(ch);
   writeDone->P();
   lock->Release();
}
