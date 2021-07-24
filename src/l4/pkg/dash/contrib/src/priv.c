#include <unistd.h>

#include "priv.h"
#include "var.h"

uid_t uid;
gid_t gid;

void setprivileged(int on)
{
	static int is_privileged = 1;
	if (is_privileged == on)
		return;

	is_privileged = on;

	/*
	 * To limit bogus system(3) or popen(3) calls in setuid binaries, require
	 * -p flag to work in this situation.
	 */
	if (!on && (uid != geteuid() || gid != getegid())) {
		setuid(uid);
		setgid(gid);
		/* PS1 might need to be changed accordingly. */
		choose_ps1();
	}
}
