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
#include <cstring.h>
#include <types.h>
#include <data.h>
#include <vm.h>

const Size EOF = 0x04;

const Size maxKeywordCount = 8;

/*
 * This is a tail-recursive parser, which means it is designed to work with
 * left-recursive grammars. It uses less stack space than a recursive descent
 * parser, and is fairly easy to write.
 * 
 * 
 * stmt ::= expr ('.' expr)* '.'?
 * expr ::= value (Keyword (':' value (Keyword ':' value)*)? | Special value)
 * blockHeader ::=
 *     ((Keyword (',' Keyword)* '|') |
 *     ('|' Keyword (',' Keyword)* '|') |
 *     (Keyword (',' Keyword)* '|' Keyword (',' Keyword)* '|'))?
 * methodBlockHeader ::= ('|' Keyword (',' Keyword)* '|')?
 * methodList ::=
 *     (Keyword (':' value (Keyword ':' value)*)? | Special value) methodBlock
 * block ::= '{' blockHeader stmt '}'
 * methodBlock ::= '{' methodBlockHeader stmt '}'
 * objDef ::= '@{' stmt (',' stmt)* '|'
 *     (Keyword (',' Keyword)* '|')? methodList '}'
 * traitDef ::= '#{' methodList '}'
 * value ::= array | block | Integer | Double | String | Char | objDef | traitDef
 * array ::= '[' (stmt (',' stmt)*)? ']'
 * 
 * top ::= blockHeader stmt
 * 
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
    "object",
    "trait"
};

typedef struct parseStructure
{
    StringBuilder *sb;
    struct parseStructure *next;
} ParseStructure;

ParseStructure *parseStructureNew()
{
    ParseStructure *ps = malloc(sizeof(ParseStructure));
    ps->sb = stringBuilderNew(NULL);
    ps->next = NULL;
    return ps;
}

ParseStructure *parseStructurePush(ParseStructure *root)
{
    while (root->next != NULL) root = root->next;
    return (root->next = parseStructureNew());
}

ParseStructure *parseStructureCommit(ParseStructure *root)
{
    ParseStructure *node = root;
    assert(node->next != NULL, "parser error");
    ParseStructure *previous;
    do
    {
        previous = node;
        node = root->next;
    } while (node->next != NULL);
    stringBuilderAppendN(previous->sb, stringBuilderToString(node->sb),
        node->sb->size);
    previous->next = node->next;
    free(node);
    return previous;
}

StringBuilder *parseStructureCollapse(ParseStructure *root)
{
    ParseStructure *node, *next;
    for (node = root->next; node != NULL; node = next)
    {
        next = node->next;
        stringBuilderAppendN(root->sb, stringBuilderToString(node->sb),
            node->sb->size);
        free(node);
    }
    StringBuilder *result = root->sb;
    free(root);
    return result;
}

void parseStructureDebug(ParseStructure *root)
{
    printf("SB Contents:\n");
    do
    {
        stringBuilderPrint(root->sb);
        printf("\n");
    }
    while ((root = root->next) != NULL);
}

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
    ParseStructure *root = parseStructureNew();
    InternTable *symbolTable = internTableNew();
    Token *curToken = NULL;
    jmp_buf exit; // on case of error
    
    /* First we push a new stringBuilder in our parseStructure. This leaves the
     * root so that we can add a symbol table to the beginning of the bytecode
     * later on. */
    ParseStructure *node = parseStructurePush(root);
    
    void parserRequire(bool val, String message)
    {
        if (unlikely(!val))
        {
            printf("Parser error: %s\n", message);
            longjmp(exit, true);
        }
    }
    
    inline void nextToken()
    {
        curToken = lex(source, curToken);
        source += curToken->end;
    }
    
    Size intern(String s)
    {
        return internString(symbolTable, s);
    }
    
    /* The following function takes an array of keywords and finds the
     * corresponding message keyword's interned value. Does not work for unary
     * messages, just use intern() instead. */
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
    
    Token *lookahead(Size n)
    {
        Token *token = curToken;
        String currentPos = source;
        Token *lookahead = NULL;
        while (n--)
        {
            curToken = lex(currentPos, curToken);
            currentPos += curToken->end;
        }
        lookahead = curToken;
        curToken = token;
        return lookahead;
    }
    
    inline void outByte(u8 byte, ParseStructure *ps)
    {
        if (byte >= 0xF0)
            stringBuilderAppendChar(ps->sb, extendedBC8);
        stringBuilderAppendChar(ps->sb, byte);
    }
    
    void outWord(u16 word, ParseStructure *ps)
    {
        outByte(extendedBC16, ps);
        outByte(word & 0xFF, ps);
        outByte((word >> 8) & 0xFF, ps);
    }
    
    void outDWord(u32 dword, ParseStructure *ps)
    {
        outByte(extendedBC32, ps);
        outByte(dword & 0xFF, ps);
        outByte((dword >> 8) & 0xFF, ps);
        outByte((dword >> 16) & 0xFF, ps);
        outByte((dword >> 24) & 0xFF, ps);
    }
    
    void outVal(Size word, ParseStructure *ps)
    {
        if (word == (word & 0xFF))
            outByte((u8)word, ps);
        else if (word == (word & 0xFFFF))
            outWord((u16)word, ps);
        else
            outDWord(word, ps);
    }
    
    void outStr(String s, ParseStructure *ps)
    {
        stringBuilderAppendN(ps->sb, s, strlen(s) + 1); // include null
    }
    
    inline bool startsValue(TokenType type)
    {
        return type == integerToken   || type == doubleToken      ||
               type == stringToken    || type == charToken        ||
               type == symbolToken    || type == openBracketToken ||
               type == openParenToken || type == openBraceToken   ||
               type == keywordToken;
    }
    
    void parseBlockHeader(bool methodDecl, Size argc)
    {
        /* In a method declaration, the syntax for arguments is different,
         * so the argument portion is skipped. Argc is given if methodDecl.
         * 
         * The output syntax is as follows;
         * 
         * [Variable Count] [Argument Count] [arg interned]* [var interned]*
         * 
         * Note that an extended command can occur at the beginning of any of
         * these sections.
         * 
         */
        ParseStructure *parseBlockNode = node;
        Size varc = 0;
        node = parseStructurePush(node);
        TokenType lookaheadType = lookahead(1)->type;
        if ((lookaheadType == commaToken || lookaheadType == pipeToken)
            && !methodDecl)
        {
            /* Arguments */
            
            parserRequire(curToken->type == keywordToken,
                "Expected keyword token");
            outVal(intern(curToken->data), node);
            argc++;
            nextToken();
            while (curToken->type != pipeToken)
            {
                parserRequire(curToken->type == commaToken,
                    "Expected keyword token");
                nextToken();
                parserRequire(curToken->type == keywordToken,
                    "Expected keyword token");
                outVal(intern(curToken->data), node);
                argc++;
                nextToken();
            }
        }
        if (curToken->type == pipeToken)
        {
            nextToken();
            lookaheadType = lookahead(1)->type;
            if (lookaheadType == pipeToken || lookaheadType == commaToken)
            {
                parserRequire(curToken->type == keywordToken,
                    "Expected keyword token");
                outVal(intern(curToken->data), node);
                varc++;
                nextToken();
                while (curToken->type != pipeToken)
                {
                    parserRequire(curToken->type == commaToken,
                        "Expected keyword token");
                    nextToken();
                    parserRequire(curToken->type == keywordToken,
                        "Expected keyword token");
                    outVal(intern(curToken->data), node);
                    varc++;
                    nextToken();
                }
                nextToken();
            }
        }
        
        outVal(varc, parseBlockNode);
        outVal(argc, parseBlockNode);
        node = parseStructureCommit(parseBlockNode);
    }
    
    auto void parseValue();
    
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
                        parserRequire(i == 1, "Expected ':' token. Useparenthesis around binary messages.");
                        outByte(symbolBC, node);
                        outVal(intern(keywords[0]), node);
                        outByte(messageBC, node);
                        outByte(0, node); // argument count
                        isUnary = true;
                        break;
                    }
                    nextToken();
                    parseValue();
                }
                if (isUnary)
                    continue;
                outByte(symbolBC, node);
                outVal(methodNameIntern(i, keywords), node);
                outByte(messageBC, node);
                outVal(i, node);
            }
            else
            {
                Size binaryMessage = intern(curToken->data);
                nextToken();
                parseValue();
                outByte(symbolBC, node);
                outVal(binaryMessage, node);
                outByte(messageBC, node);
                outByte(1, node); // number of arguments
            }
        }
    }
    
    auto void parseStmt();
    
    void parseMethodList()
    {
        /* Output format:
         * 
         * [method count] {block definitions} */
        
        Size methodCount = 0;
        
        while (curToken->type != closeBraceToken)
        {
            methodCount++;
            parserRequire(curToken->type == keywordToken,
                "Expected method declaration");
            String keywords[maxKeywordCount];
            String args[maxKeywordCount + 1];
            Size keywordc = 0,
                 argc = 0;
            args[argc++] = "self";
            keywords[keywordc++] = curToken->data;
            nextToken();
            if (curToken->type == colonToken) // not unary message
            {
                nextToken();
                args[argc++] = curToken->data;
                nextToken();
                while (curToken->type != openBraceToken)
                {
                    parserRequire(curToken->type == keywordToken,
                        "Expected method keyword");
                    parserRequire(keywordc < maxKeywordCount,
                        "Message has too many keywords");
                    keywords[keywordc++] = curToken->data;
                    nextToken();
                    parserRequire(curToken->type == colonToken,
                        "Expected ':' token");
                    nextToken();
                    args[argc++] = curToken->data;
                    nextToken();
                }
                outVal(methodNameIntern(keywordc, keywords), node);
            }
            else
                outVal(intern(keywords[0]), node);
            
            outByte(blockBC, node);
            parserRequire(curToken->type == openBraceToken, "Expected '{' token");
            nextToken();
            parseBlockHeader(true, argc);
            parseStmt();
            parserRequire(curToken->type == closeBraceToken, "Expected '}' token");
            nextToken();
            outByte(endBC, node);
        }
    }
    
    void parseValue()
    {
        switch (curToken->type)
        {
            case integerToken:
            {
                outByte(integerBC, node);
                outStr(curToken->data, node);
                nextToken();
            } break;
            case doubleToken:
            {
                outByte(doubleBC, node);
                outStr(curToken->data, node);
                nextToken();
            } break;
            case stringToken:
            {
                outByte(stringBC, node);
                outStr(curToken->data, node);
                nextToken();
            } break;
            case charToken:
            {
                outByte(charBC, node);
                outByte((char)(Size)curToken->data, node);
                nextToken();
            } break;
            case symbolToken: // variable
            {
                outByte(symbolBC, node);
                outVal(intern(curToken->data), node);
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
                outByte(arrayBC, node);
                outVal(elements, node);
            } break;
            case openBraceToken: // block
            {
                nextToken();
                outByte(blockBC, node);
                parseBlockHeader(false, 0);
                parseStmt();
                parserRequire(curToken->type == closeBraceToken, "Expected '}' token");
                nextToken();
                outByte(endBC, node);
            } break;
            case openObjectBraceToken:
            {
                /* Format:
                 * 
                 * [ObjectBC] [trait count] [var count] [var intern]* [method count]
                 * 
                 * traits are popped from the stack, as is the object's
                 * prototype (the first in the list of traits, but is not a
                 * trait itself)
                 * 
                 */
                
                Size traitc = 0,
                     varc = 0;
                
                /* Parse prototype */
                
                nextToken();
                parseStmt();
                
                /* Parse traits */
                
                while (curToken->type != pipeToken)
                {
                    parserRequire(curToken->type == commaToken, "Expected ',' token");
                    nextToken();
                    parseStmt();
                    traitc++;
                }
                parserRequire(curToken->type == pipeToken, "Expected '|' token");
                
                outByte(objectBC, node);
                outVal(traitc, node);
                
                /* Parse variables */
                
                ParseStructure *objectParseStructure = node;
                node = parseStructurePush(objectParseStructure);
                
                TokenType lookaheadType = lookahead(1)->type;
                if (lookaheadType == pipeToken || lookaheadType == commaToken)
                {
                    parserRequire(curToken->type == keywordToken,
                        "Expected keyword token");
                    outVal(intern(curToken->data), node);
                    nextToken();
                    varc++;
                    while (curToken->type != pipeToken)
                    {
                        nextToken();
                        parserRequire(curToken->type == keywordToken,
                            "Expected keyword token");
                        outVal(intern(curToken->data), node);
                        nextToken();
                        varc++;
                    }
                    nextToken();
                }
                
                outVal(varc, objectParseStructure);
                node = parseStructureCommit(objectParseStructure);
                parseMethodList();
            } break;
            case openTraitBraceToken:
            {
                nextToken();
                outByte(traitBC, node);
                parseMethodList();
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
                    outByte(setBC, node);
                    outVal(keyword, node);
                }
                else
                {
                    outByte(variableBC, node);
                    outVal(intern(curToken->data), node);
                    nextToken();
                }
            } break;
            default:
            {
                parserRequire(false, "Parser Error");
            } break;
        }
    }
    
    void parseStmt()
    {
        while (startsValue(curToken->type))
        {
            parseExpr();
            if (curToken->type == stopToken)
            {
                outByte(stopBC, node);
                nextToken();
            }
            else break;
        }
    }
    
    StringBuilder *cleanup(bool all)
    {
        /* if "all" is true, then we don't leave any output to return at all. */
        Token *token = curToken;
        do tokenDel(token); while ((token = token->previous) != NULL);
        internTableDel(symbolTable);
        StringBuilder *result = parseStructureCollapse(root);
        if (all)
        {
            stringBuilderDel(result);
            return NULL;
        }
        return result;
    }
    
    if (setjmp(exit))
    {
        cleanup(true);
        return NULL;
    }
    
    nextToken();
    parseBlockHeader(false, 0);
    parseStmt();
    outByte(EOF, node);
    
    /* Build symbol table */
    
    outVal(symbolTable->count, root);
    Size i;
    for (i = 0; i < symbolTable->count; i++)
        outStr(symbolTable->table[i], root);
    
    StringBuilder *result = cleanup(false);
    
    /* Print the result */
    Size bytecodeSize = result->size;
    for (i = 0; i < bytecodeSize; i++)
    {
        u8 byte = result->s[i];
        printf("%x ", byte);
        if (byte > 0x80)
            printf("%s\n", bytecodes[byte - 0x80]);
        else
            printf("%c\n", byte);
    }
    
    return (u8*)stringBuilderToString(result);
}

