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



#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cutils/properties.h>
#include <cutils/android_reboot.h>

#include <iago.h>
#include <iago_util.h>

#include "iago_private.h"
#include "register.inc"

struct iago_context ictx;
struct selabel_handle *sehandle;


static void init_iago_context(void)
{
	ictx.opts = hashmapCreate(50, str_hash, str_equals);
	ictx.iprops = hashmapCreate(50, str_hash, str_equals);

	if (!ictx.opts || !ictx.iprops)
		die_errno("malloc");

	list_init(&ictx.plugins);
}

static void load_ini_file(const char *path)
{
	dictionary *d;

	d = iniparser_load(path);
	if (!d)
		die("couldn't load ini file %s", path);

	hashmap_add_dictionary(ictx.opts, d);
}

void add_iago_plugin(struct iago_plugin *p)
{
	list_add_tail(&ictx.plugins, &p->entry);
	ictx.plugin_count++;
}

static void preparation_phase(void)
{
	struct listnode *n;

	property_set("iago.state", "preparing");
	list_for_each(n, &ictx.plugins) {
		struct iago_plugin *p = node_to_item(n, struct iago_plugin,
				entry);
		if (p->prepare) p->prepare();
	}
	pr_info("Installation preparation complete.\n");
}

static void cli_phase(void)
{
	struct listnode *n;

	list_for_each(n, &ictx.plugins) {
		struct iago_plugin *p = node_to_item(n, struct iago_plugin,
				entry);
		if (p->cli_session) p->cli_session();
	}
	pr_info("Installation now configured.\n");
}

static void execution_phase(void)
{
	struct listnode *n;
	int i = 0;
	char buf[4];

	property_set("iago.state", "executing");
	list_for_each(n, &ictx.plugins) {
		struct iago_plugin *p = node_to_item(n, struct iago_plugin,
				entry);
		snprintf(buf, sizeof(buf), "%d",
				(int)((100.0 / ictx.plugin_count) * i));
		property_set("iago.progress", buf);
		if (p->execute) p->execute();
		i++;
	}
	sync();
	property_set("iago.state", "complete");
	pr_info("Installation complete!\n");
}

int main(int argc _unused, char **argv _unused)
{
	char prop[PROPERTY_VALUE_MAX];
	char *reboot_target;
	bool cli_mode, gui_mode;

	pr_info("iago daemon " IAGO_VERSION " starting\n");
	umask(066);
#ifdef HAVE_SELINUX
	struct selinux_opt seopts[] = {
		{ SELABEL_OPT_PATH, "/file_contexts" }
	};

	sehandle = selabel_open(SELABEL_CTX_FILE, seopts, 1);

	if (!sehandle) {
		pr_error("Warning: No file_contexts\n");
	}
#endif
	init_iago_context();
	cli_mode = (property_get("ro.boot.iago.cli", prop, "") > 0);
	gui_mode = (property_get("ro.boot.iago.gui", prop, "") > 0);

	/* Initializes the GPT partition table */
	add_iago_plugin(partitioner_init());

	/* Writes out the base image files/formats empty partitions */
	add_iago_plugin(imagewriter_init());

	/* Stages OTA update for 2nd phase */
	add_iago_plugin(ota_init());

	/* Append any non-builtin plugin structs */
	register_iago_plugins();

	/* Runs at the very end; creates the install partition that contains
	 * install.prop, fstab, and recovery.fstab */
	add_iago_plugin(finalizer_init());

	if (property_get("ro.boot.iago.ini", prop, "") > 0) {
		char *token;
		for (token = strtok(prop, ","); token; token = strtok(NULL, ","))
			append_file(token, COMBINED_INI);
	} else
		die("androidboot.iago.ini not set!\n");
	load_ini_file(COMBINED_INI);
	preparation_phase();

	if (cli_mode) {
		/* Use the built-in command-line text interactive installer
		 * if specified */
		cli_phase();
	} else if (gui_mode) {
		/* TODO this isn't fully implemented */
		//write_opts(ictx.opts, "/data/iago-prepare.ini");
		hashmap_destroy(ictx.opts);
		ictx.opts = hashmapCreate(50, str_hash, str_equals);
		if (!ictx.opts)
			die_errno("malloc");

		property_set("iago.state", "waiting");

		/* Interactive session will create and populate this file.
		 * Block until the property is set. */
		if (property_try_get("iago.ini", prop, "", -1) > 0)
			load_ini_file(prop);
		else
			die("Couldn't get configuration from interactive installer!\n");
	}

	hashmap_dump(ictx.opts);

	if (cli_mode)
		ui_pause();

	execution_phase();

	if (cli_mode) {
		pr_info("All done. Please remove installation media. Device will now reboot");
		ui_pause();
	}

	reboot_target = hashmapGetPrintf(ictx.opts, "", BASE_REBOOT);
	android_reboot(ANDROID_RB_RESTART2, 0, reboot_target);

	return 0;
}
