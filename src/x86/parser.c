/*  Copyright (C) 2011 Xander Vedejas <xvedejas@gmail.com>
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
#include <vm.h>

const Size maxKeywordCount = 8;

/*
 * This is a tail-recursive parser, which means it is designed to work with
 * left-recursive grammars. It uses less stack space than a recursive descent
 * parser, and is fairly easy to write.
 */

const String bytecodes[] =
{
    "NULL",
    "integer",
    "double",
    "string",
    "char",
    "symbol",
    "array",
    "block",
    "variable",
    "message",
    "stop",
    "set",
    "end",
    "init",
};

/*
 * Data literals:
 *     Integer         123
 *     Double          12.34
 *     String          "abcd"
 *     Character       'a'
 *     Symbol          #ab:cd:
 *     Array           [1, 2]
 *     Block           { x, y | z, w. x + y }
 * */

u8 *compile(String source)
{
    Token *curToken = NULL;
    jmp_buf exit;
    inline void nextToken()
    {
        curToken = lex(source, curToken);
        source += curToken->length;
    }
    StringBuilder *output = stringBuilderNew(NULL);
    nextToken();
    /* Per file, when loaded is compared with global method table */
    InternTable *symbolTable = internTableNew();
    /* The method table must be saved */
    StringBuilder *symbolTableBytecode = stringBuilderNew(" ");
    /* The very first byte tells the size of the symbol table. For now the
     * limit is 255 but it may very well change. that byte is reserved by
     * starting with a single character in the string builder. */
    
    void parserRequire(bool condition, String error)
    {
        if (unlikely(!condition))
        {
            if (curToken->type == EOFToken)
            {
                longjmp(exit, true);
            }
            printf("\n [[ Parser Error ]]\n");
            printf("Token: %s Data: %s Line: %i Col: %i\n",
                tokenTypeNames[curToken->type], curToken->data, curToken->line, curToken->col);
            printf("%s\n", error);
        }
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
    
    /* The following function takes an array of keywords and finds the
     * corresponding message keyword's interned value. Does not work for unary
     * messages. */
    Size methodNameIntern(Size keywordCount, String *keywords)
    {
        Size i = keywordCount,
             j = 0,
             length = 0;
        for (j = 0; j < i; j++)
            length += strlen(keywords[j]) + 1;
        String messageName = calloc(length + 1, sizeof(char));
        for (j = 0; j < i; j++)
        {
            strcat(messageName, keywords[j]);
            strcat(messageName, ":"); 
        }
        Size interned = intern(messageName);
        free(messageName);
        return interned;
    }
    
    Token *lookahead(u32 n)
    {
        Token *token = curToken;
        String currentPos = source;
        Token *lookahead = NULL;
        while (n--)
        {
            curToken = lex(source, curToken);
            source += curToken->length;
        }
        lookahead = curToken;
        curToken = token;
        source = currentPos;
        return lookahead;
    }
    
    inline void outStr(String val)
    {
        stringBuilderAppendN(output, val, strlen(val) + 1);
    }
    
    inline void outByte(char val)
    {
        stringBuilderAppendChar(output, val);
    }
    
    inline Size getPos()
    {
        return output->size;
    }
    
    inline void setPos(Size pos, u8 value)
    {
        output->s[pos] = value;
    }
    
    inline bool startsValue(TokenType type)
    {
        return type == integerToken   || type == doubleToken      ||
               type == stringToken    || type == charToken        ||
               type == symbolToken    || type == openBracketToken ||
               type == openParenToken || type == openBraceToken   ||
               type == keywordToken;
    }
    
    auto void parseValue();
    auto void parseExpr();
    
    /* Statements can be any string of expressions separated by '.' */
    void parseStmt()
    {
        while (startsValue(curToken->type))
        {
            parseExpr();
            if (curToken->type == stopToken)
            {
                outByte(stopBC);
                nextToken();
            }
            else
                break;
        }
    }
    
    void parseObjectDef()
    {
        outByte(initBC);
        Size varCountByte = getPos();
        Size varCount = 0;
        outByte('\0'); // number of variables
        parserRequire(curToken->type == openBraceToken, "Expected '{' token");
        nextToken();
        /* variables, listed as | var, var | */
        if (curToken->type == pipeToken)
        {
            nextToken();
            parserRequire(curToken->type == keywordToken, "Expected variable name");
            varCount++;
            outByte(intern(curToken->data));
            nextToken();
            while (curToken->type == commaToken)
            {
                nextToken();
                parserRequire(curToken->type == keywordToken, "Expected variable name");
                varCount++;
                outByte(intern(curToken->data));
                nextToken();
            }
            parserRequire(curToken->type == pipeToken, "Expected '|' token");
            nextToken();
        }
        setPos(varCountByte, varCount);
        /* Method Declarations */
        Size methodCountByte = getPos();
        Size methodCount = 0;
        outByte('\0'); // number of methods
        while (curToken->type != closeBraceToken)
        {
            methodCount++;
            parserRequire(curToken->type == keywordToken, "Expected method declaration");
            String keywords[maxKeywordCount];
            String args[maxKeywordCount + 1];
            Size keywordc = 0,
                 argc = 0;
            args[argc++] = "self";
            
            keywords[keywordc++] = curToken->data;
            nextToken();
            if (curToken->type == colonToken) // not unary
            {
                nextToken();
                args[argc++] = curToken->data;
                nextToken();
                while (curToken->type != openBraceToken)
                {
                    parserRequire(curToken->type == keywordToken, "Expected method keyword");
                    parserRequire(keywordc < maxKeywordCount, "Message has too many keywords");
                    keywords[keywordc++] = curToken->data;
                    nextToken();
                    parserRequire(curToken->type == colonToken, "Expected ':' token");
                    nextToken();
                    args[argc++] = curToken->data;
                    nextToken();
                }
                outByte(methodNameIntern(keywordc, keywords));
            }
            else
                outByte(intern(keywords[0]));
            
            /* Method closure */
            outByte(blockBC);
            parserRequire(curToken->type == openBraceToken, "Expected '{' token");
            nextToken();
            Size varCountByte = getPos();
            outByte('\0');
            outByte(argc);
            Size i;
            for (i = 0; i < argc; i++)
                outByte(intern(args[i]));
            
            Size varc = 0;
            if (curToken->type == pipeToken) // variables
            {
                do
                {
                    nextToken();
                    parserRequire(curToken->type == keywordToken, "Expected variable name");
                    varc++;
                    outByte(intern(curToken->data));
                    nextToken();
                } while (curToken->type == commaToken);
                parserRequire(curToken->type == pipeToken, "Expected '|' token");
            }
            setPos(varCountByte, (u8)(varc + argc));
            parseStmt();
            parserRequire(curToken->type == closeBraceToken, "Expected '}' token");
            nextToken();
            outByte(endBC);
        }
        nextToken();
        setPos(methodCountByte, methodCount);
    }
    
    /* Value plus messages */
    void parseExpr()
    {
        parseValue();
        while (curToken->type == initToken || curToken->type == keywordToken ||
            curToken->type == specialCharToken)
        {
            if (curToken->type == keywordToken)
            {
                String keywords[maxKeywordCount];
                Size i = 0;
                bool isUnary = false;
                while (curToken->type == keywordToken)
                {
                    parserRequire(i < maxKeywordCount, "More keywords than allowed in one message");
                    keywords[i++] = curToken->data;
                    nextToken();
                    if (curToken->type != colonToken) // unary message
                    {
                        parserRequire(i == 1, "Expected ':' token. Use parenthesis around binary messages.");
                        outByte(symbolBC);
                        outByte(intern(keywords[0]));
                        outByte(messageBC);
                        outByte(0); // argument count
                        isUnary = true;
                        break;
                    }
                    nextToken();
                    parseValue();
                }
                if (isUnary)
                    continue;
                outByte(symbolBC);
                outByte(methodNameIntern(i, keywords));
                outByte(messageBC);
                outByte(i);
            }
            else if (curToken->type == initToken)
            {
                nextToken();
                parseObjectDef();
            }
            else
            {
                Size binaryMessage = intern(curToken->data);
                nextToken();
                parseValue();
                outByte(symbolBC);
                outByte(binaryMessage);
                outByte(messageBC);
                outByte(1); // number of arguments
            }
        }
    }
    
    /*
        A block header is one of the four following formats:
            { a, b, c | x, y, z | ... }
            { | x, y, z | ... }
            { a, b, c | ... }
            { ... }
        
        The bytecode output is as follows, byte-by-byte
        
            n = number of local variables, including arguments
            m = number of arguments, where first m varibles are arguments.
                index of variable 0
                index of variable 1
                  &c
                index of variable n  
    */
    
    void parseBlockHeader()
    {
        Size varCountByte = getPos();
        outByte('\0');
        Size argCountByte = getPos();
        outByte('\0');
        Size arguments = 0;
        /* Argument List */
        if (lookahead(1)->type == pipeToken || lookahead(1)->type == commaToken)
        {
            parserRequire(curToken->type == keywordToken, "Expected keyword token");
            outByte(intern(curToken->data));
            arguments++;
            nextToken();
            while (curToken->type != pipeToken)
            {
                parserRequire(curToken->type == commaToken, "Expected ',' token");
                nextToken();
                parserRequire(curToken->type == keywordToken, "Expected keyword token");
                outByte(intern(curToken->data));
                arguments++;
                nextToken();
            }
        }
        setPos(argCountByte, arguments);
        Size variables = 0;
        if (curToken->type == pipeToken)
        {
            nextToken();
            /* Variable list */
            if (lookahead(1)->type == pipeToken || lookahead(1)->type == commaToken)
            {
                parserRequire(curToken->type == keywordToken, "Expected keyword token");
                outByte(intern(curToken->data));
                variables++;
                nextToken();
                while (curToken->type != pipeToken)
                {
                    parserRequire(curToken->type == commaToken, "Expected ',' token");
                    nextToken();
                    parserRequire(curToken->type == keywordToken, "Expected keyword token");
                    outByte(intern(curToken->data));
                    variables++;
                    nextToken();
                }
                parserRequire(curToken->type == pipeToken, "Expected '|' token");
                nextToken();
            }
        }
        setPos(varCountByte, (u8)(variables + arguments));
    }
    
    void parseValue()
    {
        switch (curToken->type)
        {
            case integerToken:
            {
                outByte(integerBC);
                outStr(curToken->data);
                nextToken();
            } break;
            case doubleToken:
            {
                outByte(doubleBC);
                outStr(curToken->data);
                nextToken();
            } break;
            case stringToken:
            {
                outByte(stringBC);
                outStr(curToken->data);
                nextToken();
            } break;
            case charToken:
            {
                outByte(charBC);
                outByte((char)(Size)curToken->data);
                nextToken();
            } break;
            case symbolToken:
            {
                outByte(symbolBC);
                outByte(intern(curToken->data));
                nextToken();
            } break;
            case openBracketToken: // array
            {
                Size elements = 0;
                nextToken();
                if (curToken->type != closeBracketToken) while (true)
                {
                    parseStmt();
                    elements++;
                    if (curToken->type == closeBracketToken)
                        break;
                    parserRequire(curToken->type == commaToken,
                        "Expected ',' token");
                    nextToken();
                }
                nextToken();
                outByte(arrayBC);
                outByte(elements);
            } break;
            case openBraceToken: // block
            {
                nextToken();
                outByte(blockBC);
                parseBlockHeader();
                parseStmt();
                parserRequire(curToken->type == closeBraceToken, "Expected '}' token");
                nextToken();
                outByte(endBC);
            } break;
            case openParenToken:
            {
                nextToken();
                parseStmt();
                parserRequire(curToken->type == closeParenToken, "Expected ')' token");
                nextToken();
            } break;
            case keywordToken:
            {
                if (unlikely(lookahead(1)->type == eqToken))
                {
                    Size keyword = intern(curToken->data);
                    nextToken();
                    parserRequire(curToken->type == eqToken, "Expected '=' token");
                    nextToken();
                    parseExpr();
                    outByte(setBC);
                    outByte(keyword);
                }
                else
                {
                    outByte(variableBC);
                    outByte(intern(curToken->data));
                    nextToken();
                }
            } break;
            default:
            {
                parserRequire(false, "Parser Error");
            } break;
        }
    }
    
    if (setjmp(exit))
    {
        // error or incomplete. scrap bytecode and return null.
        Token *token = curToken;
        do
        {
            tokenDel(token);
        } while ((token = token->previous) != NULL);
        stringBuilderDel(output);
        stringBuilderDel(symbolTableBytecode);
        internTableDel(symbolTable);
        return NULL;
    }
    parseBlockHeader();
    parseStmt();
    outByte(0x04); // EOF
    
    Token *token = curToken;
    do
    {
        tokenDel(token);
    } while ((token = token->previous) != NULL);
    
    /* We want our method table at the beginning of the bytecode */
    Size outputSize = output->size;
    String bodyCode = stringBuilderToString(output);
    stringBuilderAppendN(symbolTableBytecode, bodyCode, outputSize);
    free(bodyCode);
    outputSize = symbolTableBytecode->size;
    u8 *finalBytecode = (u8*)stringBuilderToString(symbolTableBytecode);
    
    finalBytecode[0] = (u8)symbolTable->count; // symbol count
    internTableDel(symbolTable);
    
    // see the output
    //Size i;
    //for (i = 0; i < outputSize; i++)
    //    printf("%x %c\n", finalBytecode[i], finalBytecode[i]);
    //printf("Parser done\n");
    
    //printf("Bytecode size %i\n", outputSize);
    return finalBytecode;
}

