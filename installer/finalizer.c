/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iago.h>
#include <iago_util.h>
#include "iago_private.h"

static void finalizer_cli(void)
{
	// sdcard device node, append to partitions as 'special'
}

static bool write_props_cb(void *k, void *v, void *context)
{
	int propsfd = (int)context;
	char *key = k;
	char *value = v;

	put_string(propsfd, "%s=%s\n", key, value);
	return true;
}


static void write_install_props(void)
{
	int propsfd;

	propsfd = xopen("/mnt/install/install.prop", O_WRONLY | O_CREAT);
	hashmapForEach(ictx.iprops, write_props_cb, (void *)propsfd);
	xclose(propsfd);
}


static bool cmdline_cb(void *k, void *v, void _unused *context)
{
	char *key = k;
	char *value = v;

	pr_info("%s=%s", key, value);
	return true;
}


static void finalizer_execute(void)
{
	char *device, *type;

	pr_info("Finalizing installation...");
	device = hashmapGetPrintf(ictx.opts, NULL, "partition.install:device");
	type = hashmapGetPrintf(ictx.opts, NULL, "partition.install:type");

	mount_partition_device(device, type, "/mnt/install");
	write_install_props();
	umount("/mnt/install");

	/* Just for info */
	pr_info("Required kernel command line options for the bootloader:");
	hashmapForEach(ictx.cmdline, cmdline_cb, NULL);
}


static struct iago_plugin plugin = {
	.cli_session = finalizer_cli,
	.execute = finalizer_execute
};


struct iago_plugin *finalizer_init(void)
{
	return &plugin;
}