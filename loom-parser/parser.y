
%{
#include <string>
#include <vector>
#include <deque>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <algorithm>
#include <functional>

using std::bind2nd;

#include "Decl.h"
#include "Event.h"
#include "EventDesc.h"
#include "EventNode.h"
#include "FixEvent.h"

extern int yylex(void);
void yyerror(const char *);

std::vector< Decl > declarations;
std::vector< FixEvent > changes;
Decl current_declaration;


class class_name_equal : public std::binary_function< Decl, char *, bool >
{
public:
    class_name_equal(const std::string &name);
    bool operator()(const Decl &decl, const char *name);
    bool operator() (const Decl &decl);

private:
    std::string name;
};


std::string filename;
std::ofstream outfile;

/*
#define PRODUCTION_DEBUG
*/

#ifdef PRODUCTION_DEBUG
#define production_debug(args...) do { fprintf(stderr, args); } while(0)
#else
#define production_debug(args...) do {} while(0)
#endif

/*
#define YACC_DEBUG
*/

#ifdef YACC_DEBUG
#define yacc_debug(args...) do { fprintf(stderr, args); } while(0)
#else
#define yacc_debug(args...) do {} while(0)
#endif

void print_events(FixEvent *event);

%}

%union
{
    int number;
    std::string *st;
    Event *event;
    EventDesc *eventdesc;
    EventDescContainer *eventdesccontainer;
    EventNode *event_group;
    enum EventOperation event_ops;
    FixEvent *fix;
}

%token <st> NAME
%token <st> C_EXPR
%token <number> NUMBER

%token <st> LOCATION_AT       /* @ */

%token <st> ORDER
%token <st> BARRIER
%token <st> CRITICAL

%token DECLARATION
%token TRIGGER_ON
%token LEFT_BRACE
%token RIGHT_BRACE
%token PERIOD
%token COMMA
%token AMPERSAND
%token COLON
%token WILD_ANY

%token ASSIGNMENT
%token LOCK
%token UNLOCK
%token TRYLOCK


%type <st> function
%type <st> class
%type <st> bool_expression
%type <number> line
%type <st> file
%type <st> object_expression
%type <event> triggering_event
%type <st> method
%type <st> class_method
%type <st> op
%type <st> context
%type <st> location
%type <st> object
%type <eventdesc> eventdesc
%type <eventdesccontainer> eventdescs
%type <eventdesccontainer> eventdesc_list
%type <event> event
%type <event_ops> order
%type <event_group> event_group
%type <fix> change

%start extension;

%%

extension:
    statements
    ;
	

statements:
    |
    statements statement
    ;

statement:
    change
    {
        changes.push_back(*$1);
        // print_events($1);
        production_debug("\n\n");
        delete $1;
    }
    |
    class_decl
    {
        production_debug("\n\n");
    }
    ;

change:
    triggering_event event_group
    {
        production_debug("change->triggering_event event_group\n");
        if ($1==NULL || $2==NULL) {
            // error
        }

        FixEvent *fix = new FixEvent();
        fix->set_triggering_event(*$1);
        fix->set_events(*$2);
        delete $1; delete $2;
        $$ = fix;
    }
    |
    event_group
    {
        production_debug("change->event_group\n");
        if ($1==NULL) {
            // error
        }

        FixEvent *fix = new FixEvent();
        fix->set_events(*$1);
        delete $1;
        $$ = fix;
    }
    ;


event_group:
    event CRITICAL WILD_ANY
    {
        production_debug("event_group->event_group critical *\n");
        if ($1==NULL) {
            $$ = NULL;
        } else {
            EventNode *node = new EventNode(event_op_atomic);
            EventNode *e = new EventNode(event_op_noop);
            e->set_event(*$1);
            node->add_first(*e);
            delete $1; delete e;
            $$ = node;
        }
    }
    |
    event order event
    {
        production_debug("event_group->event order event\n");
        if ($1==NULL || $3==NULL) {
            $$ = NULL;
        } else {
            EventNode *node = new EventNode($2);
            EventNode *first = new EventNode(event_op_noop);
            EventNode *last = new EventNode(event_op_noop);
            first->set_event(*$1);
            last->set_event(*$3);
            node->add_first(*first);
            node->add_last(*last);
            delete first;
            delete last;
            $$ = node;
        }

        delete $1;
        delete $3;
    }
    ;

