#include "fix-html.h"

#include <strings.h>

// output stream error check
#define fail_if(cond) ({ \
	if(cond) {  \
		fclose(stdout); \
		die_errno("writing output"); \
	}   \
})

// write out a byte
#define write_byte(b)   fail_if(putchar(b) == EOF)

// write out bytes
static
void write_bytes(const char* const s, const size_t n) {
	fail_if(n > 0 && fwrite(s, 1, n, stdout) < n);
}

// write out str object
static inline
void write_str(const str s) {
	write_bytes(s.data, s.length);
}

// write out a string literal
#define write_lit(s)    write_bytes(s "", sizeof(s) - 1)

// write out C string
#define write_cstr(s)   fail_if(fputs((s), stdout) == EOF)

// write out HTML-escaped text
static
void write_html(const char* s) {
	if(!s)
		return;

	const char* p = s;

	while(*p) {
		str repl;

		switch(*p) {
			case '&':
				repl = str_lit("&amp;");
				break;

			case '<':
				repl = str_lit("&lt;");
				break;

			case '>':
				repl = str_lit("&gt;");
				break;

			default:
				++p;
				continue;
		}

		// write replacement
		write_bytes(s, p - s);
		write_str(repl);
		s = ++p;
	}

	write_bytes(s, p - s);
}

// write out HTML-escaped attribute
static
void write_attr(const char* s) {
	if(!s)
		return;

	const char* p = s;

	while(*p) {
		str repl;

		switch(*p) {
			case '&':
				repl = str_lit("&amp;");
				break;

			case '<':
				repl = str_lit("&lt;");
				break;

			case '>':
				repl = str_lit("&gt;");
				break;

			case '"':
				repl = str_lit("&quot;");
				break;

			case '\'':
				repl = str_lit("&apos;");
				break;

			default:
				++p;
				continue;
		}

		// write replacement
		write_bytes(s, p - s);
		write_str(repl);
		s = ++p;
	}

	write_bytes(s, p - s);
}

// write out HTML comment
static
void write_comment(const char* s) {
	if(!s)
		return;

	write_lit("<!--");

	const char* p = s;

	while((p = strchr(p, '-')) != NULL) {
		switch(p[1]) {
			case 0:
				write_bytes(s, p - s);
				write_lit("- -->");	// add space after trailing dash
				return;

			case '-':
				write_bytes(s, p - s);
				write_lit(u8"—");	// replace double dash with em-dash
				s = (p += 2);
				break;

			default:
				p += 2;
				break;
		}
	}

	if(*s)
		write_cstr(s);

	write_lit("-->");
}

// write out raw text
static
void write_raw_text(const char* s, const char* const tag) {
	if(!s)
		return;

	const size_t tag_len = strlen(tag);
	const char* p = s;

	while(*p) {
		if(p[0] == '<' && p[1] == '/' && strncasecmp(p + 2, tag, tag_len) == 0) {
			p += 2;
			write_bytes(s, p - s);  	// up to and including '/'
			write_byte('\\');
			write_bytes(p, tag_len);	// the tag
			s = (p += tag_len);
			continue;
		}

		++p;
	}

	write_bytes(s, p - s);
}

// write out a string with prefix and closing quote if the string is not empty
static
bool cond_write(const char* const s, const str prefix) {
	if(s && *s) {
		write_str(prefix);
		write_cstr(s);
		write_byte('"');

		return true;
	}

	return false;
}

// write out DOCTYPE
static
void write_doctype(const GumboDocument* const doc) {
	if(!doc->has_doctype) {
		write_lit("<!DOCTYPE html>\n");
		return;
	}

	write_lit("<!DOCTYPE ");
	write_cstr(doc->name);

	if(cond_write(doc->public_identifier, str_lit(" PUBLIC \"")))
		cond_write(doc->system_identifier, str_lit(" \""));
	else
		cond_write(doc->system_identifier, str_lit(" SYSTEM \""));

	write_lit(">\n");
}

// write out element
static
void write_element(const GumboElement* const element) {
	const char* const tag = gumbo_normalized_tagname(element->tag);

	// write tag
	write_byte('<');
	write_cstr(tag);

	// write attributes
	for(unsigned i = 0; i < element->attributes.length; i++) {
		const GumboAttribute* const attr = element->attributes.data[i];

		if(strpbrk(attr->name, "<>\"'= \t\n\r"))
			continue;	// skip attributes with invalid symbols

		write_byte(' ');
		write_cstr(attr->name);

		if(!attr->value[0])
			continue;  // boolean attribute

		write_lit("=\"");
		write_attr(attr->value);
		write_byte('"');
	}

	write_byte('>');

	// write content
	switch(element->tag) {
		case GUMBO_TAG_AREA:	case GUMBO_TAG_BASE:	case GUMBO_TAG_BR:
		case GUMBO_TAG_COL:		case GUMBO_TAG_EMBED:	case GUMBO_TAG_HR:
		case GUMBO_TAG_IMG:		case GUMBO_TAG_INPUT:	case GUMBO_TAG_KEYGEN:
		case GUMBO_TAG_LINK:	case GUMBO_TAG_META:	case GUMBO_TAG_PARAM:
		case GUMBO_TAG_SOURCE:	case GUMBO_TAG_TRACK:	case GUMBO_TAG_WBR:
			return;	// void tag

		case GUMBO_TAG_SCRIPT:
		case GUMBO_TAG_STYLE:
			for(unsigned i = 0; i < element->children.length; i++) {
				const GumboNode* const child = element->children.data[i];

				if(child->type == GUMBO_NODE_TEXT)
					write_raw_text(child->v.text.text, tag);
				else
					write_node(child);	// just in case...
			}

			break;

		default:
			for(unsigned i = 0; i < element->children.length; i++)
				write_node(element->children.data[i]);

			break;
	}

	// close the tag
	write_lit("</");
	write_cstr(tag);
	write_byte('>');
}

// write out HTML node
void write_node(const GumboNode* const node) {
	if(!node)
		return;

	switch(node->type) {
		case GUMBO_NODE_DOCUMENT: {
			const GumboDocument* const doc = &node->v.document;

			write_doctype(doc);

			for(unsigned i = 0; i < doc->children.length; i++)
				write_node(doc->children.data[i]);

			break;
		}

		case GUMBO_NODE_ELEMENT:
		case GUMBO_NODE_TEMPLATE:
			write_element(&node->v.element);
			break;

		case GUMBO_NODE_CDATA:
		case GUMBO_NODE_TEXT:
			write_html(node->v.text.text);
			break;

		case GUMBO_NODE_WHITESPACE:
			write_cstr(node->v.text.text);
			break;

		case GUMBO_NODE_COMMENT:
			write_comment(node->v.text.text);
			break;
	}
}
