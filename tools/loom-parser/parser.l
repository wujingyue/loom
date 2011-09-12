
%{
#include <string>

class Decs;
class Event;
class EventDesc;
class EventDescContainer;
class FixEvent;
#include "EventNode.h"

#include "parser.y.h"

#undef LEX_DEBUG

#ifdef LEX_DEBUG
#include <stdio.h>
#define lex_print(a...) do { fprintf(stderr, a); } while(0)
#else
#define lex_print(a...) do {} while(0)
#endif


%}

expr [0-9a-zA-Z_\'\"\?\:\[\]\(\){}.+&*~!/%<>=^|;-]
space [ \t]

%%

"<>" { 
    lex_print("CRITICAL "); 
    return CRITICAL; 
    }

">" { 
    lex_print("ORDER"); 
    return ORDER; 
    }

"|" { 
    lex_print("BARRIER ");  
    return BARRIER; 
    }

"&" { 
    lex_print("AMPERSAND ");  
    return AMPERSAND; 
    }

"{" { 
    lex_print("LEFT_BRACE "); 
    return LEFT_BRACE; 
    }

"}" { 
    lex_print("RIGHT_BRACE "); 
    return RIGHT_BRACE; 
    }

"." { 
    lex_print("PERIOD "); 
    return PERIOD; 
    }

":" { 
    lex_print("COLON "); 
    return COLON; 
    }

"=" { 
    lex_print("ASSIGNMENT "); 
    return ASSIGNMENT; 
    }

"," { 
    lex_print("COMMA ");
    return COMMA; 
    }

"@" { 
    lex_print("LOCATION_ON "); 
    return LOCATION_AT;
    }


on {  
    lex_print("ON "); 
    return TRIGGER_ON; 
    }

decl { 
    lex_print("DECL "); 
    return DECLARATION; 
    }

lock { 
    lex_print("LOCK ");  
    return LOCK; 
    }

unlock { 
    lex_print("UNLOCK ");
    return UNLOCK; 
    }

trylock { 
    lex_print("TRYLOCK "); 
    return TRYLOCK; 
    }


\n  { 
    lex_print("\n"); 
    }

{space}+ { 
    lex_print(" "); 
    } 


[0-9]+ { 
    lex_print("NUMBER ");  
    yylval.number = atoi(yytext); 
    return NUMBER; 
    }


[a-zA-Z0-9_.]+ { 
    lex_print("NAME "); 
    yylval.st = new std::string(yytext); 
    return NAME; 
    }


\({expr}{expr}*\)/{space}*[@}\n] { 
    lex_print("EXPRESSION ");
    yylval.st = new std::string(yytext); 
    return C_EXPR; 
    }

\* {
    return WILD_ANY;
    }

\/\/.*\n 


%%

