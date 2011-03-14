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

/*
list  :: '[' value? (',' value)* ']'

block :: '{' (Keyword* ':')? stmt* '}'

value :: String
      :: Num
      :: Keyword
      :: list
      :: block
      :: '(' expr ')'

expr  :: expr (Keyword | SpecialCharacter value | (Keyword ':' value)+)
      :: value

stmt  :: expr '.'
      :: keyword '=' expr '.'
*/
 
void parserRequire(Token *token, bool condition, String error)
{
    if (unlikely(!condition))
    {
        printf("\n [[ Parser Error ]]\n");
        printf("Token: %s Data: %s Line: %i Col: %i\n", tokenTypeNames[token->type], token->data, token->line, token->col);
        printf(error);
        endThread();
    }
}

void parseTreeDump(ParseNode *root, u32 depth)
{
    int i;
    for (i = 0; i < depth; i++)
        printf(" ");
    printf("%x value: %s left: %x right: %x\n", root, root->value, root->left, root->right);
    if (root->left != NULL)
        parseTreeDump(root->left, depth + 1);
    if (root->right != NULL)
        parseTreeDump(root->right, depth + 1);
}


ParseNode *parse()
{
    Token *curToken;
    
    auto ParseNode *parseStmt();
    auto ParseNode *parseExpr();
    auto ParseNode *parseValue();
    auto ParseNode *parseProcedure();
    
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
    
    ParseNode *parseNodeNew(String value)
    {
        ParseNode *node = malloc(sizeof(ParseNode));
        node->value = value;
        return node;
    }
    
    ParseNode *parseProcedure()
    {
        ParseNode *node = parseStmt();
        while (curToken->type == stopToken)
        {
            nextToken();
            if (curToken->type == EOFToken)
                return node;
            ParseNode *replaceNode = parseNodeNew("procedure");
            replaceNode->left = node;
            replaceNode->right = parseStmt();
            node = replaceNode;
        }
        return node;
    }
    
    ParseNode *parseStmt()
    {
        if (unlikely(curToken->type == returnToken))
        {
            nextToken();
            return parseExpr();
        }
        else if (unlikely(lookahead(1)->type == eqToken))
        {
            ParseNode *node = parseNodeNew("eq");
            node->left = parseValue();
            parserRequire(curToken, curToken->type == eqToken, "Expected '=' token");
            nextToken();
            node->right = parseExpr(); 
            return node;
        }
        else
        {
            return parseExpr();
        }
        return NULL;
    }

    ParseNode *parseExpr()
    {
        return parseValue();
    }
    
    ParseNode *parseValue()
    {
        switch (curToken->type)
        {
            case stringToken:
            case numToken:
            case keywordToken:
            {
                ParseNode *node = parseNodeNew(curToken->data);
                nextToken();
                return node;
            } break;
            case openBracketToken:
            {
                parserRequire(curToken, false, "Not implemented");
            } break;
            case openBraceToken:
            {
                parserRequire(curToken, false, "Not implemented");
            } break;
            case openParenToken:
            {
                parserRequire(curToken, false, "Not implemented");
            } break;
            case EOFToken:
            {
                return NULL;
            } break;
            default:
            break;
        }
        parserRequire(curToken, false, "Expected expression");
        
        return NULL;
    }
    
    #define lexLoop(source, tokenName) Token *_next = lex(source), *tokenName;\
    for (; (tokenName = _next)->data != NULL; free(tokenName->data),\
    _next = tokenName->next, free(tokenName))
    
    Token *first = lex("a = b. c = d. e = f.");
    curToken = first;
    parseTreeDump(parseProcedure(), 0);
    return NULL;
}
