#ifndef __ASM_L4__SERVER__FDX_SRC_H__
#define __ASM_L4__SERVER__FDX_SRC_H__

#include <asm/server/server.h>
#include <l4/libfdx/fdx-defs.h>

struct l4fdx_client;

enum l4x_fdx_srv_factory_data_flags {
	L4X_FDX_SRV_FACTORY_HAS_UID            = 1 << 0,
	L4X_FDX_SRV_FACTORY_HAS_GID            = 1 << 1,
	L4X_FDX_SRV_FACTORY_HAS_OPENFLAGS_MASK = 1 << 2,
	L4X_FDX_SRV_FACTORY_HAS_BASEPATH       = 1 << 3,
	L4X_FDX_SRV_FACTORY_HAS_FILTERPATH     = 1 << 4,
	L4X_FDX_SRV_FACTORY_HAS_FLAG_NOGROW    = 1 << 5,
	L4X_FDX_SRV_FACTORY_HAS_CLIENTNAME     = 1 << 6,
};

struct l4x_fdx_srv_factory_create_data {
	unsigned opt_flags;

	unsigned uid;
	unsigned gid;
	unsigned openflags_mask;
	const char *basepath;
	const char *filterpath;
	const char *clientname;
	unsigned basepath_len;
	unsigned filterpath_len;
	unsigned clientname_len;
};

struct l4x_fdx_srv_factory_ops {
	L4_CV int (*create)(struct l4x_fdx_srv_factory_create_data *data,
	                    l4_cap_idx_t *client_cap);
};

C_FUNC int
l4x_fdx_factory_create(l4_cap_idx_t thread,
                       struct l4x_fdx_srv_factory_ops *ops,
                       void *objmem);

C_FUNC unsigned
l4x_fdx_factory_objsize(void);

// ---------

typedef struct {
	struct l4fdx_client *client;
} l4fdx_srv_struct;
typedef l4fdx_srv_struct *l4fdx_srv_obj;

struct l4x_fdx_srv_ops {
	L4_CV int (*open)(l4fdx_srv_obj srv_obj, unsigned client_req_id,
                          const char *path, unsigned len,
                          int flags, unsigned mode);
	L4_CV int (*read)(l4fdx_srv_obj srv_obj, unsigned client_req_id,
			  int, unsigned long long, size_t, unsigned shm_off);
	L4_CV int (*write)(l4fdx_srv_obj srv_obj, unsigned client_req_id,
                           int, unsigned long long, size_t, unsigned shm_off);
	L4_CV int (*fstat)(l4fdx_srv_obj srv_obj, unsigned client_req_id,
	                   int, unsigned shm_off);
	L4_CV int (*close)(l4fdx_srv_obj srv_obj, unsigned client_req_id, int);
};

struct l4x_fdx_srv_data {
	unsigned long shm_base;
	unsigned shm_size;
};

C_FUNC unsigned
l4x_fdx_srv_objsize(void);

C_FUNC l4fdx_srv_obj
l4x_fdx_srv_create_name(l4_cap_idx_t thread, const char *capname,
                        struct l4x_fdx_srv_ops *ops,
		        struct l4fdx_client *client,
                        void *objmem);

C_FUNC l4fdx_srv_obj
l4x_fdx_srv_create(l4_cap_idx_t thread,
                   struct l4x_fdx_srv_ops *ops,
		   struct l4fdx_client *client,
                   void *objmem, l4_cap_idx_t *client_cap);

C_FUNC void
l4x_fdx_srv_add_event(l4fdx_srv_obj fdxobjp, struct l4fdx_result_t *e);

C_FUNC void
l4x_fdx_srv_trigger(l4fdx_srv_obj fdxobjp);

C_FUNC struct l4x_fdx_srv_data *
l4x_fdx_srv_get_srv_data(l4fdx_srv_obj fdxobjp);

#endif /* ! __ASM_L4__SERVER__FDX_SRC_H__ */
