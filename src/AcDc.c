#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include<ctype.h>
#include "header.h"

int numOfParen = 0;

int main( int argc, char *argv[] )
{
    FILE *source, *target;
    Program program;
    SymbolTable symtab;

    if( argc == 3){
        source = fopen(argv[1], "r");
        target = fopen(argv[2], "w");
        if( !source ){
            printf("can't open the source file\n");
            exit(2);
        }
        else if( !target ){
            printf("can't open the target file\n");
            exit(2);
        }
        else{
            printf("parsing...\n");
            program = parser(source);
            printf("fclosing...\n");
            fclose(source);
            printf("building...\n");
            symtab = build(program);
            printf("checking...\n");
            check(&program, &symtab);
            printf("generating...\n");
            gencode(program, target, &symtab);
        }
    }
    else{
        printf("Usage: %s source_file target_file\n", argv[0]);
    }


    return 0;
}


/********************************************* 
  Scanning 
 *********************************************/
Token getNumericToken( FILE *source, char c )
{
    Token token;
    int i = 0;

    while( isdigit(c) ) {
        token.tok[i++] = c;
        c = fgetc(source);
    }

    if( c != '.' ){
        ungetc(c, source);
        token.tok[i] = '\0';
        token.type = IntValue;
        return token;
    }

    token.tok[i++] = '.';

    c = fgetc(source);
    if( !isdigit(c) ){
        ungetc(c, source);
        printf("Error: expect a digit but get: %c\n", c);
        exit(1);
    }

    while( isdigit(c) ){
        token.tok[i++] = c;
        c = fgetc(source);
    }

    ungetc(c, source);
    token.tok[i] = '\0';
    token.type = FloatValue;
    return token;
}

Token scanner( FILE *source )
{
    char c;
    Token token;

    while( !feof(source) ){
        // Get first char
        c = fgetc(source);
        while( isspace(c) ){
            // Keep getting char until it's not a space char
            c = fgetc(source);
        }
        if( isdigit(c) ){
            // If c is a number, we assume this token is a number.
            return getNumericToken(source, c);
        }

        int tokenLength = 0;
        if(isalpha(c) || c == '_'){
            // If c begins with alphabet or '_', we assume it is a variable.
            do{
                token.tok[tokenLength] = c;
                tokenLength++;            
                c = fgetc(source);
            }while(isalpha(c) || isdigit(c) || c == '_'); // Parse char until it's not a digit, alphabet or '_'.
            ungetc(c, source);
        }
        else{
            token.tok[tokenLength] = c;
            tokenLength++; 
        }

        token.tok[tokenLength] = '\0';
        token.type = Alphabet; // Default type is Alphabet
        
        if(tokenLength == 1){
            c = token.tok[0];
            //printf("c is %c ASCII is %d\n", c, c);
            if( islower(c) ){
                if( c == 'f' )
                    token.type = FloatDeclaration;
                else if( c == 'i' )
                    token.type = IntegerDeclaration;
                else if( c == 'p' ){
                    token.type = PrintOp;
                }    
                return token;
            }

            switch(c){
                case '=':
                    token.type = AssignmentOp;
                    return token;
                case '+':
                    token.type = PlusOp;
                    return token;
                case '-':
                    token.type = MinusOp;
                    return token;
                case '*':
                    token.type = MulOp;
                    return token;
                case '/':
                    token.type = DivOp;
                    return token;
                case '(':
                    token.type = leftParen;
                    return token;
                case ')':
                    token.type = rightParen;
                    return token;
                case EOF:
                    token.type = EOFsymbol;
                    token.tok[0] = '\0';
                    return token;
                default:
                    printf("Error: invalid character : %c\n", c);
                    exit(1);
            }
        }
        else{
            return token;
        }
    }

    token.tok[0] = '\0';
    token.type = EOFsymbol;
    return token;
}


/********************************************************
  Parsing
 *********************************************************/
Declaration parseDeclaration( FILE *source, Token token )
{
    Token token2;
    switch(token.type){
        case FloatDeclaration:
        case IntegerDeclaration:
            token2 = scanner(source);
            if (strcmp(token2.tok, "f") == 0 ||
                    strcmp(token2.tok, "i") == 0 ||
                    strcmp(token2.tok, "p") == 0) {
                printf("Syntax Error: %s cannot be used as id\n", token2.tok);
                exit(1);
            }
            return makeDeclarationNode( token, token2 );
        default:
            printf("Syntax Error: Expect Declaration %s\n", token.tok);
            exit(1);
    }
}

