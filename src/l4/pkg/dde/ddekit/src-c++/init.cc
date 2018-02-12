/**
 * The functions regarding DDE/BSD initialization are found here.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 * \author Bjoern Doebel  <doebel@os.inf.tu-dresden.de>
 */
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/printf.h>

#include <l4/dde/dde.h>
#include "internal.h"

void DDEKit::init(void)
{
	DDEKit::init_mm();
	DDEKit::init_rm();
}

