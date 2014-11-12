#include <fcntl.h>
#include <linux/dm-ioctl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "vm.h"
#include "primitives.h"

/*----------------------------------------------------------------*/

static value_t mk_c_string(char *str)
{
	return mk_ref(string_clone_cstr(str));
}

static int open_control_file()
{
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "/dev/%s/%s", DM_DIR, DM_CONTROL_NODE);
	return open(buffer, O_RDWR | O_EXCL);
}

static void init_ctl(struct dm_ioctl *ctl, size_t data_size)
{
	memset(ctl, 0, sizeof(*ctl));
	ctl->version[0] = DM_VERSION_MAJOR;
	ctl->version[1] = DM_VERSION_MINOR;
	ctl->version[2] = DM_VERSION_PATCHLEVEL;
	ctl->data_size = data_size;
	ctl->data_start = sizeof(*ctl);
}

static int dm_ioctl(struct vm *vm, int request, void *payload)
{
	int r, fd;

	fd = open_control_file();
	if (fd < 0) {
		PUSH(mk_c_string("couldn't open dm control file"));
		return fd;
	}

	r = ioctl(fd, DM_VERSION, payload);
	if (r < 0)
		PUSH(mk_c_string("ioctl call failed"));

	return r;
}

static void dm_version(struct vm *vm)
{
	char buffer[128];
	struct dm_ioctl ctl;

	init_ctl(&ctl, sizeof(ctl));
	if (dm_ioctl(vm, DM_VERSION, &ctl) < 0)
		return;

	snprintf(buffer, sizeof(buffer), "%u.%u.%u",
		 ctl.version[0], ctl.version[1], ctl.version[2]);
	PUSH(mk_c_string(buffer));
}

static void dm_remove_all(struct vm *vm)
{
	struct dm_ioctl ctl;

	init_ctl(&ctl, sizeof(ctl));
	if (dm_ioctl(vm, DM_REMOVE_ALL, &ctl) < 0)
		return;

	PUSH(mk_c_string("ok"));
}

static void dm_list_devices(struct vm *vm)
{
	char buffer[8192];	/* FIXME: what if this buffer isn't big enough? */
	struct dm_ioctl *ctl = (struct dm_ioctl *) buffer;
	struct dm_name_list *nl;
	value_t results = mk_ref(array_create());

	init_ctl(ctl, sizeof(buffer));
	if (dm_ioctl(vm, DM_LIST_DEVICES, ctl) < 0)
		return;

	if (ctl->flags & DM_BUFFER_FULL_FLAG) {
		PUSH(mk_c_string("buffer full flag set"));
		return;
	}

	nl = (struct dm_name_list *) ((void *) ctl + ctl->data_start);
	if (nl->dev) {
		while (true) {
			fprintf(stderr, nl->name);
			array_push(as_ref(results), mk_c_string(nl->name));

			if (!nl->next)
				break;

			nl = (struct dm_name_list*) (((char *) nl) + nl->next);
		}
	} else {
		fprintf(stderr, "nl->dev = 0, name = %s\n", nl->name);
	}

	PUSH(results);
}

/*----------------------------------------------------------------*/

void def_dm_primitives(struct vm *vm)
{
	def_primitive(vm, "dm-version", dm_version);
	def_primitive(vm, "dm-remove-all", dm_remove_all);
	def_primitive(vm, "dm-list-devices", dm_list_devices);
}

/*----------------------------------------------------------------*/
