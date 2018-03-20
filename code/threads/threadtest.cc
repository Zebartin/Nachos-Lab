// threadtest.cc
//  Simple test case for the threads assignment.
//
//  Create two threads, and have them context switch
//  back and forth between themselves by calling Thread::Yield,
//  to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "elevatortest.h"

// testnum is set in main.cc
int testnum = 0;
bool tsflag = false;

extern void ThreadStatus();

//----------------------------------------------------------------------
// SimpleThread
//  Loop 5 times, yielding the CPU to another ready thread
//  each iteration.
//
//  "which" is simply a number identifying the thread, for debugging
//  purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;

    for (num = 0; num < 2; num++) {
        printf("*** thread %d looped %d times(priority: %d)\n",
            which, num, currentThread->getPriority());
        currentThread->Yield();
    }
}

//----------------------------------------------------------------------
// ThreadTest1
//  Set up a ping-pong between two threads, by forking a thread
//  to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    if(tsflag)
        ThreadStatus();
    Thread *t = Thread::GenThread("forked thread");

    t->Fork(SimpleThread, (void*)(t->getTid()));
    SimpleThread(0);
}

//----------------------------------------------------------------------
// ThreadTestLab1
// Act similiarly to ThreadTest1, but with more threads.
//----------------------------------------------------------------------

void
ThreadTestLab1()
{
    DEBUG('t', "Entering ThreadTestLab1");

    Thread *t;
    for(int i = 0; i < 10; ++i){
        t = Thread::GenThread("forked thread");
        t->Fork(SimpleThread, (void*)(t->getTid()));
    }
    SimpleThread(currentThread->getTid());

    for(int i = 0; i < MaxThread; ++i){
        t = Thread::GenThread("forked thread");
        t->Fork(SimpleThread, (void*)(t->getTid()));
    }
    //Never reached
    SimpleThread(currentThread->getTid());
}

//----------------------------------------------------------------------
// ThreadTestLab2
// Act similiarly to ThreadTest1, but with more threads.
//----------------------------------------------------------------------

void
PreemptiveThread2(int tid) {
    if(tsflag)
        ThreadStatus();
    for(int i = 0; i < 5; ++i)
        printf("*** thread %d looped %d times(priority: %d)\n",
            tid, i, currentThread->getPriority());
}

void
PreemptiveThread1(int tid) {
    if(tsflag)
        ThreadStatus();
    for(int i = 0; i < 5; ++i){
        printf("*** thread %d looped %d times(priority: %d)\n",
            tid, i, currentThread->getPriority());
        if(i == 1){
            Thread *t = Thread::GenThread("preemptive thread 2", 5);
            t->Fork(PreemptiveThread2, (void*)(t->getTid()));
        }
    }
}

void
PreemptiveThread0(int tid) {
    if(tsflag)
        ThreadStatus();
    for(int i = 0; i < 5; ++i){
        printf("*** thread %d looped %d times(priority: %d)\n",
            tid, i, currentThread->getPriority());
        if(i == 1){
            Thread *t = Thread::GenThread("preemptive thread 1", 7);
            t->Fork(PreemptiveThread1, (void*)(t->getTid()));
        }
    }
}

void
ThreadTestLab2()
{
    DEBUG('t', "Entering ThreadTestLab2");

    Thread *t = Thread::GenThread("preemptive thread 0", 1);
    t->Fork(PreemptiveThread0, (void*)(t->getTid()));
}

//----------------------------------------------------------------------
// ThreadTest
//  Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 0:
        ThreadTest1();
        break;
    case 1:
        ThreadTestLab1();
        break;
    case 2:
        ThreadTestLab2();
        break;
    default:
        printf("No test specified.\n");
    break;
    }
}
