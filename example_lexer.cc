#include "allocator.hh"
#include <string_view>

enum token_type {
	endOfFile = 0,
	identifier = 1,
};

class token_node {
	token_type m_type;
	std::string_view m_ws;
	std::string_view m_lexeme;
 public:
	token_node(token_type type, std::string_view ws, std::string_view lexeme): m_type(type), m_ws(ws), m_lexeme(lexeme) {}

	token_type type() { return m_type; }
	std::string_view lex() { return m_lexeme; }
};

class lexer_t {
	ref<token_node> peek();
	void advance();
};

class lexer {
	std::string_view data;
	size_t pos{0};

	bool isAlpha(char c) {
		return unsigned((c&(~(1<<5))) - 'A') <= 'Z' - 'A';
	}

	bool isWhitespace(char c) {
		return (c == ' ');
	}

 public:
	lexer(std::string_view input): data(input) {}

	ref<token_node> next() {
		if (pos == data.length()) {
			return make<token_node>(token_type::endOfFile, "", "");
		}

		auto ws = pos;
		if (isWhitespace(data[pos])) {
			while(pos < data.length()) {
				if (!isWhitespace(data[pos])) break;
				pos++;
			}
		}

		if (isAlpha(data[pos])) {
			auto start_pos = pos;
			while(pos < data.length()) {
				if (!isAlpha(data[pos])) break;
				pos++;
			}
			return make<token_node>(token_type::identifier, data.substr(ws, start_pos - ws), data.substr(start_pos, pos - start_pos));
		}

		return make<token_node>(token_type::endOfFile, data.substr(ws, pos - ws), "");
	}
};

class lexer_queue: public lexer_t {
	ref<token_node> m_current{ uninitialised{} };
	lexer m_lex;
	bool m_completed;
 public:
	ref<token_node> peek() {
		return m_current;
	}

	void advance() {
		if (m_completed) {
			return;
		}
		m_current = m_lex.next();
		if (m_current->type() == token_type::endOfFile) m_completed = true;
	}

	lexer_queue(std::string_view input): m_lex(input), m_completed(false) {
		advance();
	}
};



#include <iostream>

static RefCounted<mallocator> g{};
int main() {
	galloc = &g;
	auto test = "this is a lexing test with ref<>s";
	auto l = lexer_queue(test);

	while(l.peek()->type() != token_type::endOfFile) {
		auto tok = &l.peek();
		std::cout << "found token = " << tok->lex() << "\n";
		l.advance();
	}
}