Declarations *parseDeclarations( FILE *source )
{
    Token token = scanner(source);
    Declaration decl;
    Declarations *decls;
    switch(token.type){
        case FloatDeclaration:
        case IntegerDeclaration:
            decl = parseDeclaration(source, token);
            decls = parseDeclarations(source);
            return makeDeclarationTree( decl, decls );
        case PrintOp:
        case Alphabet:
            ungetToken(token, source);
            return NULL;
        case EOFsymbol:
            return NULL;
        default:
            printf("Syntax Error: Expect declarations %s\n", token.tok);
            exit(1);
    }
}

Expression *parseValue( FILE *source )
{
    Token token = scanner(source);
    Expression *value = (Expression *)malloc( sizeof(Expression) );
    value->leftOperand = value->rightOperand = NULL;

    switch(token.type){
        case Alphabet:
            (value->v).type = Identifier;
            strcpy((value->v).val.id, token.tok);
            (value->v).val.id[strlen(token.tok)] = '\0';
            break;
        case IntValue:
            (value->v).type = IntConst;
            (value->v).val.ivalue = atoi(token.tok);
            break;
        case FloatValue:
            (value->v).type = FloatConst;
            (value->v).val.fvalue = atof(token.tok);
            break;
        case leftParen:
            numOfParen++;
            free(value); // To free value because we haven't used it.
            return parseExpression(source);
        default:
            printf("Syntax Error: Expect Identifier or a Number %s\n", token.tok);
            exit(1);
    }

    return value;
}

// To unget a token
void ungetToken(Token token, FILE *source) {
    
    int tokenLength = strlen(token.tok);
    for(int i = tokenLength - 1; i >= 0; i--) {
        ungetc(token.tok[i], source);
    }
}

// termTail -> * val termTail | / val termTail | epsilon
Expression *parseTermTail(FILE* source, Expression* lvalue){
    Token token = scanner(source);
    Expression *term;

    switch(token.type){
        case MulOp:
            // * val termTail
            term = (Expression *)malloc( sizeof(Expression) );
            (term->v).type = MulNode;
            (term->v).val.op = Mul;
            term->leftOperand = lvalue;
            term->rightOperand = parseValue(source);
            return parseTermTail(source, term);
        case DivOp:
            // / val termTail
            term = (Expression *)malloc( sizeof(Expression) );
            (term->v).type = DivNode;
            (term->v).val.op = Div;
            term->leftOperand = lvalue;
            term->rightOperand = parseValue(source);
            return parseTermTail(source, term);
        case PlusOp: case MinusOp: case Alphabet: case PrintOp: case rightParen: //epsilon
            ungetToken(token, source);
            return lvalue;
        case EOFsymbol:
            return lvalue;
        default:
            printf("Syntax Error: In term Expect a operand but %s\n", token.tok);
            exit(1);
    }
}

// term -> val termTail
Expression *parseTerm(FILE *source){
    Expression *val = parseValue(source); // val
    return parseTermTail(source, val); // termTail
}

// exprterm -> + term exprtail | - term exprtail | epsilon
Expression *parseExpressionTail(FILE *source, Expression *lvalue){
    Token token = scanner(source);
    Expression *expr;

    switch(token.type){
        case PlusOp:
            // + term exprtail
            expr = (Expression *)malloc( sizeof(Expression) );
            (expr->v).type = PlusNode;
            (expr->v).val.op = Plus;
            expr->leftOperand = lvalue;
            expr->rightOperand = parseTerm(source);
            return parseExpressionTail(source, expr);
        case MinusOp:
            // - term exprtail
            expr = (Expression *)malloc( sizeof(Expression) );
            (expr->v).type = MinusNode;
            (expr->v).val.op = Minus;
            expr->leftOperand = lvalue;
            expr->rightOperand = parseTerm(source);
            return parseExpressionTail(source, expr);
        case rightParen:
            numOfParen--;
            return lvalue;
        case MulOp: case DivOp: case Alphabet: case PrintOp: // epsilon
            ungetToken(token, source);
            return lvalue;
        case EOFsymbol:
            return lvalue;
        default:
            printf("Syntax Error: In expression Expect a operand but %s\n", token.tok);
            exit(1);
    }
}

