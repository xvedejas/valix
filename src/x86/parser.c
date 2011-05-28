/*  Copyright (C) 2010 Xander Vedejas <xvedejas@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <main.h>
#include <parser.h>
#include <lexer.h>
#include <threading.h>
#include <mm.h>
#include <string.h>
#include <types.h>
#include <data.h>

/*
 * This is a tail-recursive parser, which means it is designed to work with
 * left-recursive grammars. It uses less stack space than a recursive descent
 * parser, and is fairly easy to write. The following grammar is the current
 * desired subset of the final grammar.
 *  
list  :: '[' value? (',' value)* ']'

block :: '{' (Keyword (',' Keyword)* ':')? stmt* '}'

value :: String
      :: Num
      :: Keyword
      :: list
      :: block
      :: '(' expr ')'

expr  :: value (Keyword | SpecialCharacter value | (Keyword ':' value)+)

stmt  :: expr '.'
      :: keyword '=' expr '.'
*/

/* Bytecode format:
 * 
 * [next in bytecode] (pop from stack)
 * {single byte (set size)}
 * 
 * byte meaning
 *  80  import                [name of module] {\0}
 *  81  push string           [stringval] {\0}
 *  82  push number
 *  83  variable              {index}
 *  84  call method           [methodsymbol] [argc] (args)*
 *  85  begin list            {num entries}
 *  86  begin procedure       {num vars}
 *  87  end list
 *  88  end procedure
 *  89  end statement
 *  8A  return, end procedure (arg)
 *  8B  set equal             [varsymbol]
 *  8C  push symbol
 *  8D  [available]
 *  8E  set var to block arg  [varsymbol]
 *  
 */

/* Note that most function are defined as nested functions, that is they are
 * declared entirely within the scope of parse(). This allows us to use
 * variables in parse() that are either on the stack or maybe in registers
 * between all the functions. This means no lock is needed and no values must
 * be passed as parameters to use those variables in a thread-safe way. */
