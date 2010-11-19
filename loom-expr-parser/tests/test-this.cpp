#include <iostream>
using namespace std;

struct St {

	void foo(int arg) {
		printf("%d\n", arg);
	}
};

int main() {
	St ins;
	ins.foo(123);
	return 0;
}