/*
    expr -> term exprtail
    exprtail -> + term exprtail | - term exprtail | epsilon
    term -> val termtail
    termtail -> * val termtail | / val termtail | epsilon
    val -> INTEGER | FLOAT | id | (expr)
*/
Expression *parseExpression( FILE *source){
    Expression *term = parseTerm(source); // term
    return parseExpressionTail(source, term); // exprtail
}

Statement parseStatement( FILE *source, Token token )
{
    Token next_token;
    Expression *expr;
    //printf("In statement token is %s\n", token.tok);
    switch(token.type){
        case Alphabet:
            next_token = scanner(source);
            //printf("Statement token is %s\n", next_token.tok);
            if(next_token.type == AssignmentOp){
                expr = parseExpression(source);
                if (numOfParen!= 0){
                    printf("Syntax Error: number of left and right parentheses are different.\n");
                    exit(1);
                }
                
                /*
                printf("##########");
                print_expr(expr);
                printf("##########\n");
                */

                return makeAssignmentNode(token.tok, expr);
            }
            else{
                printf("Syntax Error: Expect an assignment op %s\n", next_token.tok);
                exit(1);
            }
        case PrintOp:
            next_token = scanner(source);
            //printf("PrintOp token is %s\n", next_token.tok);
            if(next_token.type == Alphabet)
                return makePrintNode(next_token.tok);
            else{
                printf("Syntax Error: Expect an identifier %s\n", next_token.tok);
                exit(1);
            }
            break;
        default:
            printf("Syntax Error: Expect a statement %s\n", token.tok);
            exit(1);
    }
}

Statements *parseStatements( FILE * source )
{

    Token token = scanner(source);
    //printf("Statements token = %s\n", token.tok);
    Statement stmt;
    Statements *stmts;

    switch(token.type){
        case Alphabet:
        case PrintOp:
            stmt = parseStatement(source, token);
            stmts = parseStatements(source);
            return makeStatementTree(stmt , stmts);
        case EOFsymbol:
            return NULL;
        default:
            printf("Syntax Error: Expect statements %s\n", token.tok);
            exit(1);
    }
}


/*********************************************************************
  Build AST
 **********************************************************************/
Declaration makeDeclarationNode( Token declare_type, Token identifier )
{
    Declaration tree_node;

    switch(declare_type.type){
        case FloatDeclaration:
            tree_node.type = Float;
            break;
        case IntegerDeclaration:
            tree_node.type = Int;
            break;
        default:
            break;
    }
    strcpy(tree_node.name, identifier.tok);
    tree_node.name[strlen(identifier.tok)] = '\0';

    return tree_node;
}

Declarations *makeDeclarationTree( Declaration decl, Declarations *decls )
{
    Declarations *new_tree = (Declarations *)malloc( sizeof(Declarations) );
    new_tree->first = decl;
    new_tree->rest = decls;

    return new_tree;
}


Statement makeAssignmentNode( char *id, Expression *expr_tail )
{
    Statement stmt;
    AssignmentStatement assign;
    stmt.type = Assignment;
    strcpy(assign.id, id); 
    assign.id[strlen(id)] = '\0';
    assign.expr = expr_tail;
    stmt.stmt.assign = assign;
    return stmt;
}

Statement makePrintNode( char *id )
{
    Statement stmt;
    stmt.type = Print;
    strcpy(stmt.stmt.variable, id);
    stmt.stmt.variable[strlen(id)] = '\0';
    return stmt;
}

Statements *makeStatementTree( Statement stmt, Statements *stmts )
{
    Statements *new_tree = (Statements *)malloc( sizeof(Statements) );
    new_tree->first = stmt;
    new_tree->rest = stmts;

    return new_tree;
}

/* parser */
Program parser( FILE *source )
{
    Program program;

    program.declarations = parseDeclarations(source);
    program.statements = parseStatements(source);

    return program;
}


/********************************************************
  Build symbol table
 *********************************************************/
void InitializeTable( SymbolTable *table )
{
    int i;
    table->numberOfSymbol = 0;
    for(i = 0 ; i < MAX_SYMBOL; i++)
        table->table[i] = Notype;
        table->valid[i] = 0;
}

