#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <vector>
#include <set>
#include <map>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <cassert>
#include <queue>
#include <cctype>
using namespace std;

const int oo = 0x7fffffff;

vector<int> a;
set<int> sols;
int I;

bool input() {
	a.clear();
	for (int i = 0; i < 4; i++) {
		int k;
		cin >> k;
		a.push_back(k);
	}
	if (a[0] == 0)
		return false;
	return true;
}

void search(vector<int> a) {
	if (a.size() == 1) {
		sols.insert(a[0]);
		return;
	}
	for (size_t i = 0; i < a.size(); ++i) {
		for (size_t j = i + 1; j < a.size(); ++j) {
			vector<int> b = a;
			b.erase(b.begin() + j);
			b.erase(b.begin() + i);
			int x = a[i], y = a[j];

			b.push_back(x + y);
			search(b);
			b.pop_back();
			b.push_back(x - y);
			search(b);
			b.pop_back();
			b.push_back(y - x);
			search(b);
			b.pop_back();
			b.push_back(x * y);
			search(b);
			b.pop_back();
			if (y != 0 && x % y == 0) {
				b.push_back(x / y);
				search(b);
				b.pop_back();
			}
			if (x != 0 && y % x == 0) {
				b.push_back(y / x);
				search(b);
				b.pop_back();
			}
		}
	}
}

void solve() {
	sols.clear();
	search(a);
}

void output() {
	int ans = -oo, ans_i = -1, ans_j = -1;
	int first = -oo, last = -oo;
	for (set<int>::iterator it = sols.begin(); it != sols.end(); ++it) {
		if (*it == last + 1)
			last++;
		else {
			if (last - first >= ans) {
				ans = last - first;
				ans_i = first;
				ans_j = last;
			}
			first = *it; last = *it;
		}
	}
	if (last - first >= ans) {
		ans = last - first;
		ans_i = first; ans_j = last;
	}
	printf("Case %d: %d to %d\n", I + 1, ans_i, ans_j);
}

int main() {
	I = 0;
	while (input()) {
		solve();
		output();
		I++;
	}
	return 0;
}

