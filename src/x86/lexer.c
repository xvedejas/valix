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
 *
 *  Maintained by:
 *      Xander Vedėjas <xvedejas@gmail.com>
 */
#include <lexer.h>
#include <main.h>
#include <string.h>
#include <threading.h>

String tokenTypeNames[] =
{   "undefToken",
    "EOFToken",
	/* keywords */
	"throwToken",
	"tryToken",
	"catchToken",
    "classToken",
    "usingToken",
    "defToken",
    "getToken",
    "setToken",
    "newToken",
    "returnToken",
    "yieldToken",
    "importToken",
    "breakToken",
    "continueToken",
    "ifToken",
    "elseToken",
	"whileToken",
	"forToken",
    /* Builtin types */
    "stringToken", // abc
    "numToken",    // 12.34
    "noneToken",   // none
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
	/* symbols */
    "alnumToken",
    "nonalnumToken",
};

void lexerError(String message)
{
    printf("Lexer Error");
    for (;;) sleep(0xFFFFFFFF);
}

Token *tokenNew(Size line, Size col)
{
    Token* token = malloc(sizeof(Token));
    token->type = undefToken;
    token->data = NULL;
    token->next = NULL;
    token->length = 0;
    token->line = line;
    token->col = col;
    return token;
}

Size matchSymbol(String source, Size start)
{
    bool isAlnum;
    switch (source[start])
    {   case '_': case 'a' ... 'z': case 'A' ... 'Z': case '0' ... '9':
            isAlnum = true;
        break;
        default:
            isAlnum = false;
    }

    Size index;
    for (index = 1; true; index++)
    {   switch (source[start + index])
        {   case '_': case 'a' ... 'z': case 'A' ... 'Z': case '0' ... '9':
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

Token *lex(String source)
{
    Token *firstToken = NULL,
        *lastToken = NULL;
    Size i = 0,
        line = 1,
        column = 0;
    void addToken(Token *token)
    {
        if (unlikely(firstToken == NULL))
        {
            firstToken = token;
            lastToken = token;
        } else
        {
            lastToken->next = token;
            lastToken = token;
        }
    }
    void addNewToken(TokenType type)
    {
        Token *token = tokenNew(line, column);
        token->type = type;
        token->data = NULL;
        addToken(token);
        i++;
        column++;
    }

    Size length = strlen(source);
    for (i = 0; i < length;)
    {
        switch(source[i])
        {
            case '/':
            {
                Size length = matchComment(source, i);
                if (length) // we have a comment
                {   Size n;
                    for (n = 0; n < length; n++)
                    {   if (source[i + n] == '\n')
                        {   column = 0;
                            line++;
                        }
                        else
                            column++;
                    }
                    i += length;
                    break;
                }
            } /* Do not break here, fall through */
            case 'a' ... 'z': case 'A' ... 'Z':
            case '~': case '`': case '!': case '@': case '#': case '$':
            case '%': case '^': case '&': case '*': case '-': case '+':
            case '_': case '?': case '<': case '>': case '\\':
            {
                Size length = matchSymbol(source, i);
                String data = malloc(sizeof(char) * (length + 1));
                strlcpy(data, source + i, length);
                Token *token = tokenNew(line, column);
                switch(data[0])
                {   case 'b':
                        // break
                        if (unlikely(strcmp(data + 1, "reak") == 0))
                            token->type = breakToken;
                    break;
                    case 'c':
                        // class
                        if (unlikely(strcmp(data + 1, "lass") == 0))
                            token->type = classToken;
                        // continue
                        else if (unlikely(strcmp(data + 1, "ontinue") == 0))
                            token->type = continueToken;
                        // catch
                        else if (unlikely(strcmp(data + 1, "atch") == 0))
                            token->type = catchToken;
                    break;
                    case 'd':
                        // def
                        if (unlikely(strcmp(data + 1, "ef") == 0))
                            token->type = defToken;
                    break;
                    case 'e':
                        // else
                        if (unlikely(strcmp(data + 1, "lse") == 0))
                            token->type = elseToken;
                    break;
                    case 'g':
                        // get
                        if (unlikely(strcmp(data + 1, "et") == 0))
                            token->type = getToken;
                    break;
                    case 'i':
                        // if
                        if (unlikely(strcmp(data + 1, "f") == 0))
                            token->type = ifToken;
                        // import
                        else if (unlikely(strcmp(data + 1, "mport") == 0))
                            token->type = importToken;
                    break;
                    case 'n':
                        // new
                        if (unlikely(strcmp(data + 1, "ew") == 0))
                            token->type = newToken;
                        // none
                        else if (unlikely(strcmp(data + 1, "one") == 0))
                            token->type = noneToken;
                    break;
                    case 'r':
                        // return
                        if (unlikely(strcmp(data + 1, "eturn") == 0))
                            token->type = returnToken;
                    break;
                    case 's':
                        // set
                        if (unlikely(strcmp(data + 1, "et") == 0))
                            token->type = setToken;
                    break;
                    case 't':
                        // throw
                        if (unlikely(strcmp(data + 1, "hrow") == 0))
                            token->type = throwToken;
                        // try
                        else if (unlikely(strcmp(data + 1, "ry") == 0))
                            token->type = tryToken;
                    break;
                    case 'u':
                        // using
                        if (unlikely(strcmp(data + 1, "sing") == 0))
                            token->type = usingToken;
                    break;
                    case 'w':
                        // while
                        if (unlikely(strcmp(data + 1, "hile") == 0))
                            token->type = whileToken;
                    break;
                    default:
                    break;
                }
                if (token->type == undefToken)
                {
                    switch (data[0])
                    {
                        case 'a' ... 'z': case 'A' ... 'Z':
                            token->type = alnumToken;
                        break;
                        default:
                            token->type = nonalnumToken;
                        break;
                    }
                }
                if (token->type == alnumToken || token->type == nonalnumToken)
                {
                    token->data = data;
                } else /* token->type is some keyword type, don't need data */
                {
                    free(data);
                }
                addToken(token);
                i += length;
                column += length;
            } break;
            case '0' ... '9':
            {   Size length = matchNumber(source, i);
                String data = malloc(sizeof(char) * (length + 1));
                strlcpy(data, source + i, length);

                Token *token = tokenNew(line, column);
                token->type = numToken;
                token->data = data;
                addToken(token);
                i += length;
                column += length;
            } break;
            case '"':
            {   Size length = matchString(source, i);
                String data = malloc(sizeof(char) * (length - 1));
                strlcpy(data, source + i + 1, length - 2);

                Token *token = tokenNew(line, column);
                token->type = stringToken;
                token->data = data;
                addToken(token);
                i += length;
                column += length;
            } break;
            case '=': addNewToken(eqToken);           break;
            case '(': addNewToken(openParenToken);    break;
            case ')': addNewToken(closeParenToken);   break;
            case '{': addNewToken(openBraceToken);    break;
            case '}': addNewToken(closeBraceToken);   break;
            case '[': addNewToken(openBracketToken);  break;
            case ']': addNewToken(closeBracketToken); break;
            case '.': addNewToken(stopToken);         break;
            case ',': addNewToken(commaToken);        break;
            case ':': addNewToken(colonToken);        break;
            case ';': addNewToken(semiToken);         break;
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
                addNewToken(EOFToken);
                return firstToken;
            break;
            default:
                column += 1;
                i++;
            break;
        }
    }
    addNewToken(EOFToken);
    return firstToken;
}
