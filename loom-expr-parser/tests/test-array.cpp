#include <iostream>
#include <cstdio>
using namespace std;

const int n = 10;

struct S1 {
	int len;
	char *digit;
};

int main() {
	S1 **a = new S1*[n];
	for (int i = 0; i < n; i++) {
		printf("%c\n", a[i]->digit[1]);
		printf("%c\n", *(a[i]->digit));
	}
	delete[] a;
	return 0;
}

