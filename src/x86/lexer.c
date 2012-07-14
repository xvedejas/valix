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
#include <lexer.h>
#include <main.h>
#include <cstring.h>
#include <threading.h>
#include <mm.h>

String tokenTypeNames[] =
{
    "undefToken",
    "EOFToken",
    /* Builtin types */
    "charToken",
    "stringToken", // abc
    "integerToken",
    "doubleToken",
    /* Builtin symbols */
    "eqToken",           // =
    "openParenToken",    // (
    "closeParenToken",   // )
    "openBraceToken",    // {
    "closeBraceToken",   // }
    "openBracketToken",  // [
    "closeBracketToken", // ]
    "stopToken",         // .
    "commaToken",        // ,
    "colonToken",        // :
    "semiToken",         // ;
    "pipeToken",         // |
    /* symbols */
    "symbolToken",
    "keywordToken",
    "specialCharToken",
    "objectDefToken",
    "traitDefToken"
};

void lexerError(String message)
{
    printf("Lexer Error");
    printf(message);
    endThread();
}

Size matchKeyword(String source, Size start)
{
    bool isAlnum;
    switch (source[start])
    {
        case '_': case 'a' ... 'z': case 'A' ... 'Z': case '0' ... '9':
            isAlnum = true;
        break;
        default:
            isAlnum = false;
    }

    Size index;
    for (index = 1; true; index++)
    {
        switch (source[start + index])
        {
            case '_': case 'a' ... 'z': case 'A' ... 'Z': case '0' ... '9':
                if (isAlnum == false)
                    return index;
            break;
            case '~': case '`': case '!': case '@': case '#': case '$':
            case '%': case '^': case '&': case '*': case '-': case '+':
            case '=': case '?': case '/': case '<': case '>': case '\\':
                if (isAlnum == true)
                    return index;
            break;
            default:
                return index;
            break;
        }
    }
    lexerError("this should never be reached");
    return 0;
}

Size matchNumber(String source, Size start)
{
    Size index;
    bool radix = false,
        floating = false;
    for (index = 0; true; index++)
    {   switch (source[start + index])
        {   case '0' ... '9':
            case 'A' ... 'Z':
            break;
            case 'r':
                if (unlikely(radix))
                    return index;
                radix = true;
            break;
            case '.':
                if (unlikely(floating))
                    return index;
                // make sure next is numeral
                switch (source[start + index + 1])
                {   case '0' ... '9':
                    case 'A' ... 'Z':
                    break;
                    default:
                        return index;
                }
                floating = true;
            break;
            default:
                return index;
            break;
        }
    }
    lexerError("this should never be reached");
    return 0;
}

Size matchString(String source, Size start)
{
    Size index;
    for (index = 1; true; index++)
    {   switch (source[start + index])
        {   case '"':
                return index + 1;
            break;
            case '\\': /* ignore next character if backslash */
                index++;
            break;
            case '\0':
                lexerError("Unterminated string error");
            break;
            default:
            break;
        }
    }
    lexerError("this should never be reached");
    return 0;
}

/// Todo: if the symbol contents switch from alnum to special characters or
/// vice-versa, also stop matching for it
Size matchSymbol(String source, Size start)
{
    Size index = 0;
    /* The contents of any string are treated like a symbol with '#'.
     * For example, (#abc == #"abc"), and #"a b c" must be written this way  */
    if (source[start + index] == '"')
        return matchString(source, start);
    do
    {
        switch (source[start + index])
        {
            case ' ': case '\n': case '\t': case '\0': case ',': case '[':
            case ']': case '(': case ')': case '{': case '}':
                return index;
            default: break;
        }        
        index++;
    } while (true);
}

Size matchComment(String source, Size start)
{
    bool multiline = true;
    Size index;
    for (index = 1; true; index++)
    {   switch(source[index + start])
        {   case '/':
            {   if (index == 1)
                    multiline = false;
            } break;
            case '*':
            {   if (index == 1)
                    break; // multiLine == true is already default
                if (multiline && source[index + start + 1] == '/')
                    return index + 2;
            } break;
            case '\0':
            {   lexerError("Unterminated comment error\n");
                return 0;
            } break;
            case '\n':
            {   if (!multiline)
                    return index;
            } // Fall through
            default:
            {   if (index == 1)
                    return 0;
            } break;
        }
    }
    lexerError("this should never be reached");
    return 0;
}

void tokenDel(Token *token)
{
    if (token->data != NULL)
        free(token->data);
    free(token);
}

