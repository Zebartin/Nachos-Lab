#include "copyright.h"
#include "system.h"
#include "thread.h"
#include <stdio.h>

void ThreadStatus(){
	Thread *tmp = NULL;
	printf("------------------Thread Status------------------\n");
	printf("Uid     Tid     Name                Status\n");
	for(int i = 0; i < MaxThread; ++i)
		if(threadPtr[i] != NULL){
			tmp = threadPtr[i];
			printf("%-8d%-8d%-20s%-10s\n", tmp->getUid(), tmp->getTid(), tmp->getName(), tmp->getStatus());
		}
	printf("-------------------------------------------------\n");
}