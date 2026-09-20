/*
 * The PlayStation Classic's power daemon watchers - see ab_console.h.
 *
 * (C) AutoBleem team, 2026 - after Sony's power_manage/watch_cpu_temperature in pcsx-ab
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#include <stdio.h>
#include <string.h>

#include "ab_console.h"

volatile int ab_console_power_off_requested;
volatile int ab_console_overheated;

#ifdef __linux__

#include <pthread.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>

#define EVENT_BUF 4096

static void *power_watch(void *unused)
{
	char buf[EVENT_BUF];
	int fd, wd, len, i;
	(void)unused;

	fd = inotify_init();
	if (fd < 0)
		return NULL;
	wd = inotify_add_watch(fd, AB_POWER_DIR, IN_CREATE);
	if (wd < 0) {
		close(fd);
		return NULL;
	}
	for (;;) {
		len = read(fd, buf, sizeof(buf));
		if (len <= 0)
			break;
		for (i = 0; i < len; ) {
			struct inotify_event *ev = (struct inotify_event *)&buf[i];
			if (ev->len > 0 && strcmp(ev->name, AB_POWER_OFF_FILE) == 0) {
				ab_console_power_off_requested = 1;
				goto out;
			}
			i += sizeof(*ev) + ev->len;
		}
	}
out:
	inotify_rm_watch(fd, wd);
	close(fd);
	return NULL;
}

static int read_temp_limit(void)
{
	char key[64];
	int value, limit = AB_CPU_TEMP_LIMIT_DEFAULT;
	FILE *f = fopen(AB_CPU_TEMP_LIMIT_FILE, "r");

	if (f == NULL)
		return limit;
	while (fscanf(f, "%63s %d", key, &value) == 2) {
		if (strcmp(key, AB_CPU_TEMP_LIMIT_KEY) == 0) {
			limit = value;
			break;
		}
	}
	fclose(f);
	return limit;
}

static void *temperature_watch(void *unused)
{
	char buf[EVENT_BUF];
	int fd, wd, limit, temp, mode;
	(void)unused;

	limit = read_temp_limit();
	fd = inotify_init();
	if (fd < 0)
		return NULL;
	wd = inotify_add_watch(fd, AB_CPU_TEMP_FILE, IN_MODIFY);
	if (wd < 0) {
		close(fd);
		return NULL;
	}
	for (;;) {
		FILE *f;
		if (read(fd, buf, sizeof(buf)) <= 0)
			break;
		f = fopen(AB_CPU_TEMP_FILE, "r");
		if (f == NULL)
			continue;
		if (fscanf(f, "%d%d", &temp, &mode) >= 1 && temp >= limit) {
			fclose(f);
			printf("autobleem: cpu temperature %d over %d, stopping\n", temp, limit);
			ab_console_overheated = 1;
			break;
		}
		fclose(f);
	}
	inotify_rm_watch(fd, wd);
	close(fd);
	return NULL;
}

int ab_console_start(void)
{
	pthread_t th;
	pthread_attr_t attr;
	struct stat st;
	int is_console = access(AB_CPU_TEMP_FILE, R_OK) == 0 || access(AB_POWER_DIR, R_OK) == 0;

	if (!is_console) {
		printf("autobleem: no power daemon (not a PlayStation Classic), nothing to watch\n");
		return 0;
	}
	/* the daemon already asked before we started: go straight out */
	if (stat(AB_POWER_DIR AB_POWER_OFF_FILE, &st) == 0)
		ab_console_power_off_requested = 1;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&th, &attr, power_watch, NULL) != 0)
		printf("autobleem: no power watcher\n");
	if (pthread_create(&th, &attr, temperature_watch, NULL) != 0)
		printf("autobleem: no temperature watcher\n");
	pthread_attr_destroy(&attr);
	return 1;
}

#else /* not Linux: nothing to watch */

int ab_console_start(void)
{
	return 0;
}

#endif
