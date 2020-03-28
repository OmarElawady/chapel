grammar Chapel;
 
program : toplevel_stmt_ls
 ;

toplevel_stmt_ls : 
                 | toplevel_stmt_ls toplevel_stmt
;

toplevel_stmt : stmt
;

stmt : stmt_level_expr SEMI
;

stmt_level_expr : call_expr
;

expr : addition_expr
;

fun_expr : lhs_expr
;

lhs_expr : ident_expr
;

primary_expr : ident_expr
             | literal
;

literal : INTLITERAL
;

call_expr : primary_expr
          | fun_expr LP opt_actual_ls RP
          | fun_expr LSB opt_actual_ls RSB
;

ident_expr : IDENTIFIER
;

opt_actual_ls : 
              | actual_ls
;

actual_ls : actual
          | actual_ls COMMA actual
;

actual: expr
;

exponentiation_expr : call_expr
                    | call_expr EXP exponentiation_expr
;

multiplication_expr : exponentiation_expr
                    | multiplication_expr (STAR | DIVIDE) exponentiation_expr
;

addition_expr : multiplication_expr
              | addition_expr (PLUS | MINUS) multiplication_expr
;

fragment LETTER : [a-zA-Z_];
fragment DIGIT : [0-9];
fragment LEGAL_ID_CHAR : (LETTER | DIGIT | '$');

LP : '(';
RP : ')';
LSB : '[';
RSB : ']';
PLUS : '+';
MINUS : '-';
STAR  : '*';
DIVIDE : '/';
SEMI : ';';
EXP : '**';
COMMA : ',';

IDENTIFIER : LETTER LEGAL_ID_CHAR*;
INTLITERAL : DIGIT (DIGIT|'_')*;
WHITESPACE : [\n\r\t ] -> skip ;
NEWLINE : '\n' -> skip;
