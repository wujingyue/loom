/**
 * file: parser.cpp
 * Author: Maoliang <kceiwH@gmail.com>
 * Created Time: Tue Oct 27 23:25:31 2009
 * Description: 
 */

#include <iostream>
#include <cstdlib>

using namespace std;


extern int convert(const char *source, const char *dest);
extern FILE *yyin;
extern int yylex(void);

int main(int argc, char *argv[])
{
    if (argc!=3) {
        cout << argv[0] << " source dst\n";
        exit(0);
    }

    convert(argv[1], argv[2]);

    return 0;
}

