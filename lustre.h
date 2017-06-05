/* 
 * Copyright (c) 2015 Milovann Yanatchkov 
 *
 * This file is part of Lustre, a free software
 * licensed under the GNU General Public License v2
 * see /LICENSE for more information
 *
 */

#ifndef __EDITOR_H__
#define __EDITOR_H__

struct Context;
struct lua_State;
struct Context;
struct MNScreen;
struct Object;
struct File;
struct Lua_Stone;

// lustre_lua

extern char LU_FILE_PATH[4096];
extern int LUA_EXEC;
extern int LU_LOAD;
extern char LU_DEBUG_MSG[4096];
extern int LU_DEBUG_STATE;
extern int LU_EVERY_FRAME;
extern struct File *lua_file;

void lu_lua_exec_auto( void);
void lu_lua_load_file( const char *filepath);
void lu_lua_error( void);

void lustre_register( void(* func)(struct lua_State *L));
int lustre_init( void);

// lustre_editor

struct MINscreen *lu_editor_screen_init( struct Context *C);
void lu_editor_open( struct Context *C);
extern int LU_INIT;
extern struct File *LU_FILE;
void lu_editor_keymap( int key);
int lu_editor_cmd_exec( void);
void lu_editor_db_clear( void);
void lu_editor_log_add( const char *msg);
void lu_editor_set( const char *name, void *val);

// lustre_lib

void lu_lib_init( struct lua_State *L);
void lu_lib_object_build( struct Lua_Stone *lua_stone);
void lu_lib_objects_delete( void);


#endif
