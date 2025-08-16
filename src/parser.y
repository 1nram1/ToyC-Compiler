%{
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include "ast.hpp"

// Global variables for token values from lexer
std::string string_val;
int int_val;

// Forward declarations
extern int yylex();
extern void yyerror(const char* s);
extern FILE* yyin;

// Global variable to store the parsed AST
std::unique_ptr<ProgramWithFunctions> parsed_program;
%}

%union {
    int int_value;
    std::string* string_value;
    std::string* type_value;
    Expr* expr_ptr;
    Stmt* stmt_ptr;
    std::vector<std::unique_ptr<Stmt>>* stmt_list_ptr;
    Parameter* param_ptr;
    std::vector<std::unique_ptr<Parameter>>* param_list_ptr;
    FunctionDecl* func_decl_ptr;
    std::vector<std::unique_ptr<FunctionDecl>>* func_list_ptr;
    std::vector<std::unique_ptr<Expr>>* arg_list_ptr;
}

// Define the types for terminal symbols (tokens from lexer)
%token <int_value> INTEGER
%token <string_value> ID

%token IF ELSE WHILE BREAK CONTINUE RETURN INT VOID
%token EQ NE LT GT LE GE
%token ASSIGN
%token PLUS MINUS
%token STAR SLASH PERCENT
%token LPAREN RPAREN
%token LBRACE RBRACE
%token SEMI
%token COMMA
%token AND OR

// Define the types for our non-terminal symbols
%type <func_list_ptr> program function_list
%type <func_decl_ptr> function_decl
%type <param_list_ptr> parameter_list parameter_list_opt
%type <param_ptr> parameter
%type <type_value> type_specifier
%type <arg_list_ptr> argument_list argument_list_opt
%type <stmt_list_ptr> stmt_list
%type <stmt_ptr> stmt decl_stmt expr_stmt if_stmt while_stmt break_stmt continue_stmt return_stmt block_stmt empty_stmt
%type <expr_ptr> expr assign_expr logical_or_expr logical_and_expr equality_expr relational_expr additive_expr multiplicative_expr unary_expr primary_expr

// Define operator precedence and associativity
%right ASSIGN
%left OR
%left AND
%left EQ NE
%left LT GT LE GE
%left PLUS MINUS
%left STAR SLASH PERCENT
%right UMINUS UPLUS// For unary minus
%left NOT
%right UNOT

%%

program:
    function_list { 
        parsed_program = std::make_unique<ProgramWithFunctions>(std::move(*$1)); 
        delete $1;
    }
    ;

function_list:
    function_decl { 
        $$ = new std::vector<std::unique_ptr<FunctionDecl>>(); 
        $$->push_back(std::unique_ptr<FunctionDecl>($1)); 
    }
    | function_list function_decl { 
        $1->push_back(std::unique_ptr<FunctionDecl>($2)); 
        $$ = $1; 
    }
    ;

function_decl:
    type_specifier ID LPAREN parameter_list_opt RPAREN block_stmt {
        $$ = new FunctionDecl(*$1, *$2, std::move(*$4), std::unique_ptr<Stmt>($6));
        delete $1;
        delete $2;
        delete $4;
    }
    ;

type_specifier:
    INT { $$ = new std::string("int"); }
    | VOID { $$ = new std::string("void"); }
    ;

parameter_list_opt:
    /* empty */ { $$ = new std::vector<std::unique_ptr<Parameter>>(); }
    | parameter_list { $$ = $1; }
    ;

parameter_list:
    parameter { 
        $$ = new std::vector<std::unique_ptr<Parameter>>(); 
        $$->push_back(std::unique_ptr<Parameter>($1)); 
    }
    | parameter_list COMMA parameter { 
        $1->push_back(std::unique_ptr<Parameter>($3)); 
        $$ = $1; 
    }
    ;

parameter:
    type_specifier ID { 
        $$ = new Parameter(*$1, *$2); 
        delete $1;
        delete $2;
    }
    ;

stmt_list:
    stmt { 
        $$ = new std::vector<std::unique_ptr<Stmt>>(); 
        $$->push_back(std::unique_ptr<Stmt>($1)); 
    }
    | stmt_list stmt { 
        $1->push_back(std::unique_ptr<Stmt>($2)); 
        $$ = $1; 
    }
    ;

stmt:
    decl_stmt { $$ = $1; }
    | expr_stmt { $$ = $1; }
    | if_stmt { $$ = $1; }
    | while_stmt { $$ = $1; }
    | break_stmt { $$ = $1; }
    | continue_stmt { $$ = $1; }
    | return_stmt { $$ = $1; }
    | block_stmt { $$ = $1; }
    | empty_stmt { $$ = $1; }
    ;

decl_stmt:
    type_specifier ID ASSIGN expr SEMI { 
        $$ = new DeclStmt(std::make_unique<IdExpr>(*$2), std::unique_ptr<Expr>($4)); 
        delete $1;
        delete $2;
    }
    | type_specifier ID { 
        $$ = new DeclStmt(std::make_unique<IdExpr>(*$2), nullptr); 
        delete $1;
        delete $2;
    }
    ;

expr_stmt:
    expr SEMI { $$ = new ExprStmt(std::unique_ptr<Expr>($1)); }
    ;
    
