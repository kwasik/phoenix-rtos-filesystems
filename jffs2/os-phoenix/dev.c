/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Jffs2 FileSystem - system specific information.
 *
 * Copyright 2018 Phoenix Systems
 * Author: Kamil Amanowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */
#include <stdlib.h>
#include <sys/rb.h>
#include <sys/threads.h>
#include <string.h>

#include "../os-phoenix.h"
#include "dev.h"

int old_valid_dev(dev_t dev)
{
	return 0;
}

int old_encode_dev(dev_t dev)
{
	return dev;
}

int new_encode_dev(dev_t dev)
{
	return dev;
}

dev_t old_decode_dev(u16 dev)
{
	return dev;
}

dev_t new_decode_dev(u32 dev)
{
	return dev;
}

struct {
	rbtree_t dev_oid;
	rbtree_t dev_ino;
	handle_t mutex;
} dev_common;


static int dev_cmp_oid(rbnode_t *n1, rbnode_t *n2)
{
	jffs2_dev_t *d1 = lib_treeof(jffs2_dev_t, linkage_oid, n1);
	jffs2_dev_t *d2 = lib_treeof(jffs2_dev_t, linkage_oid, n2);

	if (d1->dev.port != d2->dev.port)
		return d1->dev.port > d2->dev.port ? 1 : -1;

	if (d1->dev.id != d2->dev.id)
		return d1->dev.id > d2->dev.id ? 1 : -1;

	return 0;
}

static int dev_cmp_ino(rbnode_t *n1, rbnode_t *n2)
{
	jffs2_dev_t *d1 = lib_treeof(jffs2_dev_t, linkage_ino, n1);
	jffs2_dev_t *d2 = lib_treeof(jffs2_dev_t, linkage_ino, n2);

	if (d1->ino != d2->ino)
		return d1->ino > d2->ino ? 1 : -1;

	return 0;
}

jffs2_dev_t *dev_find_oid(oid_t *oid, unsigned long ino, int create)
{
	jffs2_dev_t find, *entry;

	memcpy(&find.dev, oid, sizeof(oid_t));

	mutexLock(dev_common.mutex);
	entry = lib_treeof(jffs2_dev_t, linkage_oid, lib_rbFind(&dev_common.dev_oid, &find.linkage_oid));

	if (!create || entry != NULL) {
		mutexUnlock(dev_common.mutex);
		return entry;
	}

	if ((entry = malloc(sizeof(jffs2_dev_t))) == NULL) {
		mutexUnlock(dev_common.mutex);
		return NULL;
	}

	entry->nlink = 1;
	entry->ino = ino;
	memcpy(&entry->dev, oid, sizeof(oid_t));
	lib_rbInsert(&dev_common.dev_ino, &entry->linkage_ino);
	lib_rbInsert(&dev_common.dev_oid, &entry->linkage_oid);
	mutexUnlock(dev_common.mutex);
	return entry;
}

jffs2_dev_t *dev_find_ino(unsigned long ino)
{
	jffs2_dev_t find, *entry;

	find.ino = ino;

	mutexLock(dev_common.mutex);
	entry = lib_treeof(jffs2_dev_t, linkage_ino, lib_rbFind(&dev_common.dev_ino, &find.linkage_ino));

	mutexUnlock(dev_common.mutex);
	return entry;
}


void dev_inc(oid_t *oid)
{
	jffs2_dev_t find, *entry;

	memcpy(&find.dev, oid, sizeof(oid_t));

	mutexLock(dev_common.mutex);
	entry = lib_treeof(jffs2_dev_t, linkage_oid, lib_rbFind(&dev_common.dev_oid, &find.linkage_oid));

	if (entry != NULL)
		entry->nlink++;

	mutexUnlock(dev_common.mutex);
}


void dev_dec(oid_t *oid)
{
	jffs2_dev_t find, *entry;

	memcpy(&find.dev, oid, sizeof(oid_t));

	mutexLock(dev_common.mutex);
	entry = lib_treeof(jffs2_dev_t, linkage_oid, lib_rbFind(&dev_common.dev_oid, &find.linkage_oid));

	if (entry != NULL)
		entry->nlink--;

	mutexUnlock(dev_common.mutex);
	dev_destroy(oid);
}


void dev_destroy(oid_t *oid)
{
	jffs2_dev_t find, *entry;

	memcpy(&find.dev, oid, sizeof(oid_t));

	mutexLock(dev_common.mutex);
	entry = lib_treeof(jffs2_dev_t, linkage_oid, lib_rbFind(&dev_common.dev_oid, &find.linkage_oid));

	if (entry == NULL) {
		mutexUnlock(dev_common.mutex);
		return;
	}

	if (entry->nlink <= 0) {
		lib_rbRemove(&dev_common.dev_ino, &entry->linkage_ino);
		lib_rbRemove(&dev_common.dev_oid, &entry->linkage_oid);
	}

	free(entry);
	mutexUnlock(dev_common.mutex);
}


void dev_init()
{
	mutexCreate(&dev_common.mutex);
	lib_rbInit(&dev_common.dev_ino, dev_cmp_ino, NULL);
	lib_rbInit(&dev_common.dev_oid, dev_cmp_oid, NULL);
}