order: 
    ORDER
    {
        $$ = event_op_order;
    }
    | 
    CRITICAL
    {
        $$ = event_op_critical;
    }
    | 
    BARRIER
    {
        $$ = event_op_barrier;
    }
    ;

event:
    LEFT_BRACE eventdesc_list RIGHT_BRACE
    {
        production_debug("event->{ eventdesc_list }\n");
        if ($2==NULL) {
            $$ = NULL;
        } else {
            Event *e = new Event();
            e->add_description(*$2);
            delete $2;
            $$ = e;
        }
    }
    ;

eventdesc_list:
    eventdesc eventdescs
    {
        production_debug("eventdesc_list->eventdesc eventdescs\n");
        if ($1==NULL) {
            $$ = NULL;
        } else {
            EventDescContainer *edc = new EventDescContainer();
            if ($1 != NULL) {
                edc->add_desc(*$1);
                delete $1;
            }

            if ($2 != NULL) {
                edc->add_desc(*$2);
                delete $2;
            }

            $$ = edc;
        }
    }
    ;

eventdescs:
    {
        production_debug("eventdescs->NULL\n");
        $$ = NULL;
    }
    |
    eventdescs COMMA eventdesc
    {
        production_debug("eventdescs->eventdescs, eventdesc\n");
        if ($3==NULL) {
            $$ = NULL;
        } else {
            EventDescContainer *edc = new EventDescContainer();
            if ($1 != NULL) {
                edc->add_desc(*$1);
                delete $1;
            }
            if ($3 != NULL) {
                edc->add_desc(*$3);
                delete $3;
            }
            $$ = edc;
        }
    }
    ;

eventdesc:
    op object location context
    {
        if ($3==NULL) {
            $$ = NULL;
        } else {
            EventDesc *desc = new EventDesc();
            if ($1 != NULL) {
                desc->set_op(*$1);
                delete $1;
            }
            if ($2 != NULL) {
                desc->set_object(*$2);
                delete $2;
            }

            desc->set_location(*$3);
            delete $3;

            if ($4!=NULL) {
                desc->set_context(*$4);
                delete $4;
            }

            $$ = desc;
        }
    }
    ;

object: 
    {
        $$ = NULL;
    }
    |
    object_expression 
    {
        $$ = $1;
    }
    ;

location:
    LOCATION_AT file COLON line
    {
        production_debug("location->@file:line\n");
        std::string *loc = new std::string(*$2);
        std::ostringstream oss;
        oss << ":" << $4;
        loc->append(oss.str());
        $$ = loc;     
        delete $2;
    }	
    ;

context:
    {
        $$ = NULL;
    }
    |
    AMPERSAND bool_expression
    {
        $$ = $2;
    }
    ;

op:
    {
        $$ = NULL;
    }
    |
    class_method
    {
        $$ = $1;
    }
    |
    method
    {
        $$ = $1;
    }
    ;

class_method:
    NAME { yacc_debug("class_name "); $$ = $1; }
    ;

method:
    LOCK { $$ = new std::string("lock"); yacc_debug("LOCK ");}
    |
    UNLOCK { $$ = new std::string("unlock"); yacc_debug("UNLOCK "); }
    |
    TRYLOCK { $$ = new std::string("trylock"); yacc_debug("TRYLOCK "); }
    ;

triggering_event:
    trigger_on eventdesc 
    {
        production_debug("triggering_event->on eventdesc\n");
        Event *trigger = new Event(event_type_item);
        if ($2!=NULL) {
            trigger->add_description(*$2);
            delete $2;
            $$ = trigger;
        } else {
            $$ = NULL;
        }
    }
    ;

class_decl:
    class_declaration class
    { 
        if (find_if(declarations.begin(), declarations.end(),
            class_name_equal(*$2))!=declarations.end()) {
                yyerror("duplicative declaration of a class");
        }
        current_declaration.set_name(*$2);
        delete $2;
        current_declaration.clear_methods(); 
    }
    LEFT_BRACE member_list RIGHT_BRACE 
    {
        declarations.push_back(current_declaration); 
    }
    ;

member_list:
    member_decl comma_member_decl
    ;

comma_member_decl:
    |
    comma_member_decl COMMA member_decl
    ;

member_decl:
    method ASSIGNMENT function {
        current_declaration.add_method(*$1, *$3); 
        delete $1;
        delete $3;
    }
    ;

object_expression:
    C_EXPR
    {
        $$ = $1;
    }
    ;