empty_stmt:
    SEMI { $$ = new EmptyStmt(); }
    ;

if_stmt:
    IF LPAREN expr RPAREN stmt ELSE stmt { 
        $$ = new IfStmt(std::unique_ptr<Expr>($3), std::unique_ptr<Stmt>($5), std::unique_ptr<Stmt>($7)); 
    }
    | IF LPAREN expr RPAREN stmt { 
        $$ = new IfStmt(std::unique_ptr<Expr>($3), std::unique_ptr<Stmt>($5), nullptr); 
    }
    ;

while_stmt:
    WHILE LPAREN expr RPAREN stmt { 
        $$ = new WhileStmt(std::unique_ptr<Expr>($3), std::unique_ptr<Stmt>($5)); 
    }
    ;

break_stmt:
    BREAK SEMI { $$ = new BreakStmt(); }
    ;

continue_stmt:
    CONTINUE SEMI { $$ = new ContinueStmt(); }
    ;

return_stmt:
    RETURN expr SEMI { $$ = new ReturnStmt(std::unique_ptr<Expr>($2)); }
    | RETURN SEMI { $$ = new ReturnStmt(nullptr); }
    ;

block_stmt:
    LBRACE stmt_list RBRACE { 
        $$ = new BlockStmt(std::move(*$2)); 
        delete $2;
    }
    | LBRACE RBRACE { 
        $$ = new BlockStmt(std::vector<std::unique_ptr<Stmt>>{}); 
    }
    ;

expr:
    assign_expr { $$ = $1; }
    ;

assign_expr:
    logical_or_expr { $$ = $1; }
    | ID ASSIGN expr { 
        $$ = new AssignExpr(std::make_unique<IdExpr>(*$1), std::unique_ptr<Expr>($3)); 
        delete $1;
    }
    ;

logical_or_expr:
    logical_and_expr { $$ = $1; }
    | logical_or_expr OR logical_and_expr { 
        $$ = new BinaryOpExpr("||", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    ;

logical_and_expr:
    equality_expr { $$ = $1; }
    | logical_and_expr AND equality_expr { 
        $$ = new BinaryOpExpr("&&", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    ;

equality_expr:
    relational_expr { $$ = $1; }
    | equality_expr EQ relational_expr { 
        $$ = new BinaryOpExpr("==", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    | equality_expr NE relational_expr { 
        $$ = new BinaryOpExpr("!=", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    ;

relational_expr:
    additive_expr { $$ = $1; }
    | relational_expr LT additive_expr { 
        $$ = new BinaryOpExpr("<", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    | relational_expr GT additive_expr { 
        $$ = new BinaryOpExpr(">", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    | relational_expr LE additive_expr { 
        $$ = new BinaryOpExpr("<=", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    | relational_expr GE additive_expr { 
        $$ = new BinaryOpExpr(">=", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    ;

additive_expr:
    multiplicative_expr { $$ = $1; }
    | additive_expr PLUS multiplicative_expr { 
        $$ = new BinaryOpExpr("+", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    | additive_expr MINUS multiplicative_expr { 
        $$ = new BinaryOpExpr("-", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    ;

multiplicative_expr:
    unary_expr { $$ = $1; }
    | multiplicative_expr STAR unary_expr { 
        $$ = new BinaryOpExpr("*", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    | multiplicative_expr SLASH unary_expr { 
        $$ = new BinaryOpExpr("/", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    | multiplicative_expr PERCENT unary_expr { 
        $$ = new BinaryOpExpr("%", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); 
    }
    ;

unary_expr:
    primary_expr { $$ = $1; }
    | MINUS unary_expr %prec UMINUS { 
        $$ = new UnaryOpExpr("-", std::unique_ptr<Expr>($2)); 
    }
    | PLUS unary_expr %prec UPLUS { 
        $$ = new UnaryOpExpr("+", std::unique_ptr<Expr>($2)); 
    }
    | NOT unary_expr %prec UNOT { 
        $$ = new UnaryOpExpr("!", std::unique_ptr<Expr>($2)); 
    }
    ;

argument_list_opt:
    /* empty */ { $$ = new std::vector<std::unique_ptr<Expr>>(); }
    | argument_list { $$ = $1; }
    ;

argument_list:
    expr { 
        $$ = new std::vector<std::unique_ptr<Expr>>(); 
        $$->push_back(std::unique_ptr<Expr>($1)); 
    }
    | argument_list COMMA expr { 
        $1->push_back(std::unique_ptr<Expr>($3)); 
        $$ = $1; 
    }
    ;

primary_expr:
    ID { 
        $$ = new IdExpr(*$1); 
        delete $1;
    }
    | INTEGER { 
        $$ = new IntegerExpr($1); 
    }
    | LPAREN expr RPAREN { $$ = $2; }
    | ID LPAREN argument_list_opt RPAREN { 
        std::vector<std::unique_ptr<Expr>> args = std::move(*$3);
        $$ = new FunctionCallExpr(*$1, std::move(args)); 
        delete $1;
        delete $3;
    }
    ;

%%

void yyerror(const char* s) {
    std::cerr << "Parser Error: " << s << std::endl;
}
