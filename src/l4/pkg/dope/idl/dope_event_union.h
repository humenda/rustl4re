typedef struct dope_event_union {
	long _d;
	union {
		struct {
			char *cmd;
		} command;
		struct {
			long rel_x;
			long rel_y;
			long abs_x;
			long abs_y;
		} motion;
		struct {
			long code;
		} press;
		struct {
			long code;
		} release;
		long type;
	} _u;
} dope_event;

#define _dope_event_defined 1

struct dope_event_union *dope_event_union__alloc(void);
gpointer dope_event_union__free(gpointer m, gpointer d, CORBA_boolean free_strings);
gpointer dope_event__free(gpointer mem, gpointer dat, CORBA_boolean free_strings);
dope_event *dope_event__alloc(void);