file:
    NAME
    {
        $$ = $1;
    }
    ;

line:
    NUMBER
    {
        $$ = $1;
    }
    ;

bool_expression:
    C_EXPR
    {
        $$ = $1;
    }
    ;

class:
    NAME
    {
        $$ = $1; 
    }
    ;

class_declaration:
    DECLARATION
    ;

trigger_on:
    TRIGGER_ON
    ;

function:
    NAME 
    { 
        $$ = $1; 
    }
    ;
%%


void yyerror(const char *s)
{
    std::cerr << s << std::endl;
}

static void write_configuration(std::ofstream &, const std::vector< FixEvent > &,
            const std::vector< Decl > &);

static void write_order(std::ofstream &out,
                        EventDescContainer::DescContainerSizeType amount,
                        const Event &first, 
                        const Event &last);
static void write_barrier(std::ofstream &out, 
                            EventDescContainer::DescContainerSizeType amount,
                            const Event &first, 
                            const Event &last);
static void write_atomic(std::ofstream &out,
                            EventDescContainer::DescContainerSizeType amount,
                            const Event &first);
static void write_critical_triggering(std::ofstream &out,
                            EventDescContainer::DescContainerSizeType amount,
                            const Event &triggering,
                            const Event &first,
                            const Event &second);
static void write_critical_notriggering(std::ofstream &out,
                            EventDescContainer::DescContainerSizeType amount,
                            const Event &first,
                            const Event &second);
static void write_critical_section(std::ofstream &out,
                                    const Event &first,
                                    const Event &second);

static void write_order_before(std::ofstream &out, const Event &);
static void write_order_after(std::ofstream &out, const Event &);

extern FILE *yyin;

int convert(const char *source, const char *dest)
{
    yyin = fopen(source, "r");
    outfile.open("parser.out");
    yyparse();

    write_configuration(outfile, changes, declarations);
    
    fclose(yyin);
    outfile.close();

    return 1;
}


enum FixType {  fix_type_unkown = -1,
                fix_type_critical, 
                fix_type_order, 
                fix_type_atomic,
                fix_type_barrier, 
                fix_type_existing_critical, 
             };

enum CriticalOperation { critical_op_lock, 
                            critical_op_unlock, 
                            critical_op_master,
                        };

enum OrderOperation { order_op_wait, order_op_signal };

enum AtomicOperation { atomic_op_enter, atomic_op_exit };

enum BarrierOperation { barrier_op_wait };

static void write_configuration(std::ofstream &out, 
            const std::vector< FixEvent >&changes, 
            const std::vector< Decl >& declarations)
{
    for (std::vector< FixEvent >::const_iterator it=changes.begin();
            it!=changes.end(); ++it) {
        const Event *triggering_event = it->get_trigger_event();
        const EventNode *nodes = it->get_events();
        const EventNode *first_node = nodes->get_first();
        const EventNode *last_node = nodes->get_last();
        const Event *first_event = NULL;
        const Event *last_event = NULL;
        switch (nodes->get_event_op()) {
        case event_op_order: {
            if (first_node->get_event_op()!=event_op_noop 
                || last_node->get_event_op()!=event_op_noop) {
                std::cerr << "configuration file may have errors\n";
            }

            EventDescContainer::DescContainerSizeType amount_event_descs = 0;

            first_event = first_node->get_event();
            last_event = last_node->get_event();

            amount_event_descs = first_event->get_amount_descs()
                                + last_event->get_amount_descs();

            write_order(out, amount_event_descs, *first_event, *last_event);
            }
            break;
        case event_op_critical: {
            if (first_node->get_event_op()!=event_op_noop 
                || last_node->get_event_op()!=event_op_noop) {
                std::cerr << "configuration file may have errors\n";
            }

            first_event = first_node->get_event();
            last_event = last_node->get_event();

            EventDescContainer::DescContainer first_critial;
            EventDescContainer::DescContainer last_critical;

            EventDescContainer::DescContainerSizeType amount_event_descs = first_event->get_amount_descs()
                                + last_event->get_amount_descs();

            if (triggering_event) {
                write_critical_triggering(out, amount_event_descs+1,
                            *triggering_event, *first_event, *last_event);
            } else {
                write_critical_notriggering(out, amount_event_descs,
                            *first_event, *last_event);
            }

            }
            break;
        case event_op_barrier: {
            if (first_node->get_event_op()!=event_op_noop 
                || last_node->get_event_op()!=event_op_noop) {
                std::cerr << "configuration file may have errors\n";
            }

            EventDescContainer::DescContainerSizeType amount_event_descs = 0;

            first_event = first_node->get_event();
            last_event = last_node->get_event();

            amount_event_descs = first_event->get_amount_descs()
                                + last_event->get_amount_descs();

            write_barrier(out, amount_event_descs, *first_event, *last_event);
            
            }
            break;
        case event_op_atomic: {
            if (first_node->get_event_op()!=event_op_noop) {
                std::cerr << "configuration file may have errors\n";
            }

            EventDescContainer::DescContainerSizeType amount_event_descs = 0;

            first_event = first_node->get_event();

            amount_event_descs = first_event->get_amount_descs();

            write_atomic(out, amount_event_descs, *first_event);
            }
            break;
        case event_op_noop:
            break;
        }
    }
}

