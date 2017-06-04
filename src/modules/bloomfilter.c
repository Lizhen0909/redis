/* Helloworld module -- A few examples of the Redis Modules API in the form
 * of commands showing how to accomplish common tasks.
 *
 * This module does not do anything useful, if not for a few commands. The
 * examples are designed in order to show the API.
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (c) 2016, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "../redismodule.h"
#include "../../deps/dablooms/src/dablooms.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <glib.h>

extern char *mkdtemp(char *template);
extern char *tempnam(const char *dir, const char *pfx);
extern const char *RM_StringPtrLen(const RedisModuleString *str, size_t *len);

GHashTable * global_bloomfilters = NULL;
GHashTable * get_bloomfilter_hashtable();

typedef struct {
	long index;
	char filename[120];
	scaling_bloom_t* bloomfilter;
} bloomfilter_count_t;

void free_string(gpointer data) {
	g_free(data);
}

void free_bloomfilter(gpointer data) {
	bloomfilter_count_t *bf = (bloomfilter_count_t*) data;
	free_scaling_bloom(bf->bloomfilter);
	remove(bf->filename);
	free(bf);
}

GHashTable * create_bloomfilter_hashtable() {
	GHashTable * hashtable = g_hash_table_new_full(g_str_hash, g_str_equal,
			free_string, free_bloomfilter);
	return hashtable;
}

void remove_all_bloomfilters() {
	if (global_bloomfilters) {
		g_hash_table_destroy(global_bloomfilters);
		global_bloomfilters = NULL;
	}
}

bloomfilter_count_t* get_bloomfilter(const char* name) {
	GHashTable * hashtable = get_bloomfilter_hashtable();
	gpointer val = g_hash_table_lookup(hashtable, name);
	if (val) {
		return (bloomfilter_count_t *) val;
	} else {
		char* copied_name = g_strdup(name);
		char *templatebuf = tempnam("/tmp", "rbf_");

		unsigned int sz = 64 * 1024 * 1024;
		scaling_bloom_t *scale_bf = new_scaling_bloom(sz, 0.01, templatebuf);
		bloomfilter_count_t *bf = malloc(sizeof(bloomfilter_count_t));
		bf->index = 0;
		bf->bloomfilter = scale_bf;
		strcpy(bf->filename, templatebuf);
		g_hash_table_insert(global_bloomfilters, copied_name, bf);
		free(templatebuf);
		return bf;
	}

}

GHashTable * get_bloomfilter_hashtable() {
	if (!global_bloomfilters) {
		global_bloomfilters = create_bloomfilter_hashtable();
	}
	return global_bloomfilters;
}

/*
 * BF.FLUSHALL
 * 		close bloom filter then flushall
 *
 */
int BfFlushALL_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv,
		int argc) {
	REDISMODULE_NOT_USED(argv);
	REDISMODULE_NOT_USED(argc);
	remove_all_bloomfilters();

	RedisModuleCallReply *reply;
	reply = RedisModule_Call(ctx, "FLUSHALL", "");
	RedisModule_ReplyWithCallReply(ctx, reply);
	RedisModule_FreeCallReply(reply);
	return REDISMODULE_OK;
}
/*
 * BF.HINCR KEY HKEY
 * 		If HKEY is in bloomfilter for KEY then HINCR KEY HKEY 1, else add it to bloomfilter
 *
 */
int BfHincr_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv,
		int argc) {

	if (argc != 3)
		return RedisModule_WrongArity(ctx);

	RedisModuleString *key = argv[1];
	RedisModuleString *hkey = argv[2];
	size_t len = 0;
	const char* strkey = RM_StringPtrLen(key, &len);

	size_t len2 = 0;
	const char* strhkey = RM_StringPtrLen(hkey, &len2);

	bloomfilter_count_t* bf = get_bloomfilter(strkey);
	if (scaling_bloom_check(bf->bloomfilter, strhkey, len2)) {
		RedisModuleCallReply *reply;
		reply = RedisModule_Call(ctx, "HINCRBY", "ssl", key, hkey, 1);
		RedisModule_ReplyWithCallReply(ctx, reply);
		RedisModule_FreeCallReply(reply);

	} else {

		scaling_bloom_add(bf->bloomfilter, strhkey, len2, bf->index++);
		RedisModule_ReplyWithLongLong(ctx, 0);
	}
	return REDISMODULE_OK;
}

/* This function must be present on each Redis module. It is used in order to
 * register the commands into the Redis server. */
int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
	if (RedisModule_Init(ctx, "bloomfilter", 1,
	REDISMODULE_APIVER_1) == REDISMODULE_ERR)
		return REDISMODULE_ERR;

	/* Log the list of parameters passing loading the module. */
	for (int j = 0; j < argc; j++) {
		const char *s = RedisModule_StringPtrLen(argv[j], NULL);
		printf("Module loaded with ARGV[%d] = %s\n", j, s);
	}

	if (RedisModule_CreateCommand(ctx, "bf.hincr", BfHincr_RedisCommand,
			"write deny-oom", 1, 1, 1) == REDISMODULE_ERR)
		return REDISMODULE_ERR;

	if (RedisModule_CreateCommand(ctx, "bf.flushall", BfFlushALL_RedisCommand,
			"write", 0, 0, 0) == REDISMODULE_ERR)
		return REDISMODULE_ERR;

	return REDISMODULE_OK;
}