void add_table(SymbolTable *table, char *c, DataType type){   
    /*
    int index = (int)(c - 'a');

    if(table->table[index] != Notype)
        printf("Error : id %c has been declared\n", c);//error
    table->table[index] = t;
    */
    printf("add identifier %s\n", c);
    int numberOfSymbol = table->numberOfSymbol;
    for(int i = 0; i < numberOfSymbol; i++){
        if(strcmp(table->id[i], c) == 0){
            printf("Error : id %s has been declared\n", c);//error
            exit(1);
        }
    }
    
    table->table[numberOfSymbol] = type;
    strcpy(table->id[numberOfSymbol], c);
    table->id[numberOfSymbol][strlen(c)] = '\0';
    table->valid[numberOfSymbol] = 1;
    table->numberOfSymbol++;
    return;
}

SymbolTable build( Program program )
{
    SymbolTable table;
    Declarations *decls = program.declarations;
    Declaration current;

    InitializeTable(&table);

    while(decls !=NULL){
        current = decls->first;
        add_table(&table, current.name, current.type);
        decls = decls->rest;
    }

    return table;
}


/********************************************************************
  Type checking
 *********************************************************************/

void convertType( Expression * old, DataType type )
{
    if(old->type == Float && type == Int){
        printf("error : can't convert float to integer\n");
        exit(1);
    }
    if(old->type == Int && type == Float){
        Expression *tmp = (Expression *)malloc( sizeof(Expression) );
        if((old->v).type == Identifier)
            printf("convert to float %s \n", (old->v).val.id);
        else
            printf("convert to float %d \n", (old->v).val.ivalue);
        tmp->v = old->v;
        tmp->leftOperand = old->leftOperand;
        tmp->rightOperand = old->rightOperand;
        tmp->type = old->type;

        Value v;
        v.type = IntToFloatConvertNode;
        v.val.op = IntToFloatConvert;
        old->v = v;
        old->type = Int;
        old->leftOperand = tmp;
        old->rightOperand = NULL;
    }
}

DataType generalize( Expression *left, Expression *right )
{
    if(left->type == Float || right->type == Float){
        printf("generalize : float\n");
        return Float;
    }
    printf("generalize : int\n");
    return Int;
}

DataType lookup_table( SymbolTable *table, char *c )
{
    /*
    int id = c-'a';
    if( table->table[id] != Int && table->table[id] != Float)
        printf("Error : identifier %c is not declared\n", c);//error
    return table->table[id];
    */
    int numberOfSymbol = table->numberOfSymbol;
    for(int i = 0; i < numberOfSymbol; i++){
        if(strcmp(table->id[i], c) == 0){
            return table->table[i];
        }
    }
    printf("Error : identifier %s is not declared\n", c);//error
    exit(1);
}