/* Lexes a single token at a time */
Token *lex(String source, Token *lastToken)
{
    Size i = 0;
    Size line, column;
    if (lastToken == NULL)
    {
        line = 1,
        column = 0;
    }
    else
    {
        line = lastToken->line;
        column = lastToken->col;
    }
    
    Token *tokenNew(TokenType type, String data, Size end)
    {
        Token* token = malloc(sizeof(Token));
        token->type = type;
        token->data = data;
        token->line = line;
        token->col = column;
        token->end = end;
        token->previous = lastToken;
        return token;
    }
    
    while (true)
    {
        switch(source[i])
        {
            case '#': // denots a #symbol, a unique string
            {
                i++;
                Size length = matchSymbol(source, i);
                String data = malloc(sizeof(char) * (length + 1));
                strlcpy(data, source + i, length);
                column += length;
                i += length;
                return tokenNew(symbolToken, data, i);
            } break;
            case '/':
            {
                Size length = matchComment(source, i);
                if (length) // we have a comment
                {
                    Size n;
                    for (n = 0; n < length; n++)
                    {
                        if (source[i + n] == '\n')
                        {   column = 0;
                            line++;
                        }
                        else
                            column++;
                    }
                    i += length;
                    break;
                }
            } /* Do not break here, fall through because '/' is not actually
                 representing the start of a comment */
            case '=': case '@': case '%':
            {
                if (source[i] == '=' && source[i+1] == ' ')
                {
                    column++;
                    i++;
                    return tokenNew(eqToken, NULL, i);
                }
                else if (source[i+1] == '{')
                {
                    if (source[i] == '@')
                    {
                        column += 2;
                        i += 2;
                        return tokenNew(openObjectBraceToken, NULL, i);
                    } else if (source[i] == '%')
                    {
                        column += 2;
                        i += 2;
                        return tokenNew(openTraitBraceToken, NULL, i);
                    }
                }
            } /* Do not break here, fall through */
            case 'a' ... 'z': case 'A' ... 'Z': case '~': case '`': case '!':
            case '$': case '^': case '&': case '*': case '-': case '+':
            case '_': case '?': case '<': case '>': case '\\':
            {
                Size length = matchKeyword(source, i);
                String data = malloc(sizeof(char) * (length + 1));
                strlcpy(data, source + i, length);
                Token *token = tokenNew(undefToken, NULL, 0);
                switch (data[0])
                {
                    case 'a' ... 'z': case 'A' ... 'Z': case '_':
                        token->type = keywordToken;
                    break;
                    default:
                        token->type = specialCharToken;
                    break;
                }
                token->data = data;
                column += length;
                token->col = column;
                i += length;
                token->end = i;
                return token;
            } break;
            case '0' ... '9':
            {
                Size length = matchNumber(source, i);
                String data = malloc(sizeof(char) * (length + 1));
                strlcpy(data, source + i, length);
                TokenType type = undefToken;
                if (strchr(data, '.'))
                    type = doubleToken;
                else
                    type = integerToken;
                column += length;
                i += length;
                return tokenNew(type, data, i);
            } break;
            case '"':
            {
                Size length = matchString(source, i);
                String data = malloc(sizeof(char) * (length - 1));
                strlcpy(data, source + i + 1, length - 2);
                column += length;
                i += length;
                return tokenNew(stringToken, data, i);
            } break;
case '(': i++; column++; return tokenNew(openParenToken, NULL, i);    break;
case ')': i++; column++; return tokenNew(closeParenToken, NULL, i);   break;
case '{': i++; column++; return tokenNew(openBraceToken, NULL, i);    break;
case '}': i++; column++; return tokenNew(closeBraceToken, NULL, i);   break;
case '[': i++; column++; return tokenNew(openBracketToken, NULL, i);  break;
case ']': i++; column++; return tokenNew(closeBracketToken, NULL, i); break;
case '.': i++; column++; return tokenNew(stopToken, NULL, i);         break;
case ',': i++; column++; return tokenNew(commaToken, NULL, i);        break;
case ':': i++; column++; return tokenNew(colonToken, NULL, i);        break;
case ';': i++; column++; return tokenNew(semiToken, NULL, i);         break;
case '|': i++; column++; return tokenNew(pipeToken, NULL, i);         break;
            case '\n':
                line++;
                column = 0;
                i++;
            break;
            case '\t':
                column += 4;
                i++;
            break;
            case '\0':
                return tokenNew(EOFToken, NULL, i);
            break;
            case ' ':
            default:
                column += 1;
                i++;
            break;
        }
    }
    panic("error");
    return NULL;
}
