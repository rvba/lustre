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

extern char LU_FILE_PATH[4096];

void lu_lua_load_file( const char *filepath);
void lu_lua_exec_script( const char *filepath);
void lu_lua_exec_auto( void);

int lustre_init( void);
int mn_lua_load( const char *filename);

extern int LUA_EXEC;
extern int LU_LOAD;
extern struct File *lua_file;
extern char LU_DEBUG_MSG[4096];
extern int LU_DEBUG_STATE;
extern int LUA_EVERY_FRAME;

void mn_lua_add_object( struct Object *obj);
void mn_lua_free_object( struct Object *obj);

void mn_lua_error( void);

// lustre_editor

struct MINscreen *lu_editor_screen_init( struct Context *C);
void editor_open( struct Context *C);
extern int LU_INIT;
extern struct File *LU_FILE;

void editor_keyboard( int key);
void ed_init( struct Context *C);

int editor_cmd_exec( void);

// lustre_lib

struct lua_State;
int lua_every_frame_call( struct lua_State *L);
void lu_lib_init( struct lua_State *L);
void lustre_build( struct Lua_Stone *lua_stone);
void lu_objects_delete( void);


#endif
