#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "myerror.h"
#include "logger.h"

struct Parser {
	const char* str;
	char cur;
	size_t index;
	size_t len;
};

void parser_start(struct Parser* parser, const char* str) {
	parser->str = str;
	parser->index = 0;
	parser->len = strlen(str);
	parser->cur = parser->len == 0 ? '\0' : parser->str[parser->index];
}

bool parser_isDone(struct Parser* parser) {
	return parser->index > parser->len;
}

void parser_eat(struct Parser* parser) {
	if(parser_isDone(parser))
		return;
	parser->index++;

	parser->cur = parser_isDone(parser) ? '\0' : parser->str[parser->index];
}

void parser_freeCompiled(Vector* compiled) {
	int index = 0;
	struct Node* node = vector_getFirst(compiled, &index);
	while(node != NULL) {
		if(node->type == NT_STRING) {
			free(node->str.lit);
		} else if(node->type == NT_ARRAY) {
			free(node->arr.arr);
			free(node->arr.key);
		} else {
			VTHROW_NEW("Unknown compiled node type: %d", node->type);
		}
		node = vector_getNext(compiled, &index);
	}
	vector_kill(compiled);
}

int parseArrayData(struct Parser * p, struct Node* node);

//TODO: Handle Errors
void parser_compileStr(const char* str, Vector* nodes) {
	vector_init(nodes, sizeof(struct Node), 8);

	struct Parser parser;
	struct Parser* p = &parser;
	parser_start(p, str);

	while(!parser_isDone(p)) {
		struct Node n = {};
		if(p->cur == '$') {
			parser_eat(p);
			parseArrayData(p, &n);
			parser_eat(p);
		} else {
			Vector curLit;
			vector_init(&curLit, sizeof(char), 16);
			while(p->cur != '$' && !parser_isDone(p)) {
				if(p->cur == '\\') {
					parser_eat(p);
					if(p->cur == '\\') {
						vector_putListBack(&curLit, "\\", 1);
						parser_eat(p);
					}else if(p->cur == '$') {
						vector_putListBack(&curLit, "$", 1);
						parser_eat(p);
					}else{
						VTHROW_NEW("Unknown escape character %c", p->cur);
					}
				} else {
					vector_putBack(&curLit, &p->cur);
					parser_eat(p);
				}
			}
			vector_putListBack(&curLit, "\0", 1);
			n.type = NT_STRING;
			n.str.lit = vector_detach(&curLit);
		}
		vector_putBack(nodes, &n);
	}
}

int parseArrayData(struct Parser * p, struct Node* node) {
	node->type = NT_ARRAY;
	{
		Vector ident;
		vector_init(&ident, sizeof(char), 12);

		while(p->cur != '[' && !parser_isDone(p)) {
			vector_putBack(&ident, &p->cur);
			parser_eat(p);
		}
		vector_putListBack(&ident, "\0", 1);

		node->arr.arr = vector_detach(&ident);
		if(node->arr.arr == NULL)
			return MYERR_BASE; //TODO: ERROCODE
	}

	parser_eat(p); //Eat the [

	{
		Vector key;
		vector_init(&key, sizeof(char), 12);

		while(p->cur != ']' && !parser_isDone(p)) {
			vector_putBack(&key, &p->cur);
			parser_eat(p);
		}
		vector_putListBack(&key, "\0", 1);

		node->arr.key = vector_detach(&key);
		if(node->arr.key == NULL)
			return MYERR_BASE; //TODO: ERROCODE
	}

	return 0;
}