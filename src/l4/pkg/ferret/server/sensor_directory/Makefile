# directories we need to know
PKGDIR       ?= ../..
L4DIR        ?=  $(PKGDIR)/../..

SYSTEMS       = x86-l4f

# source files
SERVERIDL     = ferret_dir.idl ferret_client.idl ferret_fpages.idl ferret_monitor.idl

SRC_C         = dir.c sensors.c server.c clients.c

MODE          = l4env_minimal
DEFAULT_RELOC = 0x002fd000

TARGET        = fer_sensor_dir
LIBS          = -lparsecmdline -lferret_init                     \
                -ll4vfs_name_server -ll4vfs_name_space_provider  \
                -ll4vfs_object_server-server                     \
                -ll4vfs_tree_helper -lferret_local_names

include $(L4DIR)/mk/prog.mk
