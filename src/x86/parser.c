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
    printf("%s\n", root->value);
    if (root->left != NULL)
        parseTreeDump(root->left, depth + 1);
    if (root->right != NULL)
        parseTreeDump(root->right, depth + 1);
}

ParseNode *parseNodeNew(String value)
{
    ParseNode *node = malloc(sizeof(ParseNode));
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    return node;
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
    
    ParseNode *parseProcedure()
    {
        ParseNode *node = parseStmt();
        while (curToken->type == stopToken)
        {
            nextToken();
            if (curToken->type == EOFToken)
                return node;
            ParseNode *replaceNode = parseNodeNew("<statement>");
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
            ParseNode *node = parseNodeNew("<return>");
            nextToken();
            node->left = parseExpr();
            return node;
        }
        else if (unlikely(lookahead(1)->type == eqToken))
        {
            ParseNode *node = parseNodeNew("<equal>");
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
    
    ParseNode *parseMsg()
    {
        String keywords[8];
        u32 i = 0;
        ParseNode *node = NULL;
        
        parserRequire(curToken, curToken->type == keywordToken, "Expected keyword");
        while (curToken->type == keywordToken)
        {
            parserRequire(curToken, i < 8, "More than eight keywords in one message");
            keywords[i++] = curToken->data;
            nextToken();
            if (curToken->type != colonToken) /* Binary message */
            {
                parserRequire(curToken, i == 1, "Expected keyword, not binary message. Maybe use parentheses to show you want to use a binary message?");
                node->value = keywords[0];
                return node;
            }
            nextToken();
            
            ParseNode *replaceNode = parseNodeNew("<argument>");
            replaceNode->left = node;
            replaceNode->right = parseValue();
            node = replaceNode;
        }        
        u32 j, len = 0;
        for (j = 0; j < i; j++)
        {
            len += strlen(keywords[j]) + 1;
        }
        String messageName = calloc(len, sizeof(char));
        for (j = 0; j < i; j++)
        {
            strcat(messageName, keywords[j]);
            strcat(messageName, ":");
        }
        ParseNode *message = parseNodeNew("<message>");
        message->left = parseNodeNew(messageName);
        message->right = node;
        return message;
    }
    
    ParseNode *parseExpr()
    {
        ParseNode *node = parseValue();
        while (curToken->type == specialCharToken || curToken->type == keywordToken)
        {
            if (curToken->type == keywordToken)
            {
                ParseNode *replaceNode = parseNodeNew("<expression>");
                replaceNode->left = node;
                replaceNode->right = parseMsg();
                node = replaceNode;
            }
            else /* specialCharToken */
            {
                ParseNode *binaryMsg = parseNodeNew("<message>");
                binaryMsg->left = parseNodeNew(curToken->data);;
                nextToken();
                binaryMsg->right = parseValue();
                ParseNode *replaceNode = parseNodeNew("<expression>");
                replaceNode->left = node;
                replaceNode->right = binaryMsg;
                node = replaceNode;
            }
        }
        return node;
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
            case openBracketToken: /* '[' denotes a list  */
            {
                nextToken();
                ParseNode *node = parseValue();
                while (curToken->type == commaToken)
                {
                    nextToken();
                    parserRequire(curToken, curToken->type != EOFToken, "Unexpected EOF Token");
                    ParseNode *replaceNode = parseNodeNew("<listitem>");
                    replaceNode->left = node;
                    replaceNode->right = parseValue();
                    node = replaceNode;
                }
                parserRequire(curToken, curToken->type == closeBracketToken, "Expected ']' token");
                return node;
            } break;
            case openBraceToken: /* '{' denotes a block */
            {
                nextToken();
                parserRequire(curToken, curToken->type == keywordToken, "Expected keyword token");
                ParseNode *node = parseNodeNew(curToken->data);
                nextToken();
                while (curToken->type != colonToken)
                {
                    parserRequire(curToken, curToken->type == commaToken, "Expected ',' token");
                    nextToken();
                    parserRequire(curToken, curToken->type == keywordToken, "Expected keyword token");
                    ParseNode *replaceNode = parseNodeNew("<argdef>");
                    replaceNode->left = node;
                    replaceNode->right = parseNodeNew(curToken->data);
                    node = replaceNode;
                    nextToken();
                }
                nextToken();
                ParseNode *block = parseNodeNew("<block>");
                block->left = node;
                block->right = parseProcedure();
                return block;
            } break;
            case openParenToken:
            {
                nextToken();
                ParseNode *node = parseExpr();
                parserRequire(curToken, curToken->type == closeParenToken, "Expected ')' token");
                nextToken();
                return node;
            } break;
            case EOFToken:
            {
                parserRequire(curToken, false, "Unexpected EOF Token");
            } break;
            default:
            break;
        }
        parserRequire(curToken, false, "Expected expression");
        
        return NULL;
    }
    
    void parseNodeDel(ParseNode *node)
    {
        if (node->left != NULL)
            parseNodeDel(node->left);
        if (node->right != NULL)
            parseNodeDel(node->right);
        free(node->value);
        free(node);
    }
    
    Token *first = lex("a = {x, y, z : return x * y * z }.");
    curToken = first;
    ParseNode *tree = parseProcedure();
    parseTreeDump(tree, 0);
    parseNodeDel(tree);
    Token *next;
    do
    {
        next = first->next;
        free(first);
        if (first->data != NULL)
            free(first->data);
    } while ((first = next) != NULL);
    return NULL;
}
