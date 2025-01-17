#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include "lexer.h"

char cur_symbol;
token_t token;
int token_error;
int boot_load = 0;
char *boot_code;
#define SYMBOL_BUFFER_SIZE 8
char symbol_buffer[SYMBOL_BUFFER_SIZE];
int buffer_write_pos = 0;
int buffer_read_pos = 0;

void error(char *str, ...);

void get_cur_char()
{
    if (buffer_write_pos == buffer_read_pos) {
	if (!boot_load)
	    cur_symbol = getchar();
	else
	    cur_symbol = *boot_code++;
	symbol_buffer[buffer_write_pos++] = cur_symbol;
	buffer_read_pos = buffer_write_pos &= SYMBOL_BUFFER_SIZE - 1;
    } else {
	cur_symbol = symbol_buffer[buffer_read_pos++];
	buffer_read_pos &= SYMBOL_BUFFER_SIZE - 1;
    }
}

void unget_cur_char()
{
    cur_symbol = symbol_buffer[(buffer_read_pos - 2) & SYMBOL_BUFFER_SIZE - 1];
    --buffer_read_pos;
    buffer_read_pos &= SYMBOL_BUFFER_SIZE - 1;
}

int is_whitespace(char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

void skip_comment()
{
    while (cur_symbol != '\n' && cur_symbol != EOF)
	get_cur_char();
}

void skip_white_space()
{
    while (is_whitespace(cur_symbol) || cur_symbol == ';') {
	if (cur_symbol == ';')
	    skip_comment();
	get_cur_char();
    }
    unget_cur_char();
}

int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

int is_alpha(char c)
{
    return c >= 'a' && c <= 'z' || c>= 'A' && c <= 'Z';
}

int is_symbol(char c)
{
    char str[] = "+-*/=_&|<>";
    for (int i = 0; i < sizeof(str); i++)
	if (str[i] == c)
	    return 1;
    return 0;
}

int is_hex_symbol(char c)
{
    return c >= 'a' && c <= 'f' ||
           c >= 'A' && c <= 'F';
}

int is_delimeter(char c)
{
    return c == ')' || c == '(' || is_whitespace(c) || c == EOF || c == '"' || c == '\\';
}

int hex_num()
{
    long long cur_num =  0;
    int hex_value;
    const int msb_shr = CHAR_BIT * sizeof(int);

    do {
		get_cur_char();
		if (is_delimeter(cur_symbol))
			break;
		if (is_digit(cur_symbol)) {
			cur_num = cur_num * 16 + cur_symbol - '0';
		} else if (is_hex_symbol(cur_symbol)) {
			cur_symbol = toupper(cur_symbol);
			cur_num = cur_num * 16 + cur_symbol - 'A' + 10;
		} else {
			token_error = 1;
			printf("invalid hex num\n");
			return 0;
		}
		if ((cur_num >> msb_shr) & 1) {
			token_error = 1;
			printf("hex number overflow\n");
			return 0;
		}
    } while (is_digit(cur_symbol) || is_hex_symbol(cur_symbol));

    unget_cur_char();
    return cur_num;
}

int get_float_num(int int_num)
{
    int cnt = 0;
    float float_num = 0;
    int floatbits = 0;
    int num = int_num;
    int temp;
    get_cur_char();
    float count = 1;
    while(is_digit(cur_symbol)) {
	float_num = float_num * 10 + (cur_symbol - '0');
	count *= 10;
	get_cur_char();
    }
    float res_float = int_num > 0 ?  int_num + (float_num / count) : int_num - (float_num / count);
    printf("res_float is equal  %f\n", res_float);
    return *(int *)&res_float;
}

int get_num()
{
    int fl = 0;
    int cur_num = 0;
    if (cur_symbol == '0') {
	get_cur_char();
	if (cur_symbol == 'x')
	    return hex_num();
    } else if (cur_symbol == '-') {
        fl = 1;
        get_cur_char();
    }
    const int sgn_shr = CHAR_BIT * sizeof(int) - 1;
    int sgn = fl ? -1 : 1;
    int msb;
    while (is_alpha(cur_symbol) || is_digit(cur_symbol) || is_symbol(cur_symbol) || cur_symbol == '.')  {
	if (is_digit(cur_symbol)) {
	    cur_num = cur_num * 10 + sgn * (cur_symbol - '0');
	    msb = (cur_num >> sgn_shr) & 1;
	    if (msb != fl) {
		token_error = 1;
		printf("number overflow\n");
		return 0;
	    }
	    get_cur_char();
	} else if (cur_symbol == '.') {
	    token.type = T_FLOAT;
	    return get_float_num(cur_num);
	} else {
	    token_error = 1;
	    printf("invalid num\n");
	    return 0;
	}
    }
    unget_cur_char();
    return cur_num;
}

void get_symbol(char *cur_str)
{
    int c = 0;
    while (!is_delimeter(cur_symbol))
    {
	if (cur_symbol == '\b') {
	    cur_str[--c] = ' ';
	    get_cur_char();
	    continue;
	}
	if (!(is_alpha(cur_symbol) || is_digit(cur_symbol) || is_symbol(cur_symbol))){
	    printf("ERROR: lexer.c: Unsupported character in input: %c(#%x)", cur_symbol, cur_symbol);
	    token_error = 1;
	    return;
	}
        cur_str[c++] = cur_symbol;
        get_cur_char();
	if (c > MAX_SYMBOL) {
	    printf("ERROR: lexer.c: MAX_SYMBOL");
	    token_error = 1;
	    return;
	}
    }
    unget_cur_char();
    cur_str[c] = 0;
}

void get_string(char *cur_str)
{
    int c = 0;

    get_cur_char();
    while (cur_symbol != EOF) {
	if (cur_symbol == '"')
	    	break;
	else if (c == MAX_STR) {
		token_error = 1;
		--c;
		break;
	}
    	else if (cur_symbol == '\\') {
	    	get_cur_char();
	    	if (cur_symbol == 'n')
			cur_symbol = '\n';
		else if (cur_symbol == 'x')
		{
		    int code = hex_num();
		    if (code > 255)
		    {
			printf("get_string: invalid symbol code\n");
			token_error = 1;
			return;
		    }
		    cur_str[c++] = code;
		    get_cur_char();
		    continue;
		}
	}
	cur_str[c++] = cur_symbol;
	get_cur_char();
    }
    if (cur_symbol != '"')
	token_error = 1;
    cur_str[c] = '\0';
}

token_t get_comma()
{
    char ctr[2];
    ctr[0] = cur_symbol;
    get_cur_char();
    ctr[1] = cur_symbol;
    if (ctr[0] == ',' && ctr[1] == '@') {
        token.type = COMMA_AT;
    }
    else {
        token.type = COMMA;
        unget_cur_char();
    }
}

token_t get_sharp()
{
    char ctr[2];
    ctr[0] = cur_symbol;
    get_cur_char();
    ctr[1] = cur_symbol;
    if (ctr[0] == '#' && ctr[1] == '\\') {
	token.type = T_CHAR;
	get_cur_char();
	token.value = cur_symbol;
    } else {
	token.type = SHARP;
	unget_cur_char();
    }
}

token_t *get_token()
{
    token_error  = 0;
    get_cur_char();
    skip_white_space();
    get_cur_char();
    switch (cur_symbol) {
    case '(':
	token.type = LPAREN;
	break;
    case ')':
	token.type = RPAREN;
	break;
    case EOF:
	token.type = END;
	break;
    case '\'':
	token.type = QUOTE;
	break;
    case '`':
	token.type = BACKQUOTE;
	break;
    case ',':
	get_comma();
	break;
    case '"':
	get_string(token.str);
	token.type = T_STRING;
	break;
    case '#':
	get_sharp();
	break;
    case '.':
	token.type = DOT;
	break;
    default:
	if (cur_symbol == '-' || is_digit(cur_symbol)) {
	    if (cur_symbol == '-') {
	        get_cur_char();
		char c = cur_symbol;
		unget_cur_char();
	        if (!is_digit(c))
	            goto get_token_symbol;
	    }
	    token.type = T_NUMBER;
	    token.value = get_num();
	} else if (is_alpha(cur_symbol) || is_symbol(cur_symbol)) {
	    get_token_symbol:
	        token.type =  T_SYMBOL;
	        get_symbol(token.str);
	} else {
	    token.type = INVALID;
	    error("ERROR: lexer.c: INVALID SYMBOL");
	}
    }
    return &token;
}

void print_token(token_t *token)
{
    switch (token->type) {
    case T_NUMBER:
	printf("NUM %d\n", token->value);
	break;
    case T_SYMBOL:
	printf("SYM %s\n", token->str);
	break;
    case LPAREN:
	printf("LPAREN\n");
	break;
    case RPAREN:
	printf("RPAREN\n");
	break;
    case END:
	printf("END\n");
	break;
    case QUOTE:
	printf("QUOTE\n");
	break;
    case BACKQUOTE:
	printf("BACKQUOTE\n");
	break;
    case COMMA:
	printf("COMMA\n");
	break;
    case COMMA_AT:
	printf("COMMA_AT\n");
	break;
    case INVALID:
	printf("INVALID\n");
	break;
    case T_STRING:
	printf("STRING %s\n", token->str);
	break;
    case SHARP:
	printf("SHARP\n");
	break;
    case DOT:
	printf("DOT\n");
    }
}

void reset_buffer()
{
    buffer_read_pos = buffer_write_pos = 0;
}