u8 *parse(Token *first)
{
    StringBuilder *output = stringBuilderNew(NULL);
    Token *curToken;
    Stack *varCounts = stackNew();
    Size currentVarCount = 0;
    /* Per file, when loaded is compared with global method table */
    InternTable *symbolTable = internTableNew();
    /* The method table must be saved */
    StringBuilder *symbolTableBytecode = stringBuilderNew(" ");
    /* The very first byte tells the size of the method table. For now the
     * limit is 255 but it may very well change. that byte is reserved by
     * starting with a single character in the string builder. */
    
    auto void parseStmt();
    auto void parseExpr();
    auto void parseValue();
    auto void parseProcedure();
    
    void parserRequire(bool condition, String error)
    {
        if (unlikely(!condition))
        {
            printf("\n [[ Parser Error ]]\n");
            printf("Token: %s Data: %s Line: %i Col: %i\n",
                tokenTypeNames[curToken->type], curToken->data, curToken->line, curToken->col);
            printf(error);
            endThread();
        }
    }
    
    void out(String val, Size len)
    {
        stringBuilderAppendN(output, val, len);
    }
    
    void outchr(char val)
    {
        stringBuilderAppendChar(output, val);
    }
    
    void nextToken()
    {
        curToken = curToken->next;
    }
    
    Size intern(String string)
    {
        bool alreadyInTable = isStringInterned(symbolTable, string);
        Size index = internString(symbolTable, string);
        if (!alreadyInTable)
        {
            stringBuilderAppend(symbolTableBytecode, string);
            stringBuilderAppendChar(symbolTableBytecode, '\0');
        }
        return index;
    }
    
    Token *lookahead(u32 n)
    {
        Token *token = curToken;
        while (n--) token = curToken->next;
        return token;
    }
    
    void parseProcedure()
    {
        parseStmt();
        while (curToken->type == stopToken)
        {
            nextToken();
            if (curToken->type == EOFToken)
                return;
            parseStmt();
        }
    }
    
    void parseStmt()
    {
        if (unlikely(curToken->type == returnToken))
        {
            nextToken();
            parseExpr();
            outchr(returnByteCode); /* Return directive */
        }
        else if (unlikely(curToken->type == importToken))
        {
            outchr(importByteCode); /* Import directive */
            nextToken();
            parserRequire(curToken->type == keywordToken, "Expected module name.");
            out(curToken->data, strlen(curToken->data));
            outchr('\0');
            nextToken();
        }
        else if (unlikely(lookahead(1)->type == eqToken))
        {
            parserRequire(curToken->type == keywordToken, "Expected keyword.");
            String lefthandKeyword = curToken->data;
            nextToken();
            parserRequire(curToken->type == eqToken, "Expected '=' token.");
            nextToken();
            parseExpr();
            outchr(setEqualByteCode); /* Equal directive */
            if (!isStringInterned(symbolTable, lefthandKeyword))
                currentVarCount++;
            outchr(intern(lefthandKeyword));
        }
        else
            parseExpr();
        outchr(endStatementByteCode); /* End Statement directive */
    }
    
    void parseMsg()
    {
        String keywords[8];
        u32 i = 0;
        parserRequire(curToken->type == keywordToken, "Expected keyword.");
        while (curToken->type == keywordToken)
        {
            parserRequire(i < 8, "More than eight keywords in one message");
            keywords[i++] = curToken->data;
            nextToken();
            if (curToken->type != colonToken) /* Unary message */
            {
                parserRequire(i == 1, "Expected keyword. Maybe use parentheses to show you want to use a binary/unary message?");
                outchr(callMethodByteCode); /* Message directive */
                outchr(intern(keywords[0]));
                outchr((char)0); // argc
                return;
            }
            nextToken();
            parseValue();
        }
        u32 j;
        outchr(callMethodByteCode); /* Message directive */
        Size length = 0;
        for (j = 0; j < i; j++)
            length += strlen(keywords[j]) + 1;
        String methodName = calloc(length + 1, sizeof(char));        
        for (j = 0; j < i; j++)
        {
            strcat(methodName, keywords[j]);
            strcat(methodName, ":");
        }
        outchr(intern(methodName));
        outchr((char)i); // argc
        return;
    }
    
    void parseExpr()
    {
        parseValue();
        while (curToken->type == specialCharToken || curToken->type == keywordToken)
        {
            if (curToken->type == keywordToken)
                parseMsg();
            else /* specialCharToken, binary message */
            {
                String data = curToken->data;
                nextToken();
                parseValue();
                outchr(callMethodByteCode); /* (binary) message directive */
                outchr((u8)intern(data)); /* method name */
                outchr((char)1); // argc
            }
        }
    }
    
    void parseValue()
    {
        switch (curToken->type)
        {
            case stringToken:
            {
                outchr(newStringByteCode); /* String directive */
                out(curToken->data, strlen(curToken->data));
                outchr('\0');
                nextToken();
                return;
            } break;
            case numToken:
            {
                outchr(newNumberByteCode); /* Number directive */
                out(curToken->data, strlen(curToken->data));
                outchr('\0');
                nextToken();
                return;
            } break;
            case symbolToken:
            {
                outchr(newSymbolByteCode); /* Symbol directive */
                outchr((u8)intern(curToken->data));
                nextToken();
                return;
            } break;
            case keywordToken:
            {
                outchr(variableByteCode); /* Keyword directive */
                if (!isStringInterned(symbolTable, curToken->data))
                    currentVarCount++;
                outchr((char)intern(curToken->data));
                nextToken();
                return;
            } break;
            case openBracketToken: /* '[' denotes a list  */
            {
                outchr(beginListByteCode); /* Begin list directive */
                nextToken();
                while (curToken->type == commaToken)
                {
                    nextToken();
                    parserRequire(curToken->type != EOFToken, "Unexpected EOF Token");
                    parseValue();
                }
                parserRequire(curToken->type == closeBracketToken, "Expected ']' token");
                nextToken();
                outchr(endListByteCode); /* End list directive */
                return;
            } break;
            case openBraceToken: /* '{' denotes a block */
            {
                /// todo:
                /// 8E opcode to load each argument
                outchr(beginProcedureByteCode); /* Begin block directive */
                nextToken();
                
                stackPush(varCounts, (void*)currentVarCount);
                currentVarCount = 0;
                /* Let's remember where the procedure began so that we can say here
                 * how many variables have been defined for this procedure */
                Size i = output->size;
                outchr('\0');
                
                if (lookahead(1)->type == commaToken || lookahead(1)->type == colonToken)
                {
                    // If we have arguments...
                    parserRequire(curToken->type == keywordToken, "Expected keyword token");
                    outchr((char)intern(curToken->data));
                    outchr('\0');
                    nextToken();
                    while (curToken->type != colonToken)
                    {
                        parserRequire(curToken->type == commaToken, "Expected ',' token");
                        nextToken();
                        parserRequire(curToken->type == keywordToken, "Expected keyword token");
                        out(curToken->data, strlen(curToken->data));
                        outchr('\0');
                        nextToken();
                    }
                }
                outchr(beginProcedureByteCode); /* Second begin block directive: means argument list done */
                outchr('\0'); // number of expected local vars
                
                parseProcedure();
                
                parserRequire(curToken->type == closeBraceToken, "Expected '}' token");
                nextToken();
                
                // say how many variables
                output->s[i] = (char)currentVarCount;
                Size previous = (Size)stackPop(varCounts);
                stackPush(varCounts, (void*)currentVarCount);
                currentVarCount = previous;
                
                outchr(endProcedureByteCode); /* End block directive */
                return;
            } break;
            case openParenToken:
            {
                nextToken();
                parseExpr();
                parserRequire(curToken->type == closeParenToken, "Expected ')' token");
                nextToken();
                return;
            } break;
            case EOFToken:
            {
                parserRequire(false, "Unexpected EOF Token");
            } break;
            default:
            break;
        }
        parserRequire(false, "Expected expression");
        
        return;
    }
    curToken = first;
    currentVarCount = 0;
    outchr('\x86');
    /* Let's remember where the procedure began so that we can say here
     * how many variables have been defined for this procedure */
    Size i = output->size;
    outchr('\0');
    
    parseProcedure();
    
    // say how many variables
    output->s[i] = (char)currentVarCount;
    stackDel(varCounts);
    
    outchr('\xFF'); // EOS
    Token *next;
    do
    {
        next = first->next;
        free(first);
        if (first->data != NULL)
            free(first->data);
    } while ((first = next) != NULL);
    
    /* We want our method table at the beginning of the bytecode */
    Size outputSize = output->size;
    String bodyCode = stringBuilderToString(output);
    stringBuilderAppendN(symbolTableBytecode, bodyCode, outputSize);
    free(bodyCode);
    outputSize = symbolTableBytecode->size;
    u8 *finalBytecode = (u8*)stringBuilderToString(symbolTableBytecode);
    
    finalBytecode[0] = (u8)symbolTable->count; // method count
    internTableDel(symbolTable);
    
    // see the output
    //for (i = 0; i < outputSize; i++)
    //    printf("%x %c\n", finalBytecode[i], finalBytecode[i]);
    //printf("Parser done\n");
    
    return (u8*)finalBytecode;
}