void constFolding(Expression *expr){
    Expression *left = expr->leftOperand;
    Expression *right = expr->rightOperand;
    
    if((left->v).type == FloatConst && (right->v).type == FloatConst){
        float lValue = (left->v).val.fvalue;
        float rValue = (right->v).val.fvalue;
        switch((expr->v).type){
            case PlusNode:
                (expr->v).val.fvalue = lValue + rValue;
                break;
            case MinusNode:
                (expr->v).val.fvalue = lValue - rValue;
                break;
            case MulNode:
                (expr->v).val.fvalue = lValue * rValue;
                break;
            case DivNode:
                (expr->v).val.fvalue = lValue / rValue;
                break;
            default:
                printf("Constant folding error!!\n");
                exit(1);
        }
        (expr->v).type = FloatConst;
        expr->leftOperand = NULL;
        expr->rightOperand = NULL;
    }
    else if((left->v).type == IntConst && (right->v).type == IntConst){
        int lValue = (left->v).val.ivalue;
        int rValue = (right->v).val.ivalue;
        switch((expr->v).type){
            case PlusNode:
                (expr->v).val.ivalue = lValue + rValue;
                break;
            case MinusNode:
                (expr->v).val.ivalue = lValue - rValue;
                break;
            case MulNode:
                (expr->v).val.ivalue = lValue * rValue;
                break;
            case DivNode:
                (expr->v).val.ivalue = lValue / rValue;
                break;
            default:
                printf("Constant folding error!!\n");
                exit(1);
        }
        (expr->v).type = IntConst;
        expr->leftOperand = NULL;
        expr->rightOperand = NULL;
    }
    else if((left->v).type == FloatConst && (right->v).type == IntToFloatConvertNode && (right->leftOperand->v).type == IntConst){
        float lValue = (left->v).val.fvalue;
        float rValue = (float)(right->leftOperand->v).val.ivalue;
        switch((expr->v).type){
            case PlusNode:
                (expr->v).val.fvalue = lValue + rValue;
                break;
            case MinusNode:
                (expr->v).val.fvalue = lValue - rValue;
                break;
            case MulNode:
                (expr->v).val.fvalue = lValue * rValue;
                break;
            case DivNode:
                (expr->v).val.fvalue = lValue / rValue;
                break;
            default:
                printf("Constant folding error!!\n");
                exit(1);
        }
        (expr->v).type = FloatConst;
        expr->leftOperand = NULL;
        expr->rightOperand = NULL;
    }
    else if((left->v).type == IntToFloatConvertNode && (right->v).type == FloatConst && (left->leftOperand->v).type == IntConst){
        float lValue = (float)(left->leftOperand->v).val.ivalue;
        float rValue = (right->v).val.fvalue;
        switch((expr->v).type){
            case PlusNode:
                (expr->v).val.fvalue = lValue + rValue;
                break;
            case MinusNode:
                (expr->v).val.fvalue = lValue - rValue;
                break;
            case MulNode:
                (expr->v).val.fvalue = lValue * rValue;
                break;
            case DivNode:
                (expr->v).val.fvalue = lValue / rValue;
                break;
            default:
                printf("Constant folding error!!\n");
                exit(1);
        }
        (expr->v).type = FloatConst;
        expr->leftOperand = NULL;
        expr->rightOperand = NULL;
    }
}

void checkexpression( Expression * expr, SymbolTable * table )
{
    char c[MAX_ID_LENGTH];
    //printf("----------\n");
    //print_expr(expr);
    //printf("----------\n");
    if(expr->leftOperand == NULL && expr->rightOperand == NULL){
        switch((expr->v).type){
            case Identifier:
                strcpy(c, (expr->v).val.id);
                expr->type = lookup_table(table, c);
                break;
            case IntConst:
                expr->type = Int;
                break;
            case FloatConst:
                expr->type = Float;
                break;
                //case PlusNode: case MinusNode: case MulNode: case DivNode:
            default:
                break;
        }
    }
    else{
        Expression *left = expr->leftOperand;
        Expression *right = expr->rightOperand;

        checkexpression(left, table);
        checkexpression(right, table);

        DataType type = generalize(left, right);
        convertType(left, type);//left->type = type;//converto
        convertType(right, type);//right->type = type;//converto
        expr->type = type;

        // Constant folding.
        constFolding(expr); 
    }
}

void checkstmt( Statement *stmt, SymbolTable * table )
{
    if(stmt->type == Assignment){
        AssignmentStatement assign = stmt->stmt.assign;
        printf("assignment : %s \n", assign.id);
        checkexpression(assign.expr, table);
        stmt->stmt.assign.type = lookup_table(table, assign.id);
        if (assign.expr->type == Float && stmt->stmt.assign.type == Int) {
            printf("error : can't convert float to integer\n");
            exit(1);
        } else {
            convertType(assign.expr, stmt->stmt.assign.type);
        }
    }
    else if (stmt->type == Print){
        printf("print : %s \n",stmt->stmt.variable);
        lookup_table(table, stmt->stmt.variable);
    }
    else{
        printf("error : statement error\n");//error
        exit(1);
    }
}

void check( Program *program, SymbolTable * table )
{
    Statements *stmts = program->statements;
    while(stmts != NULL){
        checkstmt(&stmts->first,table);
        stmts = stmts->rest;
    }
}


/***********************************************************************
  Code generation
 ************************************************************************/
void fprint_op( FILE *target, ValueType op )
{
    switch(op){
        case MinusNode:
            fprintf(target,"-\n");
            break;
        case PlusNode:
            fprintf(target,"+\n");
            break;
        case MulNode:
            fprintf(target,"*\n");
            break;
        case DivNode:
            fprintf(target,"/\n");
            break;
        default:
            fprintf(target,"Error in fprintf_op ValueType = %d\n",op);
            break;
    }
}

