#include "syscall.h"

int
main()
{
	int fd1, fd2, len;
	char buf[20];
	Create("ftest1.txt");
	fd1 = Open("ftest1.txt");
	Write("test string", 11, fd1);
	Close(fd1);

	Create("ftest2.txt");
	fd1 = Open("ftest1.txt");
	fd2 = Open("ftest2.txt");
	len = Read(buf, 6, fd1);
	Write(buf, len, fd2);
	Close(fd1);
	Close(fd2);

    //Halt();
    /* not reached */
}
