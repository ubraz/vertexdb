/*
Notes:
	tcbdboptimize() only works on hash dbs, so we can't use it - use collectgarbage instead
*/

#include "Store.h"

#include <tcutil.h>
#include <tcbdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

Store *Store_new(void)
{
	Store *self = calloc(1, sizeof(Store));
	Store_setPath_(self, "default.store");
	return self;
}

void Store_free(Store *self)
{
	Store_close(self);
	free(self->path);
	free(self);
}

void Store_setPath_(Store *self, const char *p)
{
	self->path = strcpy(realloc(self->path, strlen(p)+1), p);
}

const char *Store_path(Store *self)
{
	return self->path;
}

void Store_setCompareFunction_(Store *self, void *func)
{
	self->compareFunc = func;
}

void Store_setHardSync_(Store *self, int aBool)
{
	self->hardSync = aBool;
}


int Store_open(Store *self)
{
	self->db = tcbdbnew();

	/*
	//Tinkering with these seems to result in worse performance so far...
	
	tcbdbsetxmsiz(self->db, 1024*1024*64); 

	if(!tcbdbtune(self->db, 0, 0, 0, -1, -1, BDBTDEFLATE)) // HDBTLARGE
	{
		Log_Printf("tcbdbtune failed\n");
		return -1;
	}
	*/

	/*
	if (!tcbdbsetcache(self->db, 0, 512*4096))
	{
		//Log_Printf("tcbdbsetcache failed\n");
		return -1;
	}
	*/
	
	if (!tcbdbsetcmpfunc(self->db, self->compareFunc, NULL))
	{
		return 0;
	}

	{
		int flags = BDBOWRITER | BDBOCREAT | BDBONOLCK;
		
		if (self->hardSync) 
		{
			printf("Store: hard disk syncing enabled\n");
			flags |= BDBOTSYNC;
		}
		
		if (!tcbdbopen(self->db, self->path, flags))
		{
			return 0;
		}
	}
	
	return 1;
}

int Store_isOpen(Store *self)
{
	return self->db != 0x0;
}

int Store_close(Store *self)
{
	int r = 0;
	
	if (self->db)
	{
		tcbdbclose(self->db);
		tcbdbdel(self->db);
		self->db = 0x0;
		self->inTransaction = 0;
	}
	
	return r;
}

int Store_backup(Store *self, const char *backupPath)
{
	return tcbdbcopy(self->db, backupPath); //tc will create a .wal file
}

const char *Store_error(Store *self)
{
	return tcbdberrmsg(tcbdbecode(self->db));
}

char *Store_read(Store *self, const char *k, size_t ksize, int *vsize)
{
	void *v = tcbdbget(self->db, k, ksize, vsize);
	return v;
}

int Store_write(Store *self, const char *k, size_t ksize, const char *v, size_t vsize)
{
	if(!tcbdbput(self->db, k, ksize, v, vsize))
	{
		return 0;
	}
	
	return 1;
}

int Store_append(Store *self, const char *k, size_t ksize, const char *v, size_t vsize)
{
	if(!tcbdbputcat(self->db, k, ksize, v, vsize))
	{
		return 0;
	}
	
	return 1;
}

int Store_remove(Store *self, const char *k, size_t ksize)
{
	if(!tcbdbout(self->db, k, ksize))
	{
		return 0;
	}
	
	return 1;
}

int Store_sync(Store *self)
{
	if(!tcbdbsync(self->db))
	{
		return 0;
	}
	
	return 1;
}

int Store_inTransaction(Store *self)
{
	return self->inTransaction;
}

int Store_begin(Store *self)
{
	if(self->inTransaction) 
	{
		return 0;
	}
	
	if (!tcbdbtranbegin(self->db))
	{
		return 0;
	}
	
	self->inTransaction = 1;
	
	return 1;
}

int Store_abort(Store *self)
{
	if (!tcbdbtranabort(self->db))
	{
		return 0;
	}
	
	self->inTransaction = 0;

	return 1;
}

int Store_commit(Store *self)
{
	if (!tcbdbtrancommit(self->db))
	{
		return 0;
	}
	
	self->inTransaction = 0;
	
	return 1;
}

uint64_t Store_size(Store *self)
{
	return tcbdbfsiz(self->db);
}

uint64_t Store_numberOfKeys(Store *self)
{
	return tcbdbrnum(self->db);
}

