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
 *
 *  Maintained by:
 *      Xander Vedėjas <xvedejas@gmail.com>
 */
#ifndef __lexer_h__
#define __lexer_h__
#include <main.h>

//NOTE: If the order of these are changed. Edit lexer.c
typedef enum
{
    undefToken,        // throws error, make sure this is TokenType == 0
    EOFToken,
    /* Builtin types */
    charToken,       // 'a'
    stringToken,     // "abc"
    integerToken,    // 1234
    doubleToken,     // 12.34
    /* Builtin symbols */
    eqToken,           // =
    openParenToken,    // (
    closeParenToken,   // )
    openBraceToken,    // {
    closeBraceToken,   // }
    openBracketToken,  // [
    closeBracketToken, // ]
    stopToken,         // .
    commaToken,        // ,
    colonToken,        // :
    semiToken,         // ;
    pipeToken,         // |
    /* symbols */
    symbolToken,  // #symbolName
    keywordToken,
    specialCharToken,
} TokenType;

extern String tokenTypeNames[];

typedef struct token
{
    String data;
    TokenType type;
    struct token *previous;
    /* line: line of source code (1-indexed)
     * col: column of source code (0-indexed)
     * start: offset from source where this token begins
     * end: offset from source where this token ends (right after) */
    Size line, col, start, end;
} Token;

extern void tokenDel(Token *token);
extern Token *lex(String source, Size lastTokenEnd, Token *lastToken);
extern void testLexer(String source);

#endif
