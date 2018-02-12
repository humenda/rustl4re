#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <geom/geom.h>
#include <geom/geom_disk.h>

#include <l4/dde/ddekit/panic.h>

#include <l4/dde/fbsd/ata.h>

#include "types.h"

#define dbg_this 0

MALLOC_DEFINE(M_DAD, "dad disks", "dad disks")

static void issue_bio(struct bio *bp, dad_disk_t *dsk) {
	g_io_request(bp, dsk_di(dsk)->geom_consumer);
}

static struct g_geom *l4ata_taste(struct g_class *mp, struct g_provider *pp, int flags) {
	int err;
	struct g_geom *gp;
	struct g_consumer *cp;

	g_trace(G_T_TOPOLOGY, "l4ata_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	gp = g_new_geomf(mp, "%s.%s", mp->name, pp->name);
	cp = g_new_consumer(gp);
	if (!gp || !cp) goto err_exit_1;
	g_attach(cp, pp);
	err = g_access(cp, 1, 0, 0);
	if (err) goto err_exit_2;

	if (dbg_this)
		printf("l4ata_taste: provider=\"%s\" medsz=%lld sectsz=%d geom=\"%s\"\n", pp->name, pp->mediasize, pp->sectorsize, pp->geom->name);

	if ( (pp->mediasize > 0) && (pp->sectorsize > 0) ) {
		dad_disk_t *dsk;
		dde_disk_info_t *di;

		err = g_access(cp, 0, 1, 1);
		if (err) goto err_exit_3;

		di = (dde_disk_info_t *) malloc(sizeof(*di), M_DAD, M_WAITOK|M_ZERO);
		if (!di) {
			dde_debug("error allocating dde_disk_info_t");
			goto err_exit_4;
		}
		di->issue_bio = issue_bio;
		di->geom_consumer = cp;

		dsk = (dad_disk_t *) malloc(sizeof(*dsk), M_DAD, M_WAITOK|M_ZERO);
		if (!dsk) {
			dde_debug("error allocating dad_disk_t");
			free(di, M_DAD);
			goto err_exit_4;
		}
		dsk->sectsize      = pp->sectorsize;
		dsk->sects         = pp->mediasize/pp->sectorsize;
		dsk->name          = cp->provider->name;
		dsk->client_priv   = 0; // for client use only
		dsk->dde_priv      = di;

		dad_announce_disk(dsk);

		return gp;
	}
	goto err_exit_3;

err_exit_4:
	g_access(cp, 0, -1, -1);
err_exit_3:
	g_access(cp, -1, 0, 0);
err_exit_2:
	g_detach(cp);
err_exit_1:
	if (cp) g_destroy_consumer(cp);
	if (gp) g_destroy_geom(gp);
	return NULL;
}

static void l4ata_orphan(struct g_consumer *cp) {
}

/**
 * Declare a geom class so geom will be initialized.
 */
static struct g_class l4ata_class = {
	.name    = "l4ata",
	.version = G_VERSION,
	.taste   = l4ata_taste,
	.orphan  = l4ata_orphan,
};
DECLARE_GEOM_CLASS(l4ata_class, l4ata);

