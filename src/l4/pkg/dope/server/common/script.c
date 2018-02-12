/*
 * \brief   DOpE command interpreter module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include "dopestd.h"
#include "widget.h"
#include "appman.h"
#include "hashtab.h"
#include "tokenizer.h"
#include "script.h"
#include "scope.h"
#include "window.h"
#include "widget_data.h"

#include "dopedef.h"

#define WIDTYPE_HASHTAB_SIZE  32
#define WIDTYPE_HASH_CHARS    5

#define METHODS_HASHTAB_SIZE  32
#define METHODS_HASH_CHARS    5

#define ATTRIBS_HASHTAB_SIZE  32
#define ATTRIBS_HASH_CHARS    5

#define MAX_TOKENS    256  /* max number of command tokens             */
#define MAX_ARGSTRING 256  /* max lenght of string argument            */
#define MAX_ARGS      16   /* max number of arguments per dope command */
#define MAX_ERRBUF    256  /* max size of error result substring       */

static struct appman_services    *appman;
static struct hashtab_services   *hashtab;
static struct tokenizer_services *tokenizer;

static HASHTAB *widtypes;


/*** UNION OF POSSIBLE METHOD ARGUMENT OR ATTRIBUTE TYPES ***/
union arg {
	long    long_value;
	float   float_value;
	int     boolean_value;
	WIDGET *widget;
	char   *string;
	void   *pointer;
};


/*** INFORMATION ABOUT A SINGLE ATTRIBUT ASSIGNMENT ***/
struct assignment {
	void  *set;                   /* pointer to set function */
	int    baseclass;             /* argument base class     */
	void (*update) (void *,u16);  /* update function         */
	union arg arg;                /* assigned value          */
};


/*** ENVIRONMENT OF AN INTERPRETER ***/
struct interpreter {
	char   *tokens[MAX_TOKENS];   /* pointers token substrings             */
	u32    tok_len[MAX_TOKENS];   /* lengths of token substrings           */
	u32    tok_off[MAX_TOKENS];   /* character offsets of token substrings */
	int    num_tok;               /* total number of tokens                */
	SCOPE *scope;                 /* root scope of the interpreter         */
	char  *dst;                   /* buffer for command result string      */
	int    dst_len;               /* length of result buffer               */
	char   errbuf[MAX_ERRBUF];    /* used for creating error output        */
	char   strbuf[MAX_ARGS][MAX_ARGSTRING];
} cmdint, *ci = &cmdint;

#define INTERPRETER struct interpreter


/*** INTERNAL WIDGET TYPE REPRESENTATION ***/
struct widtype {
	char    *ident;          /* name of widget type          */
	void * (*create)(void);  /* widget creation routine      */
	HASHTAB *methods;        /* widget methods information   */
	HASHTAB *attribs;        /* widget attributs information */
};


/*** INTERNAL METHOD ARGUMENT REPRESENTATION ***/
struct methodarg;
struct methodarg {
	char *arg_name;          /* argument name                   */
	char *arg_type;          /* argument type identifier string */
	char *arg_default;       /* argument default value          */
	int   baseclass;         /* base class of argument type     */
	struct methodarg *next;  /* next argument in argument list  */
};


/*** INTERNAL METHOD REPRESENTATION ***/
struct method {
	char    *name;                  /* method name string               */
	char    *ret_type;              /* identifier string of return type */
	int      ret_baseclass;         /* base class of return type        */
	void  *(*routine)(void *,...);  /* method address                   */
	struct methodarg *args;         /* list of arguments                */
};


/*** INTERNAL ATTRIBUTE REPRESENTATION ***/
struct attrib {
	char    *name;                   /* name of attribute                    */
	char    *type;                   /* type of attribute                    */
	int      baseclass;              /* base class of attribute type         */
	void  *(*get) (void *);          /* get function to request the attibute */
	void   (*set) (void *, void *);  /* set function to set the attribute    */
	void   (*update) (void *, u16);  /* called after attribute changes       */
};


#define VAR_BASECLASS_UNDEFINED 0
#define VAR_BASECLASS_WIDGET    1
#define VAR_BASECLASS_LONG      2
#define VAR_BASECLASS_FLOAT     3
#define VAR_BASECLASS_STRING    4
#define VAR_BASECLASS_BOOLEAN   5

#define CMD_TYPE_METHOD     1
#define CMD_TYPE_ASSIGNMENT 2
#define CMD_TYPE_REQUEST    3


int init_script(struct dope_services *d);


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

/*** THROW ERROR MESSAGE ***
 *
 * This macro copies an error message to the result buffer
 * and cancels the calling function with errtype as return
 * value.
 */
