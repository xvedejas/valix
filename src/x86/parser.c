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
 * byte meaning         stack args                    end directive with \0?
 *  80  import          none                          yes
 *  81  push string     none (pushes)                 yes
 *  82  push number     "                             yes
 *  83  push keyword    "                             yes
 *  84  call method     as many args as method takes  yes
 *  85  begin list      none                          no
 *  86  begin block     none                          no
 *  87  end list        none                          no
 *  88  end block       none                          no
 *  89  end statement   all                           no
 *  8A  return          [pop return value]            no
 *  8B  set equal       [pop value] [pop symbol]      no
 *  8C  binary message  two                           yes
 */

/* Note that most function are defined as nested functions, that is they are
 * declared entirely within the scope of parse(). This allows us to use
 * variables in parse() that are either on the stack or maybe in registers
 * between all the functions. This means no lock is needed and no values must
 * be passed as parameters to use those variables in a thread-safe way. */
String parse(Token *first)
{
    Size outputSize = 16;
    u8 *output = malloc(sizeof(char) * outputSize);
    Size outputIndex = 0;
    Token *curToken;
    
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
        if (outputIndex + len >= outputSize)
            output = realloc(output, outputIndex + len * 2);
        strncat((String)output + outputIndex, val, len);
        printf("OUT: %s\n", val);
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
        return;
    }
    
    void parseStmt()
    {
        if (unlikely(curToken->type == returnToken))
        {
            nextToken();
            parseExpr();
            out("\x8A", 1); /* Return directive */
        }
        else if (unlikely(curToken->type == importToken))
        {
            out("\x80", 1); /* Import directive */
            nextToken();
            parserRequire(curToken->type == keywordToken, "Expected keyword.");
            out(curToken->data, strlen(curToken->data));
            out("\0", 1);
            nextToken();
        }
        else if (unlikely(lookahead(1)->type == eqToken))
        {
            parserRequire(curToken->type == keywordToken, "Expected keyword.");
            String lefthandKeyword = curToken->data;
            nextToken();
            //parseValue();
            parserRequire(curToken->type == eqToken, "Expected '=' token.");
            nextToken();
            parseExpr();
            out("\x8B", 1); /* Equal directive */
            out(lefthandKeyword, strlen(lefthandKeyword));
            out("\0", 1);
        }
        else
            parseExpr();
        out("\x89", 1); /* Statement directive */
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
                out("\0", 1);
                return;
            }
            nextToken();
            parseValue();
        }        
        u32 j;
        for (j = 0; j < i; j++)
        {
            out(keywords[j], strlen(keywords[j]));
            out(":", 1);
        }
        out("\0", 1);
        return;
    }
    
    void parseExpr()
    {
        while (curToken->type == specialCharToken || curToken->type == keywordToken)
        {
            if (curToken->type == keywordToken)
                parseMsg();
            else /* specialCharToken, binary message */
            {
                String data = curToken->data;
                nextToken();
                parseValue();
                out("\x8C", 1); /* binary message directive */
                out(data, strlen(data));
                out("\0", 1);
            }
        }
        return;
    }
    
    void parseValue()
    {
        switch (curToken->type)
        {
            case stringToken:
            {
                out("\x81", 1); /* String directive */
                out(curToken->data, strlen(curToken->data));
                out("\0", 1);
                nextToken();
                return;
            } break;
            case numToken:
            case keywordToken:
            {
                out("\x83", 1); /* Keyword directive */
                out(curToken->data, strlen(curToken->data));
                out("\0", 1);
                nextToken();
                return;
            } break;
            case openBracketToken: /* '[' denotes a list  */
            {
                out("\x85", 1); /* Begin list directive */
                nextToken();
                while (curToken->type == commaToken)
                {
                    nextToken();
                    parserRequire(curToken->type != EOFToken, "Unexpected EOF Token");
                    parseValue();
                }
                parserRequire(curToken->type == closeBracketToken, "Expected ']' token");
                nextToken();
                out("\x87", 1); /* End list directive */
                return;
            } break;
            case openBraceToken: /* '{' denotes a block */
            {
                out("\x86", 1); /* Begin block directive */
                nextToken();
                
                if (lookahead(1)->type == commaToken || lookahead(1)->type == colonToken)
                {
                    // If we have arguments...
                    parserRequire(curToken->type == keywordToken, "Expected keyword token");
                    out(curToken->data, strlen(curToken->data));
                    out("\0", 1);
                    nextToken();
                    while (curToken->type != colonToken)
                    {
                        parserRequire(curToken->type == commaToken, "Expected ',' token");
                        nextToken();
                        parserRequire(curToken->type == keywordToken, "Expected keyword token");
                        out(curToken->data, strlen(curToken->data));
                        out("\0", 1);
                        nextToken();
                    }
                }
                out("\x86", 1); /* Second begin block directive: means argument list done */
                nextToken();
                parseProcedure();
                parserRequire(curToken->type == closeBraceToken, "Expected ',' token");
                nextToken();
                out("\x88", 1); /* End block directive */
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
    parseProcedure();
    out("\xFF", 1); // EOS
    Token *next;
    do
    {
        next = first->next;
        free(first);
        if (first->data != NULL)
            free(first->data);
    } while ((first = next) != NULL);
    return (String)output;
}
