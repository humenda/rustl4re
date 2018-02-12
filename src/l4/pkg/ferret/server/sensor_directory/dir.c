/**
 * \file   ferret/server/sensor_directory/dir.c
 * \brief  Directory server, serves as well known instance and
 *         naming service for sensors
 *
 * \date   04/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
//#include <l4/dm_generic/dm_generic.h>
//#include <l4/dm_phys/dm_phys.h>
#include <l4/l4vfs/tree_helper.h>
#include <l4/l4vfs/name_server.h>
#include <l4/l4vfs/name_space_provider.h>
#include <l4/log/l4log.h>
#include <l4/names/libnames.h>
#include <l4/util/l4_macros.h>
#include <l4/util/parse_cmd.h>
#include <l4/util/util.h>

#include <l4/ferret/ferret_dir-server.h>
#include <l4/ferret/sensors/tbuf.h>

#include <stdlib.h>

#include "sensors.h"

char LOG_tag[9] = FERRET_DIR_NAME;
int verbose;
int reg_l4vfs;
l4vfs_th_node_t * root;

#define MIN ((a)<(b)?(a):(b))

int main(int argc, const char *argv[])
{
    object_id_t root_id;
    l4dm_dataspace_t ds = L4DM_INVALID_DATASPACE;
    int i, ret;
    sensor_entry_t * se;
    CORBA_Server_Environment env = dice_default_server_environment;

    if (parse_cmdline(&argc, &argv,

                      'v', "verbose",
                      "be verbose by dumping progress information to via log",
                      PARSE_CMD_SWITCH, 1, &verbose,

                      'l', "l4vfs", "register volume_id at L4VFS' name_server",
                      PARSE_CMD_SWITCH, 1, &reg_l4vfs,

                      0, 0) != 0)
    {
        return 1;
    }

    // create root node for tree_helper
    root = l4vfs_th_new_node("[ferret root]", L4VFS_TH_TYPE_DIRECTORY,
                             NULL, NULL);

    // register at L4VFS' name_server

    if (reg_l4vfs)
    {
        l4_threadid_t ns;
        root_id.volume_id = FERRET_L4VFS_VOLUME_ID;
        root_id.object_id = 0;

        ns = l4vfs_get_name_server_threadid();
        if (l4_is_invalid_id(ns))
        {
            exit(1);
        }
        LOGd(verbose, "Got name_server thread_id ...");
        for (i = 0; i < 3; i++)
        {
            ret = l4vfs_register_volume(ns, l4_myself(), root_id);
            // register ok --> done
            if (ret == 0)
                break;
            else
                LOG("Error registering at name_server: %d, retrying ...", ret);
            l4_sleep(500);
        }
        if (ret)
        {
            LOG("Could not register at name_server, gave up!");
            exit(1);
        }
        LOGd(verbose, "Registered my volume_id at name_server.");
    }

    // register special sensors ...

    // trace buffer
    se = sensors_new_entry(&ds, FERRET_TBUF_MAJOR, FERRET_TBUF_MINOR,
                           0, FERRET_TBUF, FERRET_PERMANENT, "", NULL);
    if (! se)
    {
        LOG("Could not register trace buffer as sensor, ignored!");
    }

    env.malloc = (dice_malloc_func)malloc;
    env.free = (dice_free_func)free;

    LOGd(verbose, "Registering at names ...");
    names_register(FERRET_DIR_NAME);

    LOGd(verbose, "Starting server loop ...");
    ferret_dir_server_loop(&env);
}
