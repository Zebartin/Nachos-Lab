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
#include "synch.h"
#include "elevatortest.h"

// testnum is set in main.cc
int testnum = 0;
bool tsflag = false;

// for lab3
#define N 10
int count;
List *items;
Semaphore *mutex;
Semaphore *empty;
Semaphore *full;
Lock *lock;
Condition *condc;
Condition *condp;
Barrier *barrier;

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
// ThreadTest0
//  Set up a ping-pong between two threads, by forking a thread
//  to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest0()
{
    DEBUG('t', "Entering ThreadTest0");

    if(tsflag)
        ThreadStatus();
    Thread *t = Thread::GenThread("forked thread");

    t->Fork(SimpleThread, (void*)(t->getTid()));
    SimpleThread(0);
}

//----------------------------------------------------------------------
// ThreadTest1
// Act similiarly to ThreadTest1, but with more threads.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

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
// ThreadTest2
// Test for lab 2 ex 3.
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
ThreadTest2()
{
    DEBUG('t', "Entering ThreadTest2");

    Thread *t = Thread::GenThread("preemptive thread 0", 1);
    t->Fork(PreemptiveThread0, (void*)(t->getTid()));
}

//----------------------------------------------------------------------
// ThreadTest3
// Test for lab 2 cha 1.
//----------------------------------------------------------------------

void
SimpleThread1(int times)
{
    int num;

    for (num = 0; num < times; ++num) {
        printf("*** thread %d looped %d times(priority: %d)\n",
            currentThread->getTid(), num, currentThread->getPriority());
        interrupt->OneTick();
    }
}

void
ThreadTest3()
{
    DEBUG('t', "Entering ThreadTest3");

    Thread *t = Thread::GenThread("rr thread 0");
    t->Fork(SimpleThread1, 20);
    t = Thread::GenThread("rr thread 1");
    t->Fork(SimpleThread1, 15);
    t = Thread::GenThread("rr thread 2");
    t->Fork(SimpleThread1, 35);
}

//----------------------------------------------------------------------
// ThreadTest4
// Producer-Consumer with semaphores
//----------------------------------------------------------------------
void producer(int id){
    int i = 0;
    while(i++ < N){
        empty->P();
        mutex->P();
        items->Append(i + (id-1)*(N>>1));
        printf("producer%d inserts %d\n", id, i + (id-1)*(N>>1));
        mutex->V();
        full->V();
    }
}
void consumer(int id){
    int t, i = 0;
    while(i++ < N){
        full->P();
        mutex->P();
        t = (int)items->Remove();
        printf("consumer%d removes %d\n", id, t);
        mutex->V();
        empty->V();
    }
}
void
ThreadTest4()
{
    DEBUG('t', "Entering ThreadTest4");
    items = new List;
    mutex = new Semaphore("mutex", 1);
    empty = new Semaphore("empty", N);
    full = new Semaphore("full", 0);
    Thread *p1 = Thread::GenThread("producer 1");
    Thread *p2 = Thread::GenThread("producer 2");
    Thread *c1 = Thread::GenThread("consumer 1");
    Thread *c2 = Thread::GenThread("consumer 2");
    p1->Fork(producer, 1);
    p2->Fork(producer, 2);
    c1->Fork(consumer, 1);
    c2->Fork(consumer, 2);
}
//----------------------------------------------------------------------
// ThreadTest5
// Producer-Consumer with condition variable
//----------------------------------------------------------------------
void producerprime(int id){
    int i = 0;
    while(i++ < N){
        lock->Acquire();
        while(count == N){
            printf("producer%d: No place. Wait.\n", id);
            condp->Wait(lock);  //生产者等待
        }
        items->Append(i + (id-1)*N);
        count++;
        printf("producer%d inserts %d\n", id, i + (id-1)*N);
        if(count == 1)
            condc->Signal(lock);    //唤醒消费者
        lock->Release();
    }
}
void consumerprime(int id){
    int t, i = 0;
    while(i++ < N){
        lock->Acquire();
        while(count == 0){
            printf("consumer%d: No production. Wait.\n", id);
            condc->Wait(lock);  //消费者等待
        }
        t = (int)items->Remove();
        count--;
        printf("consumer%d removes %d\n", id, t);
        if(count == N - 1)
            condp->Signal(lock);    //唤醒生产者
        lock->Release();
    }
}
void
ThreadTest5()
{
    DEBUG('t', "Entering ThreadTest5");
    count = 0;
    items = new List;
    lock = new Lock("mutex");
    condc = new Condition("consumer");
    condp = new Condition("producer");
    Thread *p1 = Thread::GenThread("producer 1");
    Thread *p2 = Thread::GenThread("producer 2");
    Thread *c1 = Thread::GenThread("consumer 1");
    Thread *c2 = Thread::GenThread("consumer 2");
    p1->Fork(producerprime, 1);
    p2->Fork(producerprime, 2);
    c1->Fork(consumerprime, 1);
    c2->Fork(consumerprime, 2);
}

//----------------------------------------------------------------------
// ThreadTest6
// Barrier
//----------------------------------------------------------------------

void
SimpleThread2(int id)
{
    for(int i = 0; i < 5; ++i){
        printf("*** thread %d looped %d times\n", id, i);
        if(i == 1)
            barrier->Stall();
    }
}

void
ThreadTest6()
{
    DEBUG('t', "Entering ThreadTest6");
    barrier = new Barrier("barrier", 3);
    Thread *t1 = Thread::GenThread("thread");
    Thread *t2 = Thread::GenThread("thread");
    Thread *t3 = Thread::GenThread("thread");
    t1->Fork(SimpleThread2, 1);
    t2->Fork(SimpleThread2, 2);
    t3->Fork(SimpleThread2, 3);
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
        ThreadTest0();
        break;
    case 1:
        ThreadTest1();
        break;
    case 2:
        ThreadTest2();
        break;
    case 3:
        ThreadTest3();
        break;
    case 4:
        ThreadTest4();
        break;
    case 5:
        ThreadTest5();
        break;
    case 6:
        ThreadTest6();                                                                                                                              
        break;
    default:
        printf("No test specified.\n");                                                                                                                                                                                                             
    break;
    }
}
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        