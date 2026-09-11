grammar StateMachineDsl;

model: machine EOF;

machine: 'machine' ID '{' statesDecl initialDecl eventsDecl classBlock* actionsBlock? stateBlock* transition* '}';
statesDecl: 'states' ':' ID (',' ID)*;
initialDecl: 'initial' ':' ID;
eventsDecl: 'events' ':' ID (',' ID)*;

stateBlock: 'state' ID '{' (entryDecl | exitDecl | initialDecl | historyDecl | historyDeepDecl | concurrentDecl | stateBlock)* '}';
entryDecl: 'entry' ':' action;
exitDecl: 'exit' ':' action;
historyDecl: 'history' ':' ID;
historyDeepDecl: 'history*' ':' ID;
concurrentDecl: 'concurrent';

transition: 'transition' ID '->' ID 'on' ID '{' (guardDecl | actionDecl)* '}';
guardDecl: 'guard' ':' expr;
actionDecl: 'action' ':' action;

classBlock: 'class' ID '{' cppDecl headerDecl '}';
cppDecl: 'cpp' ':' STRING;
headerDecl: 'header' ':' STRING;
actionsBlock: 'actions' '{' actionMap* '}';
actionMap: ID ':' ID '.' ID;

action: ID ('.' ID)*;
expr: orExpr;
orExpr: andExpr ('||' andExpr)*;
andExpr: primary ('&&' primary)*;
primary: '(' orExpr ')' | comparison;
comparison: ID op (ID | INT | STRING);
op: '==' | '!=' | '>' | '>=' | '<' | '<=';

COMMENT: '#' ~[\r\n]* -> skip;
ID: [a-zA-Z_][a-zA-Z0-9_]*;
INT: [0-9]+;
STRING: '"' ('\\' . | ~["\\])* '"';
WS: [ \t\r\n]+ -> skip;
