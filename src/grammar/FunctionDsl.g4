grammar FunctionDsl;

model: block* EOF;

block: typesBlock | valuesBlock | resourceBlock | funcBlock | setupBlock | classBlock;

typesBlock: 'types' '{' typeMap* '}';
typeMap: ID '->' STRING;

valuesBlock: 'values' '{' valueSource* '}';
valueSource: ID ':' (list | range);
list: '[' (STRING (',' STRING)*)? ']';
range: INT '..' INT;

resourceBlock: 'resource' ID '{' (ctypeDecl | statesDecl | initialDecl | observeDecl | varDecl | listDecl)* '}';
ctypeDecl: 'ctype' ':' STRING;
statesDecl: 'states' ':' ID (',' ID)*;
initialDecl: 'initial' ':' ID;
observeDecl: 'observe' ':' ID;
varDecl: 'var' ID '=' INT;
listDecl: 'list' ID '=' '[' (INT (',' INT)*)? ']';

funcBlock: 'func' ID '(' params? ')' ('->' typeName)? '{' funcMember* '}';
params: param (',' param)*;
param: 'out'? ID ':' typeName ('from' ID)?;
typeName: ID;

funcMember: symbolDecl | signatureDecl | requiresDecl | effectsDecl | successDecl | receiverDecl | updateDecl;
symbolDecl: 'symbol' ':' STRING;
signatureDecl: 'signature' ':' STRING;
requiresDecl: 'requires' ':' (cond (',' cond)*)?;
effectsDecl: 'effects' ':' (effect (',' effect)*)?;
successDecl: 'success' ':' successExpr;
updateDecl: 'update' ':' updateItem (',' updateItem)*;
updateItem: ID ('='|'+='|'-=') (ID|INT) | ID '<<' (ID|INT) | ID '>>';

setupBlock: 'setup' '{' setupEntry* '}';
setupEntry: ID ':' ID '(' (setupArg (',' setupArg)*)? ')' 'x' INT;
setupArg: STRING | INT;

classBlock: 'class' ID '{' cppDecl headerDecl '}';
cppDecl: 'cpp' ':' STRING;
headerDecl: 'header' ':' STRING;
receiverDecl: 'receiver' ':' ID;

cond: ID 'is' ID;
effect: target '->' ID;
target: 'result' | ID;

successExpr: orExpr;
orExpr: andExpr ('||' andExpr)*;
andExpr: primary ('&&' primary)*;
primary: 'true' | 'false' | '(' orExpr ')' | operand op operand;
operand: 'result' | 'NULL' | ID | INT | 'len' '(' ID ')' | 'front' '(' ID ')';
op: '==' | '!=' | '>' | '>=' | '<' | '<=';

COMMENT: '#' ~[\r\n]* -> skip;
ID: [a-zA-Z_][a-zA-Z0-9_]*;
INT: [0-9]+;
STRING: '"' ('\\' . | ~["\\])* '"';
WS: [ \t\r\n]+ -> skip;