static void write_location_object_context(std::ofstream &out, const EventDesc &desc)
{
    out << " " << *desc.get_location();
    if (desc.get_object()!=NULL) {
        out << " -o " << *desc.get_object();
    }

    if (desc.get_context()!=NULL) {
        out << " -c " << *desc.get_context();
    }
}

static void write_order(std::ofstream &out,
                        EventDescContainer::DescContainerSizeType amount,
                        const Event &first, 
                        const Event &last)
{

            out << static_cast< int > (fix_type_order) << " " 
                << amount << std::endl;

            write_order_before(out, first);
            write_order_after(out, last);

            out << std::endl;
}

static void write_order_before(std::ofstream &out, const Event &event)
{
    EventDescContainer::DescContainer descs = event.get_descs();

    for (EventDescContainer::DescContainer::const_iterator it=descs.begin();
        it!=descs.end(); ++it) {
        out << static_cast< int> (order_op_wait);
        write_location_object_context(out, *it);
        out << std::endl; 
    }
}

static void write_order_after(std::ofstream &out, const Event &event)
{
    EventDescContainer::DescContainer descs = event.get_descs();

    for (EventDescContainer::DescContainer::const_iterator it=descs.begin();
        it!=descs.end(); ++it) {
        out << static_cast< int> (order_op_signal);
        write_location_object_context(out, *it);
        out << std::endl; 
    }
}

static void write_barrier(std::ofstream &out, 
                            EventDescContainer::DescContainerSizeType amount,
                            const Event &first, 
                            const Event &last)
{
    out << static_cast< int > (fix_type_barrier) << " "
        << amount << std::endl;

    EventDescContainer::DescContainer descs = first.get_descs();

    for (EventDescContainer::DescContainer::const_iterator it=descs.begin();
        it!=descs.end(); ++it) {
        out << static_cast< int > (barrier_op_wait);
        write_location_object_context(out, *it);
        out << std::endl;
    }    

    descs = last.get_descs();

    for (EventDescContainer::DescContainer::const_iterator it=descs.begin();
        it!=descs.end(); ++it) {
        out << static_cast< int > (barrier_op_wait);
        write_location_object_context(out, *it);
        out << std::endl;
    }    

    out << std::endl;
}

static void write_atomic(std::ofstream &out,
                            EventDescContainer::DescContainerSizeType amount,
                            const Event &first)
{
    out << static_cast< int >(fix_type_atomic) << " "
        << amount << std::endl;

    EventDescContainer::DescContainer descs = first.get_descs();
    bool is_lock = false;
    for (EventDescContainer::DescContainer::const_iterator it=descs.begin();
            it!=descs.end(); ++it) {
        if (it->get_op()!=NULL) {
            if (it->get_op()->compare("lock")==0) {
                is_lock = true;
            } else if (it->get_op()->compare("unlock")==0) {
                is_lock = false;
            }
        } else {
            is_lock = !is_lock;
        }

        if (is_lock) {
            out << static_cast< int > (atomic_op_enter) << " ";
        } else {
            out << static_cast< int > (atomic_op_exit) << " ";
        }
        write_location_object_context(out, *it);
        out << std::endl;
    }

    out << std::endl;
}

static void write_critical_triggering(std::ofstream &out,
                            EventDescContainer::DescContainerSizeType amount,
                            const Event &triggering,
                            const Event &first,
                            const Event &second)
{
    out << static_cast< int > (fix_type_critical) << " "
        << amount << std::endl;

    out << static_cast< int > (critical_op_master) << " ";
    write_location_object_context(out, triggering.get_descs().front());
    out << std::endl;

    write_critical_section(out, first, second);

    out << std::endl;
}