void fprint_expr( FILE *target, Expression *expr, SymbolTable *table)
{

    if(expr->leftOperand == NULL){
        switch( (expr->v).type ){
            case Identifier:
                fprintf(target,"l%c\n", assignRegister(table, (expr->v).val.id));
                break;
            case IntConst:
                if((expr->v).val.ivalue >= 0){
                    fprintf(target, "%d\n", (expr->v).val.ivalue);
                }
                else{
                    fprintf(target, "_%d\n", -(expr->v).val.ivalue);
                }
                break;
            case FloatConst:
                if((expr->v).val.fvalue >= 0.0){
                    fprintf(target, "%f\n", (expr->v).val.fvalue);
                }
                else{
                    fprintf(target, "_%f\n", -(expr->v).val.fvalue);
                }
                break;
            default:
                fprintf(target,"Error In fprint_left_expr. (expr->v).type=%d\n",(expr->v).type);
                break;
        }
    }
    else{
        fprint_expr(target, expr->leftOperand, table);
        if(expr->rightOperand == NULL){
            fprintf(target,"5k\n");
        }
        else{
            //	fprint_right_expr(expr->rightOperand);
            fprint_expr(target, expr->rightOperand, table);
            fprint_op(target, (expr->v).type);
        }
    }
}

char assignRegister(SymbolTable *table, char *c){
    for(int i = 0; i < table->numberOfSymbol; i++){
        if(strcmp(c, table->id[i]) == 0){
            return 'a' + i;
        }
    }
    printf("Code Generation Error: identifier %s is not declared\n", c);
    exit(1);
}

void gencode(Program prog, FILE * target, SymbolTable *table)
{
    Statements *stmts = prog.statements;
    Statement stmt;

    while(stmts != NULL){
        stmt = stmts->first;
        switch(stmt.type){
            case Print:
                fprintf(target,"l%c\n", assignRegister(table, stmt.stmt.variable));
                fprintf(target,"p\n");
                break;
            case Assignment:
                fprint_expr(target, stmt.stmt.assign.expr, table);
                /*
                   if(stmt.stmt.assign.type == Int){
                   fprintf(target,"0 k\n");
                   }
                   else if(stmt.stmt.assign.type == Float){
                   fprintf(target,"5 k\n");
                   }*/
                fprintf(target,"s%c\n", assignRegister(table, stmt.stmt.variable));
                fprintf(target,"0 k\n");
                break;
        }
        stmts=stmts->rest;
    }

}


/***************************************
  For our debug,
  you can omit them.
 ****************************************/
void print_expr(Expression *expr)
{
    if(expr == NULL)
        return;
    else{
        print_expr(expr->leftOperand);
        switch((expr->v).type){
            case Identifier:
                printf("%s ", (expr->v).val.id);
                break;
            case IntConst:
                printf("%d ", (expr->v).val.ivalue);
                break;
            case FloatConst:
                printf("%f ", (expr->v).val.fvalue);
                break;
            case PlusNode:
                printf("+ ");
                break;
            case MinusNode:
                printf("- ");
                break;
            case MulNode:
                printf("* ");
                break;
            case DivNode:
                printf("/ ");
                break;
            case IntToFloatConvertNode:
                printf("(float) ");
                break;
            default:
                printf("error ");
                break;
        }
        print_expr(expr->rightOperand);
    }
}

void test_parser( FILE *source )
{
    Declarations *decls;
    Statements *stmts;
    Declaration decl;
    Statement stmt;
    Program program = parser(source);

    decls = program.declarations;

    while(decls != NULL){
        decl = decls->first;
        if(decl.type == Int)
            printf("i ");
        if(decl.type == Float)
            printf("f ");
        printf("%s ",decl.name);
        decls = decls->rest;
    }

    stmts = program.statements;

    while(stmts != NULL){
        stmt = stmts->first;
        if(stmt.type == Print){
            printf("p %s ", stmt.stmt.variable);
        }

        if(stmt.type == Assignment){
            printf("%s = ", stmt.stmt.assign.id);
            print_expr(stmt.stmt.assign.expr);
        }
        stmts = stmts->rest;
    }
}

