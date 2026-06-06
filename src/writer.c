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

// element classification
static
bool is_void(const GumboElement* const element) {
	switch(element->tag) {
		case GUMBO_TAG_AREA:	case GUMBO_TAG_BASE:	case GUMBO_TAG_BR:
		case GUMBO_TAG_COL:		case GUMBO_TAG_EMBED:	case GUMBO_TAG_HR:
		case GUMBO_TAG_IMG:		case GUMBO_TAG_INPUT:	case GUMBO_TAG_KEYGEN:
		case GUMBO_TAG_LINK:	case GUMBO_TAG_META:	case GUMBO_TAG_PARAM:
		case GUMBO_TAG_SOURCE:	case GUMBO_TAG_TRACK:	case GUMBO_TAG_WBR:
			return true;

		default:
			return false;
	}
}

static
bool is_foreign(const GumboElement* const element) {
	return element->tag_namespace == GUMBO_NAMESPACE_SVG
		|| element->tag_namespace == GUMBO_NAMESPACE_MATHML;
}

// write element tag name
static
void write_tag_name(const GumboElement* const element) {
	// known tag
	if(element->tag != GUMBO_TAG_UNKNOWN) {
		write_cstr(gumbo_normalized_tagname(element->tag));
		return;
	}

	// recover tag name from the original text
	str tag = element->original_tag;

	gumbo_tag_from_original_text(&tag);

	// try to fix SVG tags
	if(element->tag_namespace == GUMBO_NAMESPACE_SVG) {
		const char* const s = gumbo_normalize_svg_tagname(&tag);

		if(s && *s) {
			write_cstr(s);
			return;
		}
	}

	write_str(tag);
}

// attribute name validator
static
bool is_valid_attr_name(const char* s) {
	if(!s || *s == 0)
		return false;

	do {
		switch(*s) {
			case 'a' ... 'z':
			case 'A' ... 'Z':
			case '0' ... '9':
			case '_': case '-': case '.': case ':':
				continue;

			default:
				// reject remaining ASCII characters
				if((unsigned char)*s < 0x80)
					return false;
		}
	} while(*++s != 0);

	return true;
}

// write out element
static
void write_element(const GumboElement* const element) {
	// write tag
	write_byte('<');
	write_tag_name(element);

	// write attributes
	for(unsigned i = 0; i < element->attributes.length; i++) {
		const GumboAttribute* const attr = element->attributes.data[i];

		if(!is_valid_attr_name(attr->name))
			continue;	// skip attributes with invalid symbols

		write_byte(' ');
		write_cstr(attr->name);

		if(!attr->value[0])
			continue;  // boolean attribute

		write_lit("=\"");
		write_attr(attr->value);
		write_byte('"');
	}

	if(is_void(element)) {
		write_byte('>');
		return;
	}

	if(element->children.length == 0 && is_foreign(element)) {
		write_lit(" />");
		return;
	}

	write_byte('>');

	// write content
	switch(element->tag) {
		case GUMBO_TAG_SCRIPT:
		case GUMBO_TAG_STYLE:
			for(unsigned i = 0; i < element->children.length; i++) {
				const GumboNode* const child = element->children.data[i];

				if(child->type == GUMBO_NODE_TEXT)
					write_cstr(child->v.text.text);
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
	write_tag_name(element);
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
			write_lit("<![CDATA[");
			write_cstr(node->v.text.text);
			write_lit("]]>");
			break;

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
