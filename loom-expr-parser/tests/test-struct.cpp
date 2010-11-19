#include <iostream>
using namespace std;

struct T {
	int x;
	int y;
};

struct S {
	int a;
	int b;
	T c;
};

void foo() {
	struct AnotherS {
		int a;
		int b;
	};
	AnotherS ins;
	ins.a = 1;
	ins.b = 2;
}

int main() {
	S ins;
	S* p = &ins;
	ins.a = 1;
	ins.b = 2;
	ins.c.x = 1;
	ins.c.y = 2;
	p->a = 2;
	foo();
	return 0;
}

