#include <fcntl.h>
#include <linux/dm-ioctl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "interpreter.h"
#include "primitives.h"

/*----------------------------------------------------------------*/

static value_t mk_c_string(const char *str)
{
	return mk_string(str, str + strlen(str));
}

static int open_control_file()
{
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "/dev/%s/%s", DM_DIR, DM_CONTROL_NODE);
	return open(buffer, O_RDWR | O_EXCL);
}

static void dm_version(struct interpreter *terp)
{
	struct dm_ioctl ctl;
	int r, fd;

	fd = open_control_file();
	if (fd < 0) {
		PUSH(mk_c_string("couldn't open dm control file"));
		return;
	}

	memset(&ctl, 0, sizeof(ctl));
	ctl.data_size = sizeof(ctl);
	ctl.data_start = sizeof(ctl);

	r = ioctl(fd, DM_VERSION, &ctl);
	if (r < 0) {
		PUSH(mk_c_string("ioctl call failed"));
		return;
	}

	{
		char buffer[128];
		snprintf(buffer, sizeof(buffer), "%u.%u.%u",
			 ctl.version[0], ctl.version[1], ctl.version[2]);
		PUSH(mk_c_string(buffer));
	}
}

/*----------------------------------------------------------------*/

void add_dm_primitives(struct interpreter *terp)
{
	add_primitive(terp, "dm-version", dm_version);
}

/*----------------------------------------------------------------*/
