/*  Copyright (C) 2012 Xander Vedejas <xvedejas@gmail.com>
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

// For parser asserts
#define parserRequire(boolean, message, args...) ({ if (unlikely(!(boolean))) \
{ printf("Parser error: ");\
  printf(message, ## args);\
  printf("\nLine %i, Col %i\n", curToken->line, curToken->col);\
  longjmp(exit, true); } })

/*
 * Note "a*[b]" means a 0 or more times separated by b, optionally followed by b
 * 

unaryMsg   ::= Keyword*
binaryMsg  ::= unaryMsg? Special value binaryMsg?
keywordMsg ::= binaryMsg? (keyword':' value binaryMsg?)*
cascade    ::= value keywordMsg*[';']
expression ::= (Keyword '=')? cascade
statement  ::= expression*['.']
vars       ::= '|' Keyword* '|'
method     ::= (Special? Keyword) | (Keyword ':' Keyword)* '{' vars? stmt '}' 
header     ::= (':' Keyword)* vars?
block      ::= '{' header stmt '}'
objectDef  ::= '[' stmt+[','] vars method* ']'
data       ::= Int | Double | String | Symbol | Char
value      ::= array | block | objectDef | data | '(' statement ')'
array      ::= '(' statement*[','] ')'
top        ::= header stmt

 * 
 * Not syntax but possibly relevant: blocks will currently return whatever
 * value the statement block evaluates to. But sometimes it is desirable to
 * return a value from an inner block. I imagine syntax like the following
 * would be most desirable, since it is fairly flexible and allows breaking
 * and returning from multiple levels deep.
 * 
 * thisBlock return: value
 * thisBlock yield: value
 * thisBlock tailCall: args
 * thisBlock break
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
    "cascade"
};

/* This is a linked list of string builders. When parsing it may be desirable
 * to insert bytecode before bytecode that has already been created, so the
 * parse structure is not collapsed into a single string builder until the
 * end of parsing */
typedef struct parseStructure
{
    StringBuilder *sb;
    struct parseStructure *next;
} ParseStructure;

ParseStructure *parseStructureNew()
{
    ParseStructure *ps = malloc(sizeof(ParseStructure));
    ps->sb = stringBuilderNew(stringBuilderAlloc(), NULL);
    ps->next = NULL;
    return ps;
}

/* Add new stringbuilder to the end of the linked list */
ParseStructure *parseStructurePush(ParseStructure *root)
{
    while (root->next != NULL) root = root->next;
    return (root->next = parseStructureNew());
}

/* Merge the last two stringbuilders in the linked list into one */
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

