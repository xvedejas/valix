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

/* value :: ALNUM
 *       || NUMBER
 *       || STRING
 *       || '[' expression (',' expression)* ']'
 *       || '{' symlist? '|' procedure '}'
 *       || '{' procedure '}'
 * 
 * expression :: value (ALNUM | ALNUM ':' expression | NONALNUM expression)*
 *            || '(' expression ')'
 * 
 * symlist :: ALNUM (',' ALNUM)*
 * 
 * lvalue :: expression? ALNUM (ALNUM)*
 * 
 * statement :: ('^' | lvalue '=')? expression '.'
 * 
 * procedure :: (statement)*
 * 
 */
 
#define nextToken() { token = token->next; }

void parserRequire(Token *token, bool condition, String error)
{
    if (unlikely(!condition))
    {
        printf("\n [[ Parser Error ]]\n");
        printf("Token: %s Line: %i Col: %i\n", token->data, token->line, token->col);
        printf(error);
        endThread();
    }
}

void parse()
{
    lexLoop("a + b * c + d", token)
    {
        printf("Data: %s\n", token->data);
        parserRequire(token, false, "test error\n");
    }
}

ParseNode *parseNodeNew(String value)
{
    ParseNode *node = malloc(sizeof(ParseNode));
    node->value = value;
    return node;
}

void parseExpression(Token *token, ParseNode *tree)
{
    ParseNode *node;
    switch (token->type)
    {
        case openParenToken:
        {
            nextToken();
            parserRequire(token, token->type == closeParenToken, "Expected ')' token");
        } break;
        default:
        {
             node = parseNodeNew("expr");
        } break;
    }
}
