#include <stdio.h>

int fun(int a);

int a;
int g;

int main(int argc, char *argv[])
{

    int i;
    for (i = 0; i < 10; ++i) {
        printf("%d\n", i);
    }

    if (i > 10) {
        printf("i > 10\n");
    }

    return 0;
}

int func(int a)
{
    int b;
    int c = 0;
    
    b = c + a;
    
    c = b + a;

    for (b=0; b<10; ++b) {
	    int d;
	    d = b;
	    d++;
    }
	return 0;
}