/* Merge all stringbuilders in the linkned list into one */
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
 *     Symbol          #ab:cd: or #'ab:cd:'
 *     Array           [1, 2]
 *     Block           { :x :y | var1 var2 | x + y }
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
    
    /* Now there are a bunch of nested function definitions. They can access
     * variables on this function's stack frame, making the code thread-safe
     * and encapsulated, in comparison to a strategy using global variables */
    
    /* We only lex as we need to. This lexes the next token and sets
     * the variable curToken accordingly. This also does not allow going back;
     * that is, it frees the previous tokens. Use lookahead() if you don't want
     * to dispose of the current token yet. */
    inline void nextToken()
    {
        Token *previous = curToken;
        curToken = lex(source, ((previous == NULL)?0:curToken->end), curToken);
        tokenDel(previous);
        curToken->previous = NULL; // important to do this
    }
    
    /* Each block of bytecode has its own table of interned strings */
    Size intern(String s)
    {
        printf("table %x interning '%s' ", symbolTable, s);
        Size value = internString(symbolTable, s);
        printf("-> %i\n", value);
        return value;
    }
    
    /* The following function takes an array of keywords and finds the
     * corresponding message keyword's interned value. Does not work for unary
     * or binary messages, just use intern() instead. */
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
        /// don't free, this string belongs to the interntable now
        ///free(messageName);
        return interned;
    }
    
    /* Look ahead by lexing, then backtracking.
     * Only use lookahead to look ahead a constant number of tokens, otherwise
     * it's probably not worth it. */
    Token *lookahead(Size n)
    {
        Token *token = curToken;
        Token *lookahead = NULL;
        while (n--) curToken = lex(source, curToken->end, curToken);
        lookahead = curToken;
        curToken = token;
        return lookahead;
    }
    
    /* Output bytecode to the end */
    inline void outByte(u8 byte, ParseStructure *ps)
    {
        if (byte >= 0xF0)
            stringBuilderAppendChar(ps->sb, extendedBC8);
        stringBuilderAppendChar(ps->sb, byte);
    }
    
    // 16-bit value, lsb out first
    void outWord(u16 word, ParseStructure *ps)
    {
        outByte(extendedBC16, ps);
        outByte(word & 0xFF, ps);
        outByte((word >> 8) & 0xFF, ps);
    }
    
    // 32-bit value, lsb out first
    void outDWord(u32 dword, ParseStructure *ps)
    {
        outByte(extendedBC32, ps);
        outByte(dword & 0xFF, ps);
        outByte((dword >> 8) & 0xFF, ps);
        outByte((dword >> 16) & 0xFF, ps);
        outByte((dword >> 24) & 0xFF, ps);
    }
    
    // Prefer using this to the previous three functions.
    void outVal(Size word, ParseStructure *ps)
    {
        if (word == (word & 0xFF))
            outByte((u8)word, ps);
        else if (word == (word & 0xFFFF))
            outWord((u16)word, ps);
        else
            outDWord(word, ps);
    }
    
    /* Output a string. Must be null terminated. */
    void outStr(String s, ParseStructure *ps)
    {
        stringBuilderAppendN(ps->sb, s, strlen(s) + 1); // include null
    }
    
    inline bool startsValue(TokenType type)
    {
        return type == integerToken    || type == doubleToken      ||
               type == stringToken    || type == charToken        ||
               type == symbolToken    || type == openBracketToken ||
               type == openParenToken || type == openBraceToken   ||
               type == keywordToken;
    }
    
    void expectToken(TokenType type, String typename)
    {
        parserRequire(curToken->type == type, "Expected %s token, got %s token",
            typename, tokenTypeNames[curToken->type]);
    }
    
    void parseVars()
    {
    /* Input syntax:
     * 
     * '|' Keyword* '|'
     * 
     * The output syntax is as follows;
     * 
     * [VarCount] [interned vars...]
     */
        expectToken(pipeToken, "'|'");
        nextToken();
        
        ParseStructure *parseBlockNode = node;
        Size varc = 0;
        node = parseStructurePush(node);
        
        while (curToken->type != pipeToken)
        {
            expectToken(keywordToken, "keyword");
            outVal(intern(curToken->data), node);
            varc++;
            nextToken();
        }
        nextToken();
        outVal(varc, parseBlockNode);
        node = parseStructureCommit(parseBlockNode);
    }
    
    void parseBlockHeader()
    {
    /* Input syntax:
     * 
     * (':' Keyword)* vars?
     * 
     * The output syntax is as follows;
     * 
     * [ArgumentCount] [interned args...] parseVars()
     */
        ParseStructure *parseBlockNode = node;
        Size argc = 0;
        node = parseStructurePush(node);
        
        while (curToken->type == colonToken)
        {
            nextToken();
            expectToken(keywordToken, "block argument");
            outVal(intern(curToken->data), node);
            argc++;
            nextToken();
        }
        if (curToken->type == pipeToken)
            parseVars();
        outVal(argc, parseBlockNode);
        node = parseStructureCommit(parseBlockNode);
    }
    
    auto void parseValue();
    
    void parseUnaryMsg()
    {
    /* Input syntax:
     * 
     * Keyword*
     * 
     * Output syntax:
     * 
     * (messageBC [interned name])*
     * 
     * Note: the number of args for a message can always be determined
     * from its name, so that isn't specified in the bytecode.
     */
        while (curToken->type == keywordToken)
        {
            if (lookahead(1)->type == colonToken)
                return;
            outByte(messageBC, node); // bytecode "message"
            outVal(intern(curToken->data), node); // message name
            nextToken();
        }
    }
    
    void parseBinaryMsg()
    {
    /* Input syntax:
     * 
     * Special value binaryMsg?
     * 
     * Output syntax:
     * 
     * parseValue() parseBinaryMsg() messageBC [interned name]
     * 
     * Note: the number of args for a message can always be determined
     * from its name, so that isn't specified in the bytecode.
     */
        // If we don't have a binary message yet, try unary, which can be a
        // special character or colon
        if (curToken->type != specialCharToken && curToken->type != colonToken)
        {
            parseUnaryMsg();
            // Still no? Well, there must not be a binary message. Return.
            if (curToken->type != specialCharToken &&
                curToken->type != colonToken)
                return;
        }
        
        Size binaryMessage;
        if (curToken->type == colonToken)
            binaryMessage = intern(":");
        else
            binaryMessage = intern(curToken->data);
        nextToken();
        parseValue();
        parseBinaryMsg();
        outByte(messageBC, node); // bytecode "message"
        outVal(binaryMessage, node); // message name
    }
    
    void parseKeywordMsg()
    {
    /* Input syntax:
     * 
     * binaryMsg? (keyword':' value binaryMsg?)*
     * 
     * Output syntax:
     * 
     * parseBinaryMsg() messageBC [interned name] parseValue() parseBinaryMsg()
     * 
     * Note: the number of args for a message can always be determined
     * from its name, so that isn't specified in the bytecode.
     */
        // If we don't have a keyword message yet, try binary
        if (lookahead(1)->type != colonToken)
        {
            parseBinaryMsg();
            // Still no? Well, there must not be a keyword message. Return.
            if (curToken->type != keywordToken)
                return;
        }
        
        String keywords[maxKeywordCount];
        Size i = 0;
        
        while (curToken->type == keywordToken)
        {
            parserRequire(i < maxKeywordCount, "More keywords than "
                "allowed in one message");
            keywords[i++] = curToken->data;
            nextToken();
            expectToken(colonToken, "':'");
            nextToken();
            parseValue();
            parseBinaryMsg();
        }
        outByte(messageBC, node); // bytecode "message"
        outVal(methodNameIntern(i, keywords), node); // message name
    }
    
    void parseExpr()
    {
        bool assignment = (lookahead(1)->type == eqToken);
        Size keyword; // interned variable if assignment is true
        if (assignment)
        {
            expectToken(keywordToken, "assignment variable name");
            keyword = intern(curToken->data);
            nextToken();
            expectToken(eqToken, "'='");
            nextToken();
        }
        /* parsing cascade (series of commands separated by semicolon) */
        parseValue();
        while (curToken->type == keywordToken ||
               curToken->type == specialCharToken)
        {
            parseKeywordMsg();
            if (curToken->type != semiToken)
                break;
            outByte(cascadeBC, node);
            nextToken();
        }
        
        if (assignment)
        {
            outByte(setBC, node);
            outVal(keyword, node);
        }
    }
    
    auto void parseStmt();
    
    void parseMethods()
    {
    /* Input format:
     * 
     * ( (Special? Keyword) | (Keyword ':' Keyword)* '{' vars? stmt '}' )*
     * 
     * Output format:
     * 
     * [method count] ([methodname] [arg]* parseVars() parseStmt() endBC)*
     * 
     * Note: argc can be deduced by the method name
     */
        
        Size methodCount = 0;
        
        ///
        ParseStructure *methodCountNode = node;
        node = parseStructurePush(node);
        
        while (curToken->type != closeBraceToken)
        {
            methodCount++;
            
            if (curToken->type == specialCharToken)
            {
                /* Binary method definition */
                outVal(intern(curToken->data), node);
                nextToken();
                outVal(intern(curToken->data), node);
                nextToken();
            }
            else
            {
                /* Keyword method definition */
                expectToken(keywordToken, "method definition");
                String keywords[maxKeywordCount];
                Size keywordc = 0;
                
                keywords[keywordc] = curToken->data;
                nextToken();
                if (curToken->type == colonToken) // not unary message
                {
                    ParseStructure *methodNameNode = node;
                    node = parseStructurePush(node);
                    
                    nextToken();
                    outVal(intern(curToken->data), node);
                    nextToken();
                    for (; curToken->type != openBraceToken;)
                    {
                        expectToken(keywordToken, "method keyword");
                        parserRequire(keywordc < maxKeywordCount,
                            "Message has too many keywords");
                        keywords[keywordc] = curToken->data;
                        nextToken();
                        expectToken(colonToken, "':'");
                        nextToken();
                        outVal(intern(curToken->data), node);
                        nextToken();
                    }
                    /* Go back and add the method name before the argument
                     * names we just emitted */
                    outVal(methodNameIntern(keywordc, keywords),
                        methodNameNode);
                    node = parseStructureCommit(methodNameNode);
                }
                else
                {
                    /* Unary Message */
                    outVal(intern(keywords[0]), node);
                    /* do not output any args here */
                }
            }
            
            expectToken(openBraceToken, "'{'");
            nextToken();
            if (curToken->type == pipeToken)
                parseVars();
            parseStmt();
            expectToken(closeBraceToken, "'}'");
            nextToken();
            outByte(endBC, node);
        }
        
        /* Go back and insert the number of methods at the beginning */
        outVal(methodCount, methodCountNode);
        node = parseStructureCommit(methodCountNode);
    }
    
    void parseObjectDef()
    {
    /* Input format:
     * 
     * '[' stmt+[','] vars method* ']'
     * 
     * Output format:
     * 
     * objectBC [stmt count] parseStmt()+ parseVars() parseMethods()
     * 
     * Note: the first stmt in parseStmt must be an object to use as a prototype
     * and the other stmts must return traits. These are evaluated immediately.
     * The object definition is distinguished from the block definition by the
     * double colon at the beginning. 
     */
        outByte(objectBC, node);
        
        Size traitc = 0,
             varc = 0;
        
        /* Parse prototype (for object definitions), parse traits
         * for trait definitions. This argument is required. The default
         * supertrait is called Trait, which is an object representing
         * an empty trait. */
        
        nextToken();
        parseStmt();
        
        /* Parse additional traits */
        
        while (curToken->type != pipeToken)
        {
            expectToken(commaToken, "','");
            nextToken();
            parseStmt();
            traitc++;
        }
        outVal(traitc, node);
        
        /* Parse variables */
        
        parseVars();
        
        /* Parse the method list */
        
        parseMethods();
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
            case openBracketToken: // object = [...]
            {
                nextToken();
                parseObjectDef();
                break;
            } break;
            case openBraceToken: // block = {...}
            {
                nextToken();
                parseBlockHeader();
                parseStmt();
                expectToken(closeBraceToken, "'}'");
                nextToken();
                outByte(endBC, node);
            } break;
            case openParenToken:
            {
                /* Represents an inner order of operation for (stmt)
                 * but the following are interpreted as arrays:
                 * 
                 *   (element,)
                 *   (element, element)
                 *   (element, element,)
                 * 
                 *    etc.
                 * 
                 * The following are invalid:
                 * 
                 *   ()
                 *   (,)
                 *   (,element)
                 * 
                 */
                bool seenComma = false;
                Size elementCount = 0;
                nextToken();
                while (true)
                {
                    parseStmt();
                    elementCount++;
                    parserRequire(curToken->type == commaToken ||
                        curToken->type == closeParenToken,
                        "Expected ')' token or ',' token");
                    if (curToken->type == commaToken)
                    {
                        seenComma = true;
                        nextToken();
                    }
                    if (curToken->type == closeParenToken)
                        break;
                }
                nextToken();
                if (seenComma)
                {
                    outByte(arrayBC, node);
                    outVal(elementCount, node);
                }
            } break;
            case keywordToken:
            {
                outByte(variableBC, node);
                outVal(intern(curToken->data), node);
                nextToken();
            } break;
            default:
            {
                parserRequire(false, "Parser Error");
            } break;
        }
    }
    
    void parseStmt()
    {
        printf("curtoken type %s\n", tokenTypeNames[curToken->type]);
        while (startsValue(curToken->type))
        {
            parseExpr();
            if (curToken->type == stopToken)
            {
                outByte(stopBC, node);
                nextToken();
            }
            else
                break;
        }
    }
    
    StringBuilder *cleanup(bool all)
    {
        /* if "all" is true, then we don't leave any output to return.
         * This routine deletes all the existing parse structures. */
        Token *token = curToken;
        do tokenDel(token); while ((token = token->previous) != NULL);
        internTableDel(symbolTable);
        StringBuilder *result = parseStructureCollapse(root);
        if (all)
        {
            stringBuilderFree(stringBuilderDel(result));
            return NULL;
        }
        return result;
    }
    
    if (setjmp(exit))
    {
        /* On the case of error, clean up and return NULL */
        cleanup(true);
        return NULL;
    }
    
    nextToken(); // get first token
    printf("First token type %s data %s\n", tokenTypeNames[curToken->type], curToken->data);
    parseBlockHeader();
    parseStmt();
    outByte(EOF, node);
    
    /* Build symbol table */
    
    printf("symbol table count %i\n", symbolTable->count);
    
    outVal(symbolTable->count, root);
    
    Size i;
    for (i = 0; i < symbolTable->count; i++)
        outStr(symbolTable->table[i], root);
    
    StringBuilder *result = cleanup(false);
    
    /* Print the result */
    Size bytecodeSize = result->size;
    printf("compilation result:\n");
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

