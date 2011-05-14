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
 *  84  call method           [method name] {\0} (args)*
 *  85  begin list            {num entries}
 *  86  begin procedure       {num vars}
 *  87  end list
 *  89  end statement
 *  8A  return, end procedure (arg)
 *  8B  set equal             [varname] {\0}
 *  8C  binary message        [method name] {\0} (args)*
 *  8D  internal function     [function name] {\0} (args*)
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
    Stack *varTables = stackNew();
    InternTable *currentVarTable; /* Per scope */
    /* Per file, when loaded is compared with global method table */
    InternTable *methodTable = internTableNew();
    /* The method table must be saved */
    StringBuilder *methodTableBytecode = stringBuilderNew(" ");
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
            outchr('\x8A'); /* Return directive */
        }
        else if (unlikely(curToken->type == importToken))
        {
            outchr('\x80'); /* Import directive */
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
            outchr('\x8B'); /* Equal directive */
            outchr(internString(currentVarTable, lefthandKeyword));
        }
        else
            parseExpr();
        outchr('\x89'); /* End Statement directive */
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
                parserRequire(i == 1, "Expected keyword, not binary message. Maybe use parentheses to show you want to use a binary message?");
                out(keywords[0], strlen(keywords[0]));
                out('\0', 1);
                return;
            }
            nextToken();
            parseValue();
        }
        u32 j;
        outchr('\x84'); /* Message directive */
        Size length = 0;
        for (j = 0; j < i; j++)
            length += strlen(keywords[j]) + 1;
        String methodName = calloc(length + 1, sizeof(char));        
        for (j = 0; j < i; j++)
        {
            strcat(methodName, keywords[j]);
            strcat(methodName, ":");
        }
        
        bool alreadyInTable = isStringInterned(methodTable, methodName);
        Size index = internString(methodTable, methodName);
        if (!alreadyInTable)
        {
            stringBuilderAppend(methodTableBytecode, methodName);
            stringBuilderAppendChar(methodTableBytecode, '\0');
        }
        outchr((u8)index);
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
                outchr('\x8C'); /* binary message directive */
                bool alreadyInTable = isStringInterned(methodTable, data);
                Size index = internString(methodTable, data);
                if (!alreadyInTable)
                {
                    stringBuilderAppend(methodTableBytecode, data);
                    stringBuilderAppendChar(methodTableBytecode, '\0');
                }
                outchr((u8)index);
            }
        }
    }
    
    void parseValue()
    {
        switch (curToken->type)
        {
            case stringToken:
            {
                outchr('\x81'); /* String directive */
                out(curToken->data, strlen(curToken->data));
                outchr('\0');
                nextToken();
                return;
            } break;
            case numToken:
            {
                outchr('\x82'); /* Number directive */
                out(curToken->data, strlen(curToken->data));
                outchr('\0');
                nextToken();
                return;
            } break;
            case keywordToken:
            {
                outchr('\x83'); /* Keyword directive */
                outchr((char)internString(currentVarTable, curToken->data));
                nextToken();
                return;
            } break;
            case openBracketToken: /* '[' denotes a list  */
            {
                outchr('\x85'); /* Begin list directive */
                nextToken();
                while (curToken->type == commaToken)
                {
                    nextToken();
                    parserRequire(curToken->type != EOFToken, "Unexpected EOF Token");
                    parseValue();
                }
                parserRequire(curToken->type == closeBracketToken, "Expected ']' token");
                nextToken();
                outchr('\x87'); /* End list directive */
                return;
            } break;
            case openBraceToken: /* '{' denotes a block */
            {
                outchr('\x86'); /* Begin block directive */
                nextToken();
                
                stackPush(varTables, currentVarTable);
                currentVarTable = internTableNew();
                /* Let's remember where the procedure began so that we can say here
                 * how many variables have been defined for this procedure */
                Size i = output->size;
                outchr('\0');
                
                if (lookahead(1)->type == commaToken || lookahead(1)->type == colonToken)
                {
                    // If we have arguments...
                    parserRequire(curToken->type == keywordToken, "Expected keyword token");
                    outchr((char)internString(currentVarTable, curToken->data));
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
                outchr('\x86'); /* Second begin block directive: means argument list done */
                nextToken();
                parseProcedure();
                parserRequire(curToken->type == closeBraceToken, "Expected ',' token");
                nextToken();
                
                // say how many variables
                output->s[i] = (char)currentVarTable->count;
                InternTable *previous = stackPop(varTables);
                stackPush(varTables, currentVarTable);
                internTableDel(currentVarTable);
                currentVarTable = previous;
                
                outchr('\x88'); /* End block directive */
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
    currentVarTable = internTableNew();
    outchr('\x86');
    /* Let's remember where the procedure began so that we can say here
     * how many variables have been defined for this procedure */
    Size i = output->size;
    outchr('\0');
    
    parseProcedure();
    
    // say how many variables
    output->s[i] = (char)currentVarTable->count;
    internTableDel(currentVarTable);
    stackDel(varTables);
    
    outchr('\xFF'); // EOS
    Token *next;
    do
    {
        next = first->next;
        free(first);
        if (first->data != NULL)
            free(first->data);
    } while ((first = next) != NULL);
    
    sleep(1000);
    
    /* We want our method table at the beginning of the bytecode */
    Size outputSize = output->size;
    String bodyCode = stringBuilderToString(output);
    stringBuilderAppendN(methodTableBytecode, bodyCode, outputSize);
    free(bodyCode);
    outputSize = methodTableBytecode->size;
    u8 *finalBytecode = (u8*)stringBuilderToString(methodTableBytecode);
    
    finalBytecode[0] = (u8)methodTable->count;
    internTableDel(methodTable);
    
    /* // see the output
    for (i = 0; i < outputSize; i++)
        printf("%x %c\n", finalBytecode[i], finalBytecode[i]);
        */
    
    return (u8*)finalBytecode;
}
