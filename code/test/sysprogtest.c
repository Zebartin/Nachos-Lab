#include "syscall.h"

#define N 4

int i;

void func()
{
	char val;
	val = 'b';
	for(i = 0; i < N; i++){
		Write(&val, 1, 1);
		Yield();
	}
}

int
main()
{
	char val1,val2;
	val1 = 'a';
	val2 = 'b';
	Write(&val1, 1, 1);
	Write(&val2, 1, 1);
	Write(&val1, 1, 1);
	Write(&val2, 1, 1);
	Write(&val1, 1, 1);
	Write(&val2, 1, 1);
	Write(&val1, 1, 1);
	Write(&val2, 1, 1);
	//Halt();
}
