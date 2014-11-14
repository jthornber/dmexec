#include <fcntl.h>
#include <linux/dm-ioctl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "primitives.h"
#include "string_type.h"
#include "utils.h"
#include "vm.h"

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

static void dm_ioctl(struct vm *vm, int request, void *payload)
{
	int r, fd;

	fd = open_control_file();
	if (fd < 0)
		error("couldn't open dm control file");

	r = ioctl(fd, request, payload);
	if (r < 0)
		error("ioctl call failed");
}

static void dm_version(struct vm *vm)
{
	char buffer[128];
	struct dm_ioctl ctl;

	init_ctl(&ctl, sizeof(ctl));
	dm_ioctl(vm, DM_VERSION, &ctl);

	snprintf(buffer, sizeof(buffer), "%u.%u.%u",
		 ctl.version[0], ctl.version[1], ctl.version[2]);
	PUSH(mk_c_string(buffer));
}

static void dm_remove_all(struct vm *vm)
{
	struct dm_ioctl ctl;

	init_ctl(&ctl, sizeof(ctl));
	dm_ioctl(vm, DM_REMOVE_ALL, &ctl);
}

static void dm_list_devices(struct vm *vm)
{
	char buffer[8192];	/* FIXME: what if this buffer isn't big enough? */
	struct dm_ioctl *ctl = (struct dm_ioctl *) buffer;
	struct dm_name_list *nl;
	value_t results = mk_ref(array_create());

	init_ctl(ctl, sizeof(buffer));
	dm_ioctl(vm, DM_LIST_DEVICES, ctl);

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

static void copy_param(const char *param, char *dest, size_t max, struct string *src)
{
	if (string_len(src) >= max)
		error("<%s> string too long for param destination", param);

	memcpy(dest, src->b, string_len(src));
}

static void dm_create(struct vm *vm)
{
	struct dm_ioctl ctl;
	struct string *uuid = as_type(STRING, POP());
	struct string *name = as_type(STRING, POP());

	init_ctl(&ctl, sizeof(ctl));
	copy_param("name", ctl.name, DM_NAME_LEN, name);
	copy_param("uuid", ctl.uuid, DM_UUID_LEN, uuid);
	dm_ioctl(vm, DM_DEV_CREATE, &ctl);
}

static void dev_cmd(struct vm *vm, int request, unsigned flags)
{
	struct dm_ioctl ctl;
	struct string *name = as_type(STRING, POP());

	init_ctl(&ctl, sizeof(ctl));
	ctl.flags = flags;
	copy_param("name", ctl.name, DM_NAME_LEN, name);
	dm_ioctl(vm, request, &ctl);
}

static void dm_remove(struct vm *vm)
{
	dev_cmd(vm, DM_DEV_REMOVE, 0);
}

static void dm_suspend(struct vm *vm)
{
	dev_cmd(vm, DM_DEV_SUSPEND, DM_SUSPEND_FLAG);
}

static void dm_resume(struct vm *vm)
{
	dev_cmd(vm, DM_DEV_SUSPEND, 0);
}

static void dm_clear(struct vm *vm)
{
	dev_cmd(vm, DM_TABLE_CLEAR, 0);
}

static void dm_load(struct vm *vm)
{
	char buffer[8192];
	struct string *name = as_type(STRING, POP());
	struct array *table = as_type(ARRAY, POP());
	struct dm_ioctl *ctl = (struct dm_ioctl *) buffer;
	struct dm_target_spec *spec;
	uint64_t current_sector = 0;

	init_ctl(ctl, sizeof(buffer));
	ctl->target_count = table->nr_elts;
	copy_param("name", ctl->name, DM_NAME_LEN, name);

	spec = (struct dm_target_spec *) (ctl + 1);

	for (unsigned i = 0; i < table->nr_elts; i++) {
		struct array *target = as_type(ARRAY, array_get(table, i));

		if (target->nr_elts != 3)
			error("<target> does not have 3 elements");

		{
			// Each entry in the table should have a fixnum length,
			// followed by target type and target ctr string.  Start
			// sectors are inferred.
			int len = as_fixnum(array_get(target, 0));
			struct string *tt = as_type(STRING, array_get(target, 1));
			struct string *target_ctr = as_type(STRING, array_get(target, 2));

			spec->sector_start = current_sector;
			current_sector += len;
			spec->length = len;
			spec->status = 0;

			copy_param("target type", spec->target_type, DM_MAX_TYPE_NAME, tt);
			memcpy(spec + 1, target_ctr->b, string_len(target_ctr));
			((char *) (spec + 1))[string_len(target_ctr)] = '\0';

			spec->next = sizeof(*spec) + round_up(string_len(target_ctr) + 1, 8);
			spec = (struct dm_target_spec *) (((char *) spec) + spec->next);
		}
	}

	// FIXME: no bounds checking
	dm_ioctl(vm, DM_TABLE_LOAD, ctl);
}



/*----------------------------------------------------------------*/

void def_dm_primitives(struct vm *vm)
{
	def_primitive(vm, "dm-version", dm_version);
	def_primitive(vm, "dm-remove-all", dm_remove_all);
	def_primitive(vm, "dm-list-devices", dm_list_devices);
	def_primitive(vm, "dm-create", dm_create);
	def_primitive(vm, "dm-remove", dm_remove);
	def_primitive(vm, "dm-suspend", dm_suspend);
	def_primitive(vm, "dm-resume", dm_resume);
	def_primitive(vm, "dm-clear", dm_clear);
	def_primitive(vm, "dm-load", dm_load);
}

/*----------------------------------------------------------------*/
