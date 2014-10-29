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

static void init_ctl(struct dm_ioctl *ctl)
{
	memset(ctl, 0, sizeof(*ctl));
	ctl->version[0] = DM_VERSION_MAJOR;
	ctl->version[1] = DM_VERSION_MINOR;
	ctl->version[2] = DM_VERSION_PATCHLEVEL;
	ctl->data_size = sizeof(*ctl);
	ctl->data_start = sizeof(*ctl);
}

static void dm_ioctl(struct interpreter *terp, int request, void *payload)
{
	int r, fd;

	fd = open_control_file();
	if (fd < 0) {
		PUSH(mk_c_string("couldn't open dm control file"));
		return;
	}

	r = ioctl(fd, DM_VERSION, payload);
	if (r < 0) {
		PUSH(mk_c_string("ioctl call failed"));
		return;
	}
}

static void dm_version(struct interpreter *terp)
{
	char buffer[128];
	struct dm_ioctl ctl;

	init_ctl(&ctl);
	dm_ioctl(terp, DM_VERSION, &ctl);
	snprintf(buffer, sizeof(buffer), "%u.%u.%u",
		 ctl.version[0], ctl.version[1], ctl.version[2]);
	PUSH(mk_c_string(buffer));
}

static void dm_remove_all(struct interpreter *terp)
{
	struct dm_ioctl ctl;

	init_ctl(&ctl);
	dm_ioctl(terp, DM_REMOVE_ALL, &ctl);
	PUSH(mk_c_string("ok"));
}

/*----------------------------------------------------------------*/

void add_dm_primitives(struct interpreter *terp)
{
	add_primitive(terp, "dm-version", dm_version);
	add_primitive(terp, "dm-remove-all", dm_remove_all);
}

/*----------------------------------------------------------------*/
