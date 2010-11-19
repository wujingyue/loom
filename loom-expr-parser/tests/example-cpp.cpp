#include <stdio.h>

class base
{
	public:
		int abc;
		int func(int a)
		{
			int b = 0;
			c = a + b;
			return 0;
		}

		int funca();

	private:
		int c;
};

int base::funca()
{
	int a;
	a = 0;
	printf("hello: %d\n", a);
	return 0;
}

class child : public base
{
	public:
		static long ab;
		int func(int a)
		{
			int abc = 10;
			{
				int cba = a + ab;
				cba += abc; abc-=cba; ab = cba + a;
			}
			printf("%d\n", abc);
			return 0;
		}

		int funcb()
		{
			printf("funcb %x\n", (unsigned int)ab);
			return 0;
		}
};


int func(int a);

static int a;
int g;

int main(int argc, char *argv[])
{

	int i = a;
	for (i = 0; i < 10; ++i) {
		printf("%d\n", i);
	}

	if (i > 10) {
		printf("i > 10\n");
	}
	func(23);
	return 0;
}

int func(int a)
{
	int b;
	int c = 0;

	b = c + a;

	c = b + a;

	child cc;
	cc.func(10);
	return 0;
}

long child::ab = 10;