#define ERR(errtype, fmt, args...) {                            \
	if (ci->dst)                                                \
		snprintf(ci->dst, ci->dst_len, "Error: " fmt, ## args); \
	printf("Error: " fmt, ## args); printf("\n");               \
	return DOPECMD_ERR_ ## errtype;                             \
}


/*** FINISH A COMMAND WITH A WARNING ***/
#define WARN(errtype, fmt, args...) {               \
	printf("Warning: " fmt, ## args); printf("\n"); \
	return DOPECMD_WARN_ ## errtype;                \
}


/*** UTILITY: RETURN NULL-TERMINATED STRING OF A TOKEN ***
 *
 * This function copies a specified token to the tmp buffer
 * of the interpreter and terminates it with a zero. This
 * function is exclusively used to return one token within
 * the error message.
 */
static char *err_token(INTERPRETER *ci, int tok) {
	memcpy(&ci->errbuf[0], ci->tokens[tok], ci->tok_len[tok]);
	ci->errbuf[ci->tok_len[tok]] = 0;
	return &ci->errbuf[0];
}


/*** UTILITY: CHECKING CONSTRAINTS FOR COMMAND PROCESSING ***
 *
 * There a certain situation for which typical constraints must be
 * checked. For each such situation there is a constraints_* function.
 * The CHECK macro makes these constraint checks easy to use. If a
 * contrain fails, a error message is thrown.
 */
#define CHECK(assertion) {int ret; if ((ret = assertion) < 0) return ret;}

static int constraints_end_of_command(INTERPRETER *ci, int tok) {
	if (tok < ci->num_tok - 1) {
		if (ci->tokens[tok][0] == ')') {
			ERR(INVALID_ARG, "trailing characters after right parenthesis");
		} else {
			ERR(INVALID_ARG, "suspicious argument '%s'", err_token(ci, tok));
		}
	}
	if (tok >= ci->num_tok)        ERR(UNCOMPLETE, "unexpected end of command");
	if (ci->tokens[tok][0] != ')') ERR(RIGHT_PAR,  "missing right parenthesis");
	return 0;
}


static int constraints_parameter_block(INTERPRETER *ci, int tok) {
	if (tok >= ci->num_tok)        ERR(UNCOMPLETE, "unexpected end of command");
	if (ci->tokens[tok][0] != '(') ERR(LEFT_PAR,   "missing left parenthesis");
	return 0;
}


static int constraints_tag(INTERPRETER *ci, int tok) {
	if (tok >= ci->num_tok)        ERR(UNCOMPLETE, "unexpected end of command");
	if (ci->tokens[tok][0] != '-') ERR(NO_TAG,     "expected tag but found '%s' instead", err_token(ci, tok));
	return 0;
}


static int constraints_value(INTERPRETER *ci, int tok) {
	if (tok >= ci->num_tok)        ERR(UNCOMPLETE,  "unexpected end of command");
	if (ci->tokens[tok][0] == ')') ERR(INVALID_ARG, "missing attribute value");
	return 0;
}


static int extract_string(const char *str_token, int tok_len, char *dst, int dst_len) {
	s32 str_len;
	s32 res_len = 0;

	if (*str_token == '"') {
		str_token++;            /* skip begin of quotation   */
		str_len = tok_len - 2;  /* cut closing apostroph too */
	} else {
		str_len = tok_len;
	}
	if (str_len < 0) return -1;
	if (str_len > dst_len) str_len = dst_len;

	while (str_len-- > 0) {
		if (*str_token == '\\') {
			switch (*(str_token+1)) {
			case '\\': *(dst++) = '\\'; str_token+=2; str_len--; break;
			case '"':  *(dst++) = '"';  str_token+=2; str_len--; break;
			case 'n':  *(dst++) = 0x0a; str_token+=2; str_len--; break;
			default:   *(dst++) = *(str_token++);
			}
		} else {
			*(dst++)=*(str_token++);
		}
		res_len++;
	}
	*dst = 0;       /* null termination */
	return res_len; /* return effective length of extracted string */
}


static char *new_symbol(const char *s,u32 length) {
	char *new = zalloc(length+1);
	if (!new) {
		INFO(printf("Script(new_symbol): out of memory\n");)
		return NULL;
	}
	extract_string(s, length, new, length);
	return new;
}


static u32 get_baseclass(char *vartype) {
	if (!vartype) return 0;
	if (dope_streq(vartype,"long",    5)) return VAR_BASECLASS_LONG;
	if (dope_streq(vartype,"float",   6)) return VAR_BASECLASS_FLOAT;
	if (dope_streq(vartype,"string",  7)) return VAR_BASECLASS_STRING;
	if (dope_streq(vartype,"boolean", 8)) return VAR_BASECLASS_BOOLEAN;
	if (dope_streq(vartype,"Widget",  7)) return VAR_BASECLASS_WIDGET;
	if (hashtab->get_elem(widtypes, vartype, 255)) return VAR_BASECLASS_WIDGET;
	return VAR_BASECLASS_UNDEFINED;
}


static struct methodarg *new_methodarg(void) {
	struct methodarg *m_arg = zalloc(sizeof(struct methodarg));

	if (!m_arg) {
		INFO(printf("Script(register_method): out of memory");)
		return NULL;
	}
	return m_arg;
}


/*** CONVERT IMMEDIATE VALUE TO FUNCTION ARGUMENT ***
 *
 * \param baseclass  desired type of the value
 * \param value      string representation of the value
 * \param len        length of value string
 * \param dst        result argument buffer
 * \return           0 on success or a negative error code
 */
static int convert_value_arg(int baseclass, char *value, int len,
                             union arg *dst) {
	switch (baseclass) {
		case VAR_BASECLASS_LONG:
			if (tokenizer->toktype(value, 0) != TOKEN_NUMBER) break;
			dst->long_value = strtol(value, NULL, 0);
			return 0;

		case VAR_BASECLASS_BOOLEAN:
			if (dope_streq(value,"yes", len) || dope_streq(value,"true", len) ||
			    dope_streq(value,"on",  len) || dope_streq(value,"1",    len)) {
				dst->boolean_value = 1;
				return 0;
			}
			if (dope_streq(value,"no",  len) || dope_streq(value,"false", len) ||
			    dope_streq(value,"off", len) || dope_streq(value,"0",     len)) {
				dst->boolean_value = 0;
				return 0;
			}
			break;

		case VAR_BASECLASS_STRING:
			if (extract_string(value, len, dst->string, MAX_ARGSTRING) >= 0) return 0;
			break;

		case VAR_BASECLASS_FLOAT:
			if (tokenizer->toktype(value, 0) != TOKEN_NUMBER) break;
			dst->float_value = strtod(value, NULL);
			return 0;

		case VAR_BASECLASS_WIDGET:
			if (dope_streq(value, "none", len)) {
				dst->widget = NULL;
				return 0;
			}
	}
	return DOPECMD_ERR_INVALID_ARG;
}


/*** RESOLVE SUB-SCOPE WITHIN SPECIFIED SCOPE ***
 *
 * \param cs   current scope that contains the requested sub scope
 * \param tok  first token containing the name of the next sub scope
 * \param dst  pointer to resolved scope
 * \return     number of processed tokens
 *
 * This function never throws an error.
 */
static int resolve_scope(INTERPRETER *ci, SCOPE *cs, int tok, SCOPE **dst) {
	int start_token = tok;
	SCOPE *ns;

	while ((tok + 2 < ci->num_tok)
	    && (ns = cs->scope->get_subscope(cs, ci->tokens[tok], ci->tok_len[tok]))
	    && (ci->tokens[tok + 1][0] == '.')
		&& (ns->scope->get_var(ns, ci->tokens[tok + 2], ci->tok_len[tok + 2]))) {

		cs = ns;
		tok += 2;
	}
	*dst = cs;
	return tok - start_token;
}


/*** CONVERT WIDGET NAME PATH TO WIDGET REFERENCE ***
 *
 * A widget can reside within nested subscopes.
 */
static int convert_reference_arg(INTERPRETER *ci, int baseclass, int tok,
                                 union arg *dst) {
	SCOPE *s = ci->scope;
	int    start_token = tok;

	if (baseclass != VAR_BASECLASS_WIDGET) return 0;

	/* traverse subscopes */
	tok += resolve_scope(ci, s, tok, &s);

	/* check for widget in subscope */
	dst->widget = s->scope->get_var(s, ci->tokens[tok], ci->tok_len[tok]);
	if (dst->widget) tok++;

	/* return number of processed tokens */
	return tok - start_token;
}


/*** EXTRACT ARGUMENT SEMANTICS FROM TOKENS ***
 *
 * \param baseclass  the expected argument type
 * \param tok        index of the first token of the argument
 * \param dst        pointer to argument buffer
 * \return           number of processed tokens
 *
 * The string element of the dst parameter must be initialized
 * to point to a valid string with the length of MAX_ARGSTRING.
 */
static int convert_arg(INTERPRETER *ci, int baseclass, int tok,
                       union arg *dst) {
	int ret;

	/* try to convert value argument */
	ret = convert_value_arg(baseclass, ci->tokens[tok], ci->tok_len[tok], dst);
	if (ret >= 0) return 1;

	/* try to convert reference argument */
	ret = convert_reference_arg(ci, baseclass, tok, dst);
	if (ret) return ret;

	ERR(INVALID_ARG, "invalid argument '%s'", err_token(ci, tok));
}


static int convert_result(int baseclass, union arg *value, char *dst, int dst_len) {
	if (!dst || !dst_len) return 0;
	dst[0] = 0;
	switch (baseclass) {
		case VAR_BASECLASS_BOOLEAN:
		case VAR_BASECLASS_LONG:
			snprintf(dst, dst_len, "%ld", value->long_value);
			return 0;
		case VAR_BASECLASS_FLOAT:
			dope_ftoa(value->float_value, 4, dst, dst_len);
			return 0;
		case VAR_BASECLASS_STRING:
			if (!value->string) return 0;
			if (snprintf(dst, dst_len, "%s", value->string) >= dst_len)
				WARN(TRUNC_RET_STR, "return string '%s' was truncated", value->string);
			return 0;
	}
	snprintf(dst, dst_len, "ok");
	return 0;
}


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static void *register_widget_type(char *widtype_name,void *(*create_func)(void)) {
	struct widtype *new = (struct widtype *)zalloc(sizeof(struct widtype));

	if (hashtab->get_elem(widtypes, widtype_name, 255)) {
		INFO(printf("Script(register_widget_type): widget type already exists\n");)
		return NULL;
	}

	new->create  = create_func;
	new->methods = hashtab->create(METHODS_HASHTAB_SIZE, METHODS_HASH_CHARS);
	new->attribs = hashtab->create(ATTRIBS_HASHTAB_SIZE, ATTRIBS_HASH_CHARS);
	new->ident   = widtype_name;
	hashtab->add_elem(widtypes, widtype_name, new);

	/* return pointer to widget type structure */
	return new;
}


static void register_widget_method(struct widtype *widtype, char *desc, void *methadr) {
	struct method  *method;
	struct methodarg *m_arg,**cl;
	u32  tok_off[MAX_TOKENS];
	u32  tok_len[MAX_TOKENS];
	char *tokens[MAX_TOKENS];
	u32  num_tok;
	u32  i;

	num_tok = tokenizer->parse(desc, MAX_TOKENS, tok_off, tok_len);
	for (i=0; i<num_tok; i++) {
		tokens[i] = (char *)(desc + tok_off[i]);
	}

	method = (struct method *)zalloc(sizeof(struct method));
	method->routine       = methadr;
	method->name          = new_symbol(desc + tok_off[1], tok_len[1]);
	method->ret_type      = new_symbol(desc + tok_off[0], tok_len[0]);
	method->ret_baseclass = get_baseclass(method->ret_type);
	cl = (struct methodarg **)&method->args;

	/* scan arguments and build argument list */
	if (!dope_streq(tokens[3], "void", tok_len[3])) {
		for (i=3; i<num_tok;) {
			m_arg = new_methodarg();

			/* read argument type */
			m_arg->arg_type  = new_symbol(desc + tok_off[i], tok_len[i]);
			m_arg->baseclass = get_baseclass(m_arg->arg_type);

			if (++i >= num_tok) break;

			/* read argument name */
			m_arg->arg_name = new_symbol(desc + tok_off[i], tok_len[i]);
			if (++i >= num_tok) break;

			/* check if a default value is specified */
			if ((*(desc+tok_off[i])) == '=') {
				if (++i >= num_tok) break;
				m_arg->arg_default = new_symbol(desc + tok_off[i], tok_len[i]);
//				printf("arg_default = \"%s\"\n", m_arg->arg_default);
				if (++i >= num_tok) break;
			} else {
				m_arg->arg_default = NULL;
			}

			m_arg->next = NULL;
			*cl = m_arg;
			cl = (struct methodarg **)&m_arg->next;

			/* skip comma */
			if (++i >= num_tok) break;
		}
	}

	hashtab->add_elem(widtype->methods, method->name, method);
}


static void register_widget_attrib(struct widtype *widtype, char *desc,
                                   void *get,void *set,void *update) {
	struct attrib  *attrib;
	u32 tok_off[MAX_TOKENS];
	u32 tok_len[MAX_TOKENS];

	tokenizer->parse(desc, MAX_TOKENS, tok_off, tok_len);

	attrib = (struct attrib *)zalloc(sizeof(struct attrib));
	if (!attrib) return;

	attrib->name      = new_symbol(desc + tok_off[1], tok_len[1]);
	attrib->type      = new_symbol(desc + tok_off[0], tok_len[0]);
	attrib->baseclass = get_baseclass(attrib->type);
	attrib->get       = get;
	attrib->set       = set;
	attrib->update    = update;

	hashtab->add_elem(widtype->attribs, attrib->name, attrib);
}


/*** CREATE NEW EVENT BINDING ***
 *
 * \param w        widget to attach the binding to
 * \param app_id   application to which the events should be delivered
 * \param tokens   array of binding parameter tokens
 * \param tok_len  lengths of tokens
 * \param num_tok  number of tokens that belong to the bind parameters
 * \param err      error code
 * \return         new binding or NULL if an error occured
 *
 * The token list contain the left and right parenthesis. The name
 * is a mandatory argument, followed by multiple -tag value pairs, which
 * describe the arguments to be passed with events and the conditions
 * of the event.
 */
//static struct new_binding *create_binding(WIDGET *w, SCOPE *rs, char **tokens,
//                                      int tok_len, int num_tok, int *err) {
//	struct new_binding *new = zalloc(sizeof(struct new_binding));
//	int num_args = 0, num_conds = 0;
//	int i;
//
//	*err = 0;
//	if (!new) return NULL;
//
//	/* count conditions and arguments */
//	for (i=0; i<num_tok; i++) {
//		
//	}
//	return NULL;
//}


/*** CALL WIDGET FUNCTION ***
 *
 * You think, this is an infernal mess? You are probably right.
 */
static void *call_routine(void *(*rout)(void *, ...), u16 ac, union arg args[]) {
#define A(i) args[i].pointer
	switch (ac) {
		case 1: return rout(A(0));
		case 2: return rout(A(0),A(1));
		case 3: return rout(A(0),A(1),A(2));
		case 4: return rout(A(0),A(1),A(2),A(3));
		case 5: return rout(A(0),A(1),A(2),A(3),A(4));
		case 6: return rout(A(0),A(1),A(2),A(3),A(4),A(5));
		case 7: return rout(A(0),A(1),A(2),A(3),A(4),A(5),A(6));
		case 8: return rout(A(0),A(1),A(2),A(3),A(4),A(5),A(6),A(7));
		case 9: return rout(A(0),A(1),A(2),A(3),A(4),A(5),A(6),A(7),A(8));
	}
	return NULL;
#undef A
}


/*** DETERMINE TYPE OF COMMAND ***
 *
 * \param ci   command interpreter
 * \param tok  first token of the command
 * \return     command type
 *
 * This function distincts between invokations of methods,
 * assignments and attribute requests.
 */
static int get_command_type(INTERPRETER *ci, int tok) {

	while (tok < ci->num_tok) {
		if (ci->tokens[tok][0] == '(') return CMD_TYPE_METHOD;
		if (ci->tokens[tok][0] == '=') return CMD_TYPE_ASSIGNMENT;
		tok++;
	}

	return CMD_TYPE_REQUEST;
}


/*** DETERMINE INFORMATION ABOUT THE VARIABLE AT THE SPECIFIED TOKEN ***
 *
 * \param s           scope that contains the variable
 * \param tok         token index that points to the variable name
 * \param out_w       variable widget
 * \param out_w_type  variable type information
 * \return            number of consumed tokens
 *
 * There may be two cases:
 *
 * - The variable exists within the specified scope. In this case, a
 *   dot must follow the variable name because it is followed by the
 *   method/attribute name.
 * - The Scope is the variable.
 */
static int get_variable(INTERPRETER *ci, SCOPE *s, int tok,
                        WIDGET **out_w, struct widtype **out_w_type) {
	/*
	 * If the current token is followed by a dot, the current
	 * token must be a variable name and the symbol after the
	 * dot must be a method or attribute name.
	 */
	if ((tok + 1 < ci->num_tok)
	 && (ci->tokens[tok + 1][0] == '.')) {
		char *typename;

		/* determine widget within the scope */
		if (!(*out_w = s->scope->get_var(s, ci->tokens[tok], ci->tok_len[tok])))
			ERR(UNKNOWN_VAR, "unknown variable '%s'", err_token(ci, tok));

		/* widget exists - let us find out about its type */
		typename = s->scope->get_vartype(s, ci->tokens[tok], ci->tok_len[tok]);
		if (!typename)
			ERR(INVALID_VAR, "variable '%s' has invalid type", err_token(ci, tok));

		*out_w_type = hashtab->get_elem(widtypes, typename, 255);

		/* we can skip two tokens (the variable name and the dot) */
		return 2;
	}

	/*
	 * It seems as the scope itself is the variable.
	 * This means, the current token is the method/attribute
	 * name and is not followed by a dot.
	 */

	/* return the scope */
	*out_w = (WIDGET *)s;
	*out_w_type = hashtab->get_elem(widtypes, "Scope", 255);

	/* keep current token */
	return 0;
}


/*** FIND OUT ABOUT A SINGLE ATTRIBUTE ASSIGNMENT TO A WIDGET ***
 *
 * \param ci       command interpreter containing the tokens
 * \param w        widget to modify
 * \param widtype  type information about the widget
 * \param tok      index to tag token followed by its value
 * \param assign   resulting assignment information
 * \return         number of consumed tokens or negative error code
 *
 * The arg->string element of the dst parameter must be initialized to a
 * sting with the length of MAX_ARGSTRING before calling the function.
 */
static int parse_assignment(INTERPRETER *ci, WIDGET *w, struct widtype *widtype,
                            int tok, struct assignment *assign) {
  (void)w;
	struct attrib *attrib;
	char *tag = ci->tokens[tok];
	int ret, consumed_tokens = 0;

	/* determine attribute type */
	CHECK(constraints_tag(ci, tok));
	attrib = hashtab->get_elem(widtype->attribs, tag + 1, ci->tok_len[tok] - 1);

	if (!attrib)
		ERR(UNKNOWN_TAG, "'%s' is not a valid tag", err_token(ci, tok));

	if (!attrib->set)
		ERR(ATTR_W_PERM, "attribute '%s' is not configurable", attrib->name);

	consumed_tokens++; tok++;

	assign->baseclass = attrib->baseclass;
	assign->set       = attrib->set;
	assign->update    = attrib->update;

	/* evaluate value */
	CHECK(constraints_value(ci, tok));
	ret = convert_arg(ci, assign->baseclass, tok, &assign->arg);
	if (ret < 0) return ret;

	consumed_tokens += ret;
	return consumed_tokens;
}


/*** APPLY ASSIGNMENT TO WIDGET ***/
static void apply_assignment(WIDGET *w, struct assignment *a) {
	switch (a->baseclass) {
		case VAR_BASECLASS_WIDGET:
			((void (*)(WIDGET*, WIDGET*))(a->set))(w, a->arg.widget);
			break;
		case VAR_BASECLASS_LONG:
			((void (*)(WIDGET*, long))(a->set))(w, a->arg.long_value);
			break;
		case VAR_BASECLASS_BOOLEAN:
			((void (*)(WIDGET*, int))(a->set))(w, a->arg.boolean_value);
			break;
		case VAR_BASECLASS_FLOAT:
			((void (*)(WIDGET*, float))(a->set))(w, a->arg.float_value);
			break;
		case VAR_BASECLASS_STRING:
			((void (*)(WIDGET*, char*))(a->set))(w, a->arg.string);
			break;
	}
}


/*** EXEC SET METHOD FOR A SPECIFIED WIDGET ***
 *
 * A set method may contain multiple '-tag value' pairs. Each tag
 * corresponds to an attribute that should be set to the specified value.
 *
 * \param tok  token index of the left parenthesis
 */
static int exec_set(INTERPRETER *ci, WIDGET *w, struct widtype *w_type, int tok) {
	int ret = 0, i, j;
	int num_assignments = 0;
	struct assignment assignments[MAX_ARGS], *ca;

	CHECK(constraints_parameter_block(ci, tok));
	tok++;

	/* parse tag value assignments */
	for (; tok < ci->num_tok;) {

		/* check for end of parameter block */
		if (ci->tokens[tok][0] == ')') break;

		/* retrieve information for current assignment */
		ca = &assignments[num_assignments];
		ca->arg.string = &ci->strbuf[num_assignments][0];
		ret = parse_assignment(ci, w, w_type, tok, ca);

		/* return on parse error */
		if (ret < 0) return ret;

		/* no assignment tokens left */
		if (ret == 0) break;

		/* skip processed tokens */
		tok += ret;
		if (num_assignments++ >= MAX_ARGS)
			ERR(TOO_MANY_ARGS, "too many attribute assignments in one command");
	}

	CHECK(constraints_end_of_command(ci, tok));

	/* apply assignments to widget */
	for (i=0; i<num_assignments; i++) {
		apply_assignment(w, &assignments[i]);
	}

	/* call update functions */
	for (i=0; i<num_assignments; i++) {
		if (!assignments[i].update) continue;

		/* call update function */
		assignments[i].update(w, 1);

		/* erase this update function from other assignments */
		for (j=i; j<num_assignments; j++) {
			if (assignments[j].update == assignments[i].update)
				assignments[j].update = NULL;
		}
	}
	return ret;
}


static int exec_function(INTERPRETER *ci, WIDGET *w, struct widtype *w_type,
                         struct method *meth, int tok) {
  (void)w_type;
	struct methodarg *m_arg, *o_arg;
	union arg args[20];
	union arg res;
	int   i, ret, num_args = 1, num_m_args = 0, num_o_args = 0;

	for (i=1; i<MAX_ARGS; i++) {
		args[i].string = &ci->strbuf[i][0];
	}
	args[0].pointer = w;

	CHECK(constraints_parameter_block(ci, tok));
	tok++;

	m_arg = meth->args;

	/* set mandatory parameters */
	for (; m_arg; m_arg = m_arg->next) {
		if (m_arg->arg_default) break;

		CHECK(constraints_value(ci, tok));

		ret = convert_arg(ci, m_arg->baseclass, tok, &args[num_args]);
		if (ret < 0) return ret;
		if (ret == 0) break;
		tok += ret;
		num_args++;
		num_m_args++;

		/* eat comma */
		if (m_arg->next && !m_arg->next->arg_default) {
			if (ci->tokens[tok][0] != ',') {
				ERR(MISSING_ARG, "missing comma after argument '%s'", err_token(ci, tok-1));
			} else {
				tok++;
			}
		}
	}

	if (m_arg && (!m_arg->arg_default))
		ERR(MISSING_ARG, "missing mandatory argument '%s'", m_arg->arg_name);

	/* set default values for optional parameters */
	if (m_arg) {
		o_arg = m_arg;
		num_o_args = 0;
		for (i=num_args; o_arg; o_arg = o_arg->next, i++) {
			if (i >= MAX_ARGS)
				ERR(TOO_MANY_ARGS, "too many optional arguments");

			convert_value_arg(o_arg->baseclass, o_arg->arg_default, 255, &args[i]);
			num_o_args++;
		}
	}

	/* eat comma after mandatory arguments */
	if ((tok < ci->num_tok-1) && (num_m_args > 0)) {
		if (ci->tokens[tok][0] != ',') {
			ERR(MISSING_ARG, "missing comma after mandatory arguments");
		} else {
			tok++;
			if (ci->tokens[tok][0] == ')')
				ERR(NO_TAG, "no optional parameter after comma");
		}
	}

	/* set optional parameters that are specified as tag value pairs */
	for (; tok<ci->num_tok-1;) {
		char *tag = ci->tokens[tok];
		
		CHECK(constraints_tag(ci, tok));
		for (o_arg = m_arg, i = num_args; o_arg; o_arg = o_arg->next, i++)
			if (dope_streq(tag+1, o_arg->arg_name, ci->tok_len[tok]-1))
				break;

		if (!o_arg)
			ERR(UNKNOWN_TAG, "invalid optional parameter '%s'", err_token(ci, tok));

		tok++;  /* skip tag */

		/* convert default argument string to function argument */
		CHECK(constraints_value(ci, tok));
		ret = convert_arg(ci, o_arg->baseclass, tok, &args[i]);
		tok += ret;
	}
	num_args += num_o_args;

	CHECK(constraints_end_of_command(ci, tok));

	res.pointer = call_routine(meth->routine, num_args, args);
	return convert_result(meth->ret_baseclass, &res, ci->dst, ci->dst_len);
}


static int exec_attrib(INTERPRETER *ci, WIDGET *w, struct attrib *attrib, int tok) {
  (void)tok;
	union arg res;

	if (!ci->dst) return DOPE_ERR_PERM;

	if (!attrib)
		ERR(NO_SUCH_MEMBER, "attribute '%s' does not exist", attrib->name);

	if (!attrib->get)
		ERR(ATTR_R_PERM, "attribute '%s' is not readable", attrib->name);

	if (dope_streq(attrib->type, "float", 6)) {
		float (*float_get)(WIDGET *w) = (float (*)(WIDGET *))attrib->get;
		res.float_value = float_get(w);
	} else {
		res.pointer = attrib->get(w);
	}
	return convert_result(attrib->baseclass, &res, ci->dst, ci->dst_len);
}


static int exec_command(u32 app_id, const char *cmd, char *dst, int dst_len) {
	s32 i;
	WIDGET *w;
	struct widtype *w_type;
	struct attrib *attrib;
	struct method *meth;
	int ret, tok = 0;
	void *res_value = NULL;
	char *res_type  = NULL;
	int cmd_type;
	SCOPE *s;

	ci->dst     = dst;
	ci->dst_len = dst_len;
	ci->scope   = appman->get_rootscope(app_id);

	if (!(s = ci->scope)) return DOPE_ERR_PERM;

	ci->num_tok = tokenizer->parse(cmd, MAX_TOKENS, &ci->tok_off[0], &ci->tok_len[0]);
	for (i=0; i<ci->num_tok; i++) {
		ci->tokens[i] = (char *)(cmd + ci->tok_off[i]);
	}

	/* ignore empty commands */
	if (ci->num_tok <= 0) return 0;

	/* set default result string */
	if (ci->dst) ci->dst[0] = 0;

	cmd_type = get_command_type(ci, tok);

	/* resolve scope of the first command tokens */
	ret = resolve_scope(ci, ci->scope, tok, &s);
	if (ret < 0) return ret;
	tok += ret;

	if (cmd_type == CMD_TYPE_ASSIGNMENT) {
		SCOPE *ns;

		if ((tok + 1 < ci->num_tok)
		 && (ns = s->scope->get_subscope(s, ci->tokens[tok], ci->tok_len[tok]))
		 && (ci->tokens[tok + 1][0] == '.')) {
			s = ns;
			tok += 2;
		}

		if (ci->num_tok < tok + 4)
			ERR(UNCOMPLETE, "unexpected end of command");

		if (!dope_streq(ci->tokens[tok + 2], "new", ci->tok_len[tok + 2]))
			ERR(ILLEGAL_CMD, "unknown keyword '%s'", err_token(ci, tok + 2));

		w_type = hashtab->get_elem(widtypes, ci->tokens[tok + 3], ci->tok_len[tok + 3]);
		if (!w_type)
			ERR(UNKNOWN_VAR, "widget type '%s' does not exist", err_token(ci, 3));

		res_type  = w_type->ident;
		res_value = w_type->create();

		if (ci->tokens[tok + 4][0] != '(')
			ERR(LEFT_PAR, "missing left parenthesis");

		if (res_value)
			((WIDGET *)res_value)->gen->set_app_id((WIDGET *)res_value, app_id);

		/* set initial attributes */
		ret = exec_set(ci, (WIDGET *)res_value, w_type, tok + 4);

		/* check if something went wrong with setting the initial attributes */
		if (ret < 0) {

			/* destroy widget and return error code */
			((WIDGET *)res_value)->gen->dec_ref((WIDGET *)res_value);
			res_value = NULL;
			return ret;
		}

		/* assign method result to variable */
		if (res_type) {
			if (dope_streq(res_type, "Widget", 7)) {
				res_type = ((WIDGET *)res_value)->gen->get_type(res_value);
			}
			s->scope->set_var(s, res_type, ci->tokens[tok], ci->tok_len[tok], res_value);
		}
		if (dst) snprintf(dst, dst_len, "ok");
		return 0;
	}

	if ((cmd_type == CMD_TYPE_METHOD) || (cmd_type == CMD_TYPE_REQUEST)) {
		int res;

		/* determine widget and its type of the given variable symbol */
		res = get_variable(ci, s, tok, &w, &w_type);

		if (res < 0)
			ERR(UNKNOWN_VAR, "unknown variable '%s'", err_token(ci, tok));

		tok += res;
		if (tok >= ci->num_tok) ERR(UNCOMPLETE, "unexpected end of command");

		if (cmd_type == CMD_TYPE_METHOD) {

			/* get information structure of the method to call */
			meth = hashtab->get_elem(w_type->methods, ci->tokens[tok], ci->tok_len[tok]);

			if (meth) {
				return exec_function(ci, w, w_type, meth, tok + 1);
			} else if (dope_streq(ci->tokens[tok], "set", ci->tok_len[tok])) {
				return exec_set(ci, w, w_type, tok + 1);
			}
		}

		if (cmd_type == CMD_TYPE_REQUEST) {

			/* get widget attribute information structure */
			attrib = hashtab->get_elem(w_type->attribs, ci->tokens[tok], ci->tok_len[tok]);

			if (!attrib)
				ERR(NO_SUCH_MEMBER, "attribute '%s' does not exist", err_token(ci, tok));

			/* set attribute */
			return exec_attrib(ci, w, attrib, tok + 1);
		}

		ERR(NO_SUCH_MEMBER, "attribute or method '%s' does not exist", err_token(ci, tok));
	}

	ERR(ILLEGAL_CMD, "illegal command");
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct script_services services = {
	register_widget_type,
	register_widget_method,
	register_widget_attrib,
	exec_command,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_script(struct dope_services *d) {

	hashtab     = d->get_module("HashTable 1.0");
	appman      = d->get_module("ApplicationManager 1.0");
	tokenizer   = d->get_module("Tokenizer 1.0");

	widtypes = hashtab->create(WIDTYPE_HASHTAB_SIZE, WIDTYPE_HASH_CHARS);

	d->register_module("Script 1.0",&services);
	return 1;
}