static void write_critical_notriggering(std::ofstream &out,
                            EventDescContainer::DescContainerSizeType amount,
                            const Event &first,
                            const Event &last)
{
    out << static_cast< int > (fix_type_critical) << " "
        << amount << std::endl;

    write_critical_section(out, first, last);

    out << std::endl;
}

static void write_critical_section(std::ofstream &out,
                                    const Event &first,
                                    const Event &last)
{
    bool is_lock = false;

    EventDescContainer::DescContainer descs = first.get_descs();

    for (EventDescContainer::DescContainer::const_iterator it=descs.begin();
            it!=descs.end(); ++it) {
        if (it->get_op()!=NULL) {
            if (it->get_op()->compare("lock")==0) {
                is_lock = true;
            } else if (it->get_op()->compare("unlock")==0) {
                is_lock = false;
            } 
        } else {
            is_lock = !is_lock;
        }

        if (is_lock) {
            out << static_cast< int > (critical_op_lock) << " ";
        } else {
            out << static_cast< int > (critical_op_unlock) << " ";
        }

        write_location_object_context(out, *it);
        out << std::endl;
    }

    descs = last.get_descs();

    for (EventDescContainer::DescContainer::const_iterator it=descs.begin();
            it!=descs.end(); ++it) {
        if (it->get_op()!=NULL) {
            if (it->get_op()->compare("lock")==0) {
                is_lock = true;
            } else if (it->get_op()->compare("unlock")==0) {
                is_lock = false;
            }
        } else {
            is_lock = !is_lock;
        }

        if (is_lock) {
            out << static_cast< int > (critical_op_lock) << " ";
        } else {
            out << static_cast< int > (critical_op_unlock) << " ";
        }

        write_location_object_context(out, *it);
        out << std::endl;
    }
}

bool class_name_equal::operator() (const Decl &decl, const char *n)
{
    const std::string &name = decl.get_name();
    std::string nn = n;

    return name==n;
}

bool class_name_equal::operator() (const Decl &decl)
{
    return name==decl.get_name();
}

class_name_equal::class_name_equal(const std::string &name)
{
    this->name = name;
}

static void print_event_desc(const EventDescContainer::DescContainer &descs)
{
    printf("{ ");
    for (EventDescContainer::DescContainer::const_iterator it=descs.begin();
            it!=descs.end(); ++it) {
        if (it->get_op()!=NULL) {
            printf("%s ", it->get_op()->c_str());
        }

        if (it->get_object()!=NULL) {
            printf("%s ", it->get_object()->c_str());
        }

        if (it->get_location()!=NULL) {
            printf("%s ", it->get_location()->c_str());
        }

        if (it->get_context()!=NULL) {
            printf("%s ", it->get_context()->c_str());
        }

        if (it!=descs.end()-1) {
            printf(",");
        }
    }

    printf("}");
}

static void print_event_node(const EventNode *node, int depth)
{
    EventOperation event_op = node->get_event_op();
    for (int i=0; i<depth; ++i) {
        printf("  ");
    }
    if (event_op == event_op_noop) {
        for (int i=0; i<depth; ++i) {
            printf("  ");
        }
        const Event *event = node->get_event();
        print_event_desc(event->get_descs());
    } else {
        const char *op = NULL; 
        switch (event_op) {
        case event_op_order:
            op = ">";
            break;
        case event_op_critical:
            op = "<>";
            break;
        case event_op_atomic:
            op = "*";
            break;
        case event_op_barrier:
            op = "|";
            break;
        default:
            op = "noop";
            break;
        }

        printf("%s\n", op);
        const EventNode *first = node->get_first();
        if (first != NULL) {
            print_event_node(first, depth+1);
            printf("\n");
        }
        const EventNode *last = node->get_last();
        if (last != NULL) {
            print_event_node(last, depth+1);
            printf("\n");
        }
    }
}

void print_events(FixEvent *event)
{
    const Event *triggering = event->get_trigger_event();
    if (triggering!=NULL) {
        printf("triggering event: ");
        print_event_desc(triggering->get_descs());
        printf("\n");
    }

    if (event->get_events()!=NULL) {
        print_event_node(event->get_events(), 0);
        printf("\n");
    }
}
