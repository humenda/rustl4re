#!/bin/sh

set -e
set -x

SRC_BASE=$1
OBJ_BASE=$2

GEN_H_DIR="$OBJ_BASE/include/bsd/generated"
GEN_C_DIR="$OBJ_BASE/common/bsd/generated"

if [ ! -e "$GEN_H_DIR" ]; then
	mkdir -p "$GEN_H_DIR"
fi
cd "$GEN_H_DIR"
awk -f "$SRC_BASE/common/bsd/contrib/tools/makeobjops.awk" "$SRC_BASE/common/bsd/contrib/kern/bus_if.m" -h
awk -f "$SRC_BASE/common/bsd/contrib/tools/makeobjops.awk" "$SRC_BASE/common/bsd/contrib/kern/device_if.m" -h
awk -f "$SRC_BASE/common/bsd/contrib/tools/makeobjops.awk" "$SRC_BASE/common/bsd/contrib/isa/isa_if.m" -h
awk -f "$SRC_BASE/common/bsd/contrib/tools/makeobjops.awk" "$SRC_BASE/common/bsd/contrib/dev/pci/pci_if.m" -h
awk -f "$SRC_BASE/common/bsd/contrib/tools/makeobjops.awk" "$SRC_BASE/common/bsd/contrib/dev/pci/pcib_if.m" -h
awk -f "$SRC_BASE/common/bsd/contrib/tools/makeobjops.awk" "$SRC_BASE/common/bsd/contrib/dev/acpica/acpi_if.m" -h
awk -f "$SRC_BASE/common/bsd/contrib/tools/vnode_if.awk"   "$SRC_BASE/common/bsd/contrib/kern/vnode_if.src" -h

if [ ! -e "$GEN_C_DIR" ]; then
	mkdir -p "$GEN_C_DIR"
fi
cd "$GEN_C_DIR"
awk -f "$SRC_BASE/common/bsd/contrib/tools/makeobjops.awk" "$SRC_BASE/common/bsd/contrib/kern/bus_if.m" -c
awk -f "$SRC_BASE/common/bsd/contrib/tools/makeobjops.awk" "$SRC_BASE/common/bsd/contrib/kern/device_if.m" -c
awk -f "$SRC_BASE/common/bsd/contrib/tools/makeobjops.awk" "$SRC_BASE/common/bsd/contrib/isa/isa_if.m" -c
awk -f "$SRC_BASE/common/bsd/contrib/tools/makeobjops.awk" "$SRC_BASE/common/bsd/contrib/dev/pci/pci_if.m" -c
awk -f "$SRC_BASE/common/bsd/contrib/tools/makeobjops.awk" "$SRC_BASE/common/bsd/contrib/dev/pci/pcib_if.m" -c
awk -f "$SRC_BASE/common/bsd/contrib/tools/makeobjops.awk" "$SRC_BASE/common/bsd/contrib/dev/acpica/acpi_if.m" -c
awk -f "$SRC_BASE/common/bsd/contrib/tools/vnode_if.awk"   "$SRC_BASE/common/bsd/contrib/kern/vnode_if.src" -c
awk -f "$SRC_BASE/common/bsd/contrib/conf/majors.awk"      "$SRC_BASE/common/bsd/contrib/conf/majors" > majors.c
