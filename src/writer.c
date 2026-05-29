#include "fix-html.h"

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

// write out a string literal
#define write_lit(s)    write_bytes(s "", sizeof(s) - 1)

// write out C string
#define write_cstr(s)   fail_if(fputs((s), stdout) == EOF)

// white out HTML string
static
void write_html(const char* s) {
	if(!s)
		return;

	str subst;
	const char* p = s;

	for(;;) {
		switch(*p) {
			case 0:   write_bytes(s, p - s);     return;
			case '&': subst = str_lit("&amp;");  break;
			case '<': subst = str_lit("&lt;");   break;
			case '>': subst = str_lit("&gt;");   break;
			case '"': subst = str_lit("&quot;"); break;	// for attributes
			default:  ++p; continue;
		}

		write_bytes(s, p - s);
		write_bytes(subst.data, subst.length);
		s = ++p;
	}
}

static inline
bool is_raw_text_element(const GumboTag tag) {
	return tag == GUMBO_TAG_SCRIPT || tag == GUMBO_TAG_STYLE;
}

// write out a string with prefix and closing quote if the string is not empty
static
bool cond_write(const char* const s, const str prefix) {
	if(s && *s) {
		write_bytes(prefix.data, prefix.length);
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

	write_byte('<');
	write_cstr(tag);

	for(unsigned i = 0; i < element->attributes.length; i++) {
		const GumboAttribute* const attr = element->attributes.data[i];

		write_byte(' ');
		write_cstr(attr->name);

		if(!attr->value[0])
			continue;  // boolean attribute

		write_lit("=\"");
		write_html(attr->value);
		write_byte('"');
	}

	write_byte('>');

	switch(element->tag) {
		case GUMBO_TAG_AREA:   case GUMBO_TAG_BASE:  case GUMBO_TAG_BR:
		case GUMBO_TAG_COL:    case GUMBO_TAG_EMBED: case GUMBO_TAG_HR:
		case GUMBO_TAG_IMG:    case GUMBO_TAG_INPUT: case GUMBO_TAG_KEYGEN:
		case GUMBO_TAG_LINK:   case GUMBO_TAG_META:  case GUMBO_TAG_PARAM:
		case GUMBO_TAG_SOURCE: case GUMBO_TAG_TRACK: case GUMBO_TAG_WBR:
			return;	// void tag

		default:
			break;
	}

	const bool raw = is_raw_text_element(element->tag);

	for(unsigned i = 0; i < element->children.length; i++) {
		const GumboNode* const child = element->children.data[i];

		if(raw && (child->type == GUMBO_NODE_TEXT || child->type == GUMBO_NODE_WHITESPACE))
			write_cstr(child->v.text.text);
		else
			write_node(child);
	}

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

		case GUMBO_NODE_TEXT:
		case GUMBO_NODE_WHITESPACE:
			write_html(node->v.text.text);
			break;

		case GUMBO_NODE_CDATA:
			write_lit("<![CDATA[");
			write_cstr(node->v.text.text);
			write_lit("]]>");
			break;

		case GUMBO_NODE_COMMENT:
			write_lit("<!--");
			write_cstr(node->v.text.text);
			write_lit("-->");
			break;
	}
}

