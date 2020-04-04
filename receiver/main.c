#include <stdio.h>

#ifdef DEBUG_
	char* debrel = "Debug";
#else
	char* debrel = "Release";
#endif

int main(void)
{
	printf("Receive compiled for %s\n", debrel);
}