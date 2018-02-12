/*
 * \brief   DOpE Scope widget module
 * \date    2002-05-15
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * A Scope is a namespace.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

struct scope;
#define WIDGET struct scope

#include "dopestd.h"
#include "hashtab.h"
#include "scope.h"
#include "script.h"
#include "widman.h"
#include "appman.h"
#include "widget_data.h"
#include "widget_help.h"

#define VAR_HASHTAB_SIZE  32    /* applications variable hash table config */
#define VAR_HASH_CHARS     5

static struct hashtab_services *hashtab;
static struct widman_services  *widman;
static struct script_services  *script;
static struct appman_services  *appman;

struct scope_data {
	HASHTAB *vars;
};

struct variable {
	char   *name;   /* variable name  */
	char   *type;   /* variable type  */
	WIDGET *value;  /* variable value */
};

int init_scope(struct dope_services *d);


/******************************
 *** GENERAL WIDGET METHODS ***
 ******************************/

/*** DEALLOCATE SCOPE DATA ***/
static void scope_free_data(SCOPE *s) {
	struct variable *var;
	WIDGET *w;

	/* delete variables and dissolve the references to their widgets */
	var = hashtab->get_first(s->sd->vars);
	while (var) {
		if ((w = var->value))
			w->gen->dec_ref(w);
		var = hashtab->get_next(s->sd->vars, var);
	}

	/* now, destroy the hash table */
	hashtab->dec_ref(s->sd->vars);
}


/*** RETURN WIDGET TYPE IDENTIFIER ***/
static char *scope_get_type(SCOPE *v) {
  (void)v;
	return "Scope";
}


/*******************************
 *** SCOPE SPECIFIC METHODS ***
 *******************************/

/*** REDEFINE A VARIABLE OR CREATE A NEW ONE IF NEEDED ***
 *
 * The type argument is not replicated. The caller must ensure that the
 * type string exists as long as the variable exists!
 */
static int scope_set_var(SCOPE *s, char *type, char *name, int len, WIDGET *value) {
	struct variable *v;

	/* does variable already exists? */
	v = hashtab->get_elem(s->sd->vars, name, len);
	
	/* create a new variable */
	if (!v) {
		int name_len = MIN(strlen(name), (unsigned)len);
		v = zalloc(sizeof(struct variable) + strlen(name) + 2);
		if (!v) return -1;
		v->name = (char *)((adr)v + sizeof(struct variable));
		memcpy(v->name, name, name_len);
		v->name[name_len] = 0;
		INFO(printf("scope_set_var: variable %s\n", v->name));
		hashtab->add_elem(s->sd->vars, v->name, v);
	} else {

		/* loose the reference to the old content */
		if (v->value)
			v->value->gen->dec_ref(v->value);
	}
	v->type  = type;
	v->value = value;
	return 0;
}


/*** REQUEST THE VALUE OF A VARIABLE ***/
static WIDGET *scope_get_var(SCOPE *s, char *name, int len) {
	struct variable *v = hashtab->get_elem(s->sd->vars, name, len);
	return v ? v->value : NULL;
}


/*** REQUEST THE TYPE OF A VARIABLE ***/
static char *scope_get_vartype(SCOPE *s, char *name, int len) {
	struct variable *v = hashtab->get_elem(s->sd->vars, name, len);
	return v ? v->type : NULL;
}


/*** REQUEST SUBSCOPE WITH THE SPECIFIED NAME ***/
static SCOPE *scope_get_subscope(SCOPE *s, char *name, int len) {
	struct variable *v = hashtab->get_elem(s->sd->vars, name, len);

	if (!v || !v->type || !v->value) return NULL;
	if (!dope_streq(v->type, "Scope", 256)) return NULL;

	return v->value;
}


/*** IMPORT SCOPE OF ANOTHER APPLICATION ***/
static int scope_voodoo(SCOPE *s, char *app_name) {
	SCOPE *rs;
	int app_id;
	
	/* resolve application root scope to rip off */
	app_id = appman->app_id_of_name(app_name);
	if (app_id < 0) return 0;
	rs = appman->get_rootscope(app_id);
	if (!rs) return 0;

	/* make scope use the same hashtab as the requested application */
	hashtab->dec_ref(s->sd->vars);
	hashtab->inc_ref(rs->sd->vars);
	s->sd->vars = rs->sd->vars;

	return 1;
}


static struct widget_methods gen_methods;
static struct scope_methods scope_methods = {
	scope_set_var,
	scope_get_var,
	scope_get_vartype,
	scope_get_subscope,
};



/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static SCOPE *create(void) {

	SCOPE *new = ALLOC_WIDGET(struct scope);
	SET_WIDGET_DEFAULTS(new, struct scope, &scope_methods);

	/* create hash table to store the variables of the scope */
	new->sd->vars = hashtab->create(VAR_HASHTAB_SIZE, VAR_HASH_CHARS);
	if (!new->sd->vars) {
		free(new);
		return NULL;
	}
	return new;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct scope_services services = {
	create
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

static void build_script_lang(void) {
	void *widtype = script->reg_widget_type("Scope", (void *(*)(void))create);
	script->reg_widget_method(widtype, "long voodoo(string appname)", &scope_voodoo);
	widman->build_script_lang(widtype, &gen_methods);
}


int init_scope(struct dope_services *d) {

	widman  = d->get_module("WidgetManager 1.0");
	script  = d->get_module("Script 1.0");
	hashtab = d->get_module("HashTable 1.0");
	appman  = d->get_module("ApplicationManager 1.0");

	/* define general widget functions */
	widman->default_widget_methods(&gen_methods);

	gen_methods.get_type  = scope_get_type;
	gen_methods.free_data = scope_free_data;

	build_script_lang();

	d->register_module("Scope 1.0",&services);
	return 1;
}
