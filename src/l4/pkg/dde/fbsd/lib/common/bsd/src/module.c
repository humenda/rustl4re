/**
 * Helper function for MODULE_METADATA needed by DECLARE_MODULE.
 * Written from scratch.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */
#include <sys/types.h>
#include <sys/module.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <dde_fbsd/module.h>

void module_register_metadata(void *data) {
	struct mod_metadata *metadata;
	moduledata_t *moddata;
	int err;

	metadata = (struct mod_metadata *) data;

	if (metadata->md_type != MDT_MODULE)
		return;

	moddata = (moduledata_t *) metadata->md_data;

	err = module_register(moddata, /*file*/NULL);
	if (err)
		printf("error registering module \"%s\": %d\n", moddata->name, err);
}
