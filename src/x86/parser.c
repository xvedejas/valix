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

const Size maxKeywordCount = 8;

/*
 * This is a tail-recursive parser, which means it is designed to work with
 * left-recursive grammars. It uses less stack space than a recursive descent
 * parser, and is fairly easy to write. The following grammar is the current
 * desired subset of the final grammar.
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
    "return",
    "end",
};

/*
 * Data literals:
 *     Integer         123
 *     Double          12.34
 *     String          "abcd"
 *     Character       'a'
 *     Symbol          #ab:cd:
 *     Array           [1, 2]
 *     Block           { x, y | x + y }
 * */

u8 *compile(String source)
{
    Token *curToken = NULL;
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
    /* The very first byte tells the size of the method table. For now the
     * limit is 255 but it may very well change. that byte is reserved by
     * starting with a single character in the string builder. */
    
    void parserRequire(bool condition, String error)
    {
        if (unlikely(!condition))
        {
            printf("\n [[ Parser Error ]]\n");
            printf("Token: %s Data: %s Line: %i Col: %i\n",
                tokenTypeNames[curToken->type], curToken->data, curToken->line, curToken->col);
            printf("%s\n", error);
            endThread();
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
    
    inline void setPos(Size pos, char value)
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
    
    /* Value plus messages */
    void parseExpr()
    {
        parseValue();
        while (curToken->type == keywordToken || curToken->type == specialCharToken)
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
                    break;
                Size j, length = 0;
                for (j = 0; j < i; j++)
                    length += strlen(keywords[j]) + 1;
                String messageName = calloc(length + 1, sizeof(char));
                for (j = 0; j < i; j++)
                {
                    strcat(messageName, keywords[j]);
                    strcat(messageName, ":"); 
                }
                outByte(symbolBC);
                outByte(intern(messageName));
                outByte(messageBC);
                outByte(i);
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
                outStr(curToken->data);
                nextToken();
            } break;
            case openBracketToken: // array
            {
                Size elements = 0;
                do
                {
                    nextToken();
                    parseStmt();
                    elements++;
                } while (curToken->type == commaToken);
                parserRequire(curToken->type == closeBracketToken, "Expected ']' token");
                nextToken();
                outByte(arrayBC);
                outByte(elements);
            } break;
            case openBraceToken: // block
            {
                nextToken();
                outByte(blockBC);
                Size argCountByte = getPos();
                outByte('\0');
                Size arguments = 0;
                if (lookahead(1)->type == pipeToken || lookahead(1)->type == commaToken)
                {
                    parserRequire(curToken->type == keywordToken, "Expected keyword token");
                    outByte(setBC);
                    outByte(intern(curToken->data));
                    arguments++;
                    nextToken();
                    while (curToken->type != pipeToken)
                    {
                        parserRequire(curToken->type == commaToken, "Expected ',' token");
                        nextToken();
                        parserRequire(curToken->type == keywordToken, "Expected keyword token");
                        outByte(setBC);
                        outByte(intern(curToken->data));
                        arguments++;
                        nextToken();
                    }
                    parserRequire(curToken->type == pipeToken, "Expected '|' token");
                    nextToken();
                }
                setPos(argCountByte, arguments);
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
    parseStmt();
    outByte(endBC); // EOS
    
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
    //    printf("%x %c    %s\n", finalBytecode[i], finalBytecode[i], bytecodes[finalBytecode[i]]);
    //printf("Parser done\n");
    
    //printf("Bytecode size %i\n", outputSize);
    return (u8*)finalBytecode;
}

