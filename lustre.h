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

extern char LUA_SKETCH_PATH[4096];

void mn_lua_load_script( const char *filepath);
void mn_lua_exec_script( const char *filepath);
void mn_lua_exec_auto( void);

int mn_lua_init( void);
int mn_lua_load( const char *filename);
struct lua_State *mn_lua_get( void);

extern int LUA_EXEC;
extern int LUA_LOAD;
extern struct File *lua_file;
extern char LUA_DEBUG[4096];
extern int LUA_DEBUG_STATE;
extern int LUA_EVERY_FRAME;

void mn_lua_add_object( struct Object *obj);
void mn_lua_free_object( struct Object *obj);

void mn_lua_error( void);

// lustre_editor

struct MINscreen *screen_editor_make( struct Context *C);
void editor_open( struct Context *C);
extern int LUA_INIT;
extern struct File *LUA_FILE;

void editor_keyboard( int key);
void ed_init( struct Context *C);

int editor_cmd_exec( void);

// lustre_lib

struct lua_State;
int lua_every_frame_call( struct lua_State *L);
void lua_minuit_init( struct lua_State *L);


#endif
