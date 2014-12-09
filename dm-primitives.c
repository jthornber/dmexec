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

// FIXME: any resources, such as the fd for the ctl file must be cleaned up
// when an error is thrown.  Opaque 'finalisable/resource' type.

static value_t mk_c_string(char *str)
{
	return mk_ref(string_clone_cstr(str));
}

static int open_control_file(void)
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

static void dm_ioctl(int request, void *payload)
{
	int r, fd;

	fd = open_control_file();
	if (fd < 0)
		error("couldn't open dm control file");

	r = ioctl(fd, request, payload);
	if (r < 0)
		error("ioctl call failed");
}

static void dm_version(void)
{
	char buffer[128];
	struct dm_ioctl ctl;

	init_ctl(&ctl, sizeof(ctl));
	dm_ioctl(DM_VERSION, &ctl);

	snprintf(buffer, sizeof(buffer), "%u.%u.%u",
		 ctl.version[0], ctl.version[1], ctl.version[2]);
	PUSH(mk_c_string(buffer));
	inc_pc();
}

static void dm_remove_all(void)
{
	struct dm_ioctl ctl;

	init_ctl(&ctl, sizeof(ctl));
	dm_ioctl(DM_REMOVE_ALL, &ctl);
	inc_pc();
}

static void dm_list_devices(void)
{
	char buffer[8192];	/* FIXME: what if this buffer isn't big enough? */
	struct dm_ioctl *ctl = (struct dm_ioctl *) buffer;
	struct dm_name_list *nl;
	value_t results = mk_ref(array_create());

	init_ctl(ctl, sizeof(buffer));
	dm_ioctl(DM_LIST_DEVICES, ctl);

	if (ctl->flags & DM_BUFFER_FULL_FLAG) {
		PUSH(mk_c_string("buffer full flag set"));
		inc_pc();
		return;
	}

	nl = (struct dm_name_list *) ((void *) ctl + ctl->data_start);
	if (nl->dev) {
		while (true) {
			array_push(as_ref(results), mk_c_string(nl->name));

			if (!nl->next)
				break;

			nl = (struct dm_name_list*) (((char *) nl) + nl->next);
		}
	}

	PUSH(results);
	inc_pc();
}

static void copy_param(const char *param, char *dest, size_t max, struct string *src)
{
	if (string_len(src) >= max)
		error("<%s> string too long for param destination", param);

	memcpy(dest, src->b, string_len(src));
}

static void dm_create(void)
{
	struct dm_ioctl ctl;
	struct string *uuid = as_type(STRING, POP());
	struct string *name = as_type(STRING, POP());

	init_ctl(&ctl, sizeof(ctl));
	copy_param("name", ctl.name, DM_NAME_LEN, name);
	copy_param("uuid", ctl.uuid, DM_UUID_LEN, uuid);
	dm_ioctl(DM_DEV_CREATE, &ctl);
	inc_pc();
}

static void dev_cmd(int request, unsigned flags)
{
	struct dm_ioctl ctl;
	struct string *name = as_type(STRING, POP());

	init_ctl(&ctl, sizeof(ctl));
	ctl.flags = flags;
	copy_param("name", ctl.name, DM_NAME_LEN, name);
	dm_ioctl(request, &ctl);
	inc_pc();
}

static void dm_remove(void)
{
	dev_cmd(DM_DEV_REMOVE, 0);
}

static void dm_suspend(void)
{
	dev_cmd(DM_DEV_SUSPEND, DM_SUSPEND_FLAG);
}

static void dm_resume(void)
{
	dev_cmd(DM_DEV_SUSPEND, 0);
}

static void dm_clear(void)
{
	dev_cmd(DM_TABLE_CLEAR, 0);
}

static void dm_load(void)
{
	char buffer[8192];
	struct array *table = as_type(ARRAY, POP());
	struct string *name = as_type(STRING, POP());
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
	dm_ioctl(DM_TABLE_LOAD, ctl);
	inc_pc();
}

static void status_cmd(unsigned flags)
{
	char buffer[8192];
	struct string *name = as_type(STRING, POP());
	struct array *table = array_create();
	struct dm_ioctl *ctl = (struct dm_ioctl *) buffer;
	struct dm_target_spec *spec;
	char *spec_start;

	init_ctl(ctl, sizeof(buffer));
	ctl->flags = flags;
	ctl->target_count = 0;
	copy_param("name", ctl->name, DM_NAME_LEN, name);

	dm_ioctl(DM_TABLE_STATUS, ctl);

	if (ctl->flags & DM_BUFFER_FULL_FLAG)
		error("dm-ioctl buffer too small");

	spec = (struct dm_target_spec *) (ctl + 1);
	spec_start = (char *) spec;
	for (unsigned i = 0; i < ctl->target_count; i++) {
		struct array *target = array_create();

		target = array_push(target, mk_fixnum(spec->length));
		target = array_push(target, mk_ref(string_clone_cstr(spec->target_type)));
		target = array_push(target, mk_ref(string_clone_cstr((char *) (spec + 1))));

		// FIXME: no bounds checking
		spec = (struct dm_target_spec *) (spec_start + spec->next);
		table = array_push(table, mk_ref(target));
	}

	PUSH(mk_ref(table));
	inc_pc();
}

static void dm_table(void)
{
	status_cmd(DM_STATUS_TABLE_FLAG);
}

static void dm_status(void)
{
	status_cmd(0);
}

static void dm_message()
{
	char buffer[8192];
	struct string *txt = as_type(STRING, POP());
	int sector = as_fixnum(POP());
	struct string *name = as_type(STRING, POP());
	struct dm_ioctl *ctl = (struct dm_ioctl *) buffer;
	struct dm_target_msg *msg = (struct dm_target_msg *) (ctl + 1);

	init_ctl(ctl, sizeof(buffer));
	copy_param("name", ctl->name, DM_NAME_LEN, name);
	msg->sector = sector;

	// FIXME: bounds checking
	memcpy(msg->message, txt->b, string_len(txt));
	dm_ioctl(DM_TARGET_MSG, ctl);
	inc_pc();
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
	def_primitive(vm, "dm-table", dm_table);
	def_primitive(vm, "dm-status", dm_status);
	def_primitive(vm, "dm-message", dm_message);
}

/*----------------------------------------------------------------*/
