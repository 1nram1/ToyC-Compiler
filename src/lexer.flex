%option noyywrap

%{
#include <iostream>
#include <string>
#include <cstdlib>
#include "ast.hpp"
#include "parser.hpp"

// Include the union definition from parser
extern YYSTYPE yylval;
%}

/* Regular Definitions */
DIGIT    [0-9]
ID       [a-zA-Z_][a-zA-Z0-9_]*
WHITESPACE [ \t\n\r]+

%%

"/*"        { /* Handle multi-line comments */
                int c1 = 0, c2 = yyinput();
                while (c2 != EOF) {
                    if (c1 == '*' && c2 == '/') break;
                    c1 = c2;
                    c2 = yyinput();
                }
            }
"//"[^\n]*  /* Ignore single-line comments */

{WHITESPACE} { /* Ignore whitespace */ }

"if"        { return IF; }
"else"      { return ELSE; }
"while"     { return WHILE; }
"break"     { return BREAK; }
"continue"  { return CONTINUE; }
"return"    { return RETURN; }
"int"       { return INT; }
"void"      { return VOID; }

{DIGIT}+    {
                yylval.int_value = std::atoi(yytext);
                return INTEGER;
            }

{ID}        {
                yylval.string_value = new std::string(yytext);
                return ID;
            }

"&&"        { return AND; }
"||"        { return OR; }
"=="        { return EQ; }
"!="        { return NE; }
"!"        { return NOT; }
"<="        { return LE; }
">="        { return GE; }
"<"         { return LT; }
">"         { return GT; }
"="         { return ASSIGN; }
"+"         { return PLUS; }
"-"         { return MINUS; }
"*"         { return STAR; }
"/"         { return SLASH; }
"%"         { return PERCENT; }
";"         { return SEMI; }
","         { return COMMA; }
"("         { return LPAREN; }
")"         { return RPAREN; }
"{"         { return LBRACE; }
"}"         { return RBRACE; }

.           {
                std::cerr << "Lexer Error: Unexpected character '" << *yytext << "'" << std::endl;
                exit(1);
            }

%%
