/* 
 * Copyright (c) 2015 Milovann Yanatchkov 
 *
 * This file is part of Lustre, a free software
 * licensed under the GNU General Public License v2
 * see /LICENSE for more information
 *
 */

#include "ctx.h"
#include "mode.h"
#include "app.h"
#include "base.h"
#include "lst.h"
#include "setjmp.h"
#include "file.h"
#include "scene.h"
#include "ctx.h"
#include "mesh.h"
#include "vlst.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#ifdef HAVE_LUA_5_1
#include "compat-5.3.h"
#endif

#include "lustre.h"

#include "mud.h" //FIXME
#include "mud_lua.h"
#include "stdmath.h"
#include "stdmath_lua.h"

#ifdef HAVE_TINYSPLINE
#include "mud_spline_lua.h"
#endif

#ifdef HAVE_LPEG
#include "lpeg.h"
#endif

#ifdef HAVE_ETHERDREAM
#include "etherdream_lua.h"
#endif

#ifdef HAVE_FLUXA
#include "fluxa_lua.h"
#endif

lua_State *LU_LUA_STATE = NULL;
char LU_FILE_PATH[4096];
int lua_file_open = 0;
int LUA_EXEC = 0;
static jmp_buf lu_lua_env;
t_file *lua_file = NULL;
int LU_LUA_USE_PANIC = 0;
static int LU_LUA_SHOW_FILE = 0;
int LU_LOAD = 0;
char LU_DEBUG_MSG[4096];
int LU_DEBUG_STATE = 0;
static int LU_EDITOR_OPEN = 0;
static int LU_LUA_AUTO_EXEC = 0;
static int LU_LUA_AUTO_EXEC_FORCE = 0;

static int LU_POS_H = LU_POS_LEFT; 
static int LU_POS_V = LU_POS_TOP;

int lu_lua_get_pos_h()
{
	return LU_POS_H;
}

int lu_lua_get_pos_v()
{
	return LU_POS_V;
}

// Error managment

static int lu_lua_panic( lua_State *L)
{
	longjmp( lu_lua_env, 1);
	return 1;
}

void lu_lua_error( void)
{
	sprintf( LU_DEBUG_MSG, "%s\n", lua_tostring( LU_LUA_STATE, -1));
	LU_DEBUG_STATE = 1;
}

// Lua exec script from editor

void lu_lua_exec( void)
{
	/* clear db */
	lu_editor_db_clear();

	if( LU_LUA_SHOW_FILE) printf("%s", lua_file->data);
	if( LU_LUA_USE_PANIC) lua_atpanic( LU_LUA_STATE, lu_lua_panic);

	LU_DEBUG_STATE = 0;
	LU_EVERY_FRAME = 0;
	bzero( LU_DEBUG_MSG, 4096);

	if( !setjmp(lu_lua_env))
	{
		if( luaL_loadbuffer( LU_LUA_STATE, lua_file->data, lua_file->data_size, "buf") == LUA_OK )
		{
			if( !lua_pcall( LU_LUA_STATE, 0, 0, 0))
			{
			}
			else
			{
				lu_lua_error();
			}
		}
		else
		{
			lu_lua_error();
		}
	}
	else
	{
		printf("[LUA] Panic error\n");
	}
}

// Switch to auto-exec mode

void lu_lua_exec_auto( void)
{
	LU_LUA_AUTO_EXEC_FORCE = !LU_LUA_AUTO_EXEC_FORCE;
	LU_LUA_AUTO_EXEC = LU_LUA_AUTO_EXEC_FORCE;
}

int lua_every_frame_call( lua_State *L)
{
	if( LU_EVERY_FRAME)
	{
		lua_getglobal( L, "every_frame_function");
		if( lua_pcall( L, 0,0,0) != LUA_OK)
		{
			lu_lua_error();
			return 0;
		}
	}
	return 0;
}

// Lua Module : called every frame

static void lu_lua_module( t_module *module)
{
	t_context *C = ctx_get();

	// Auto switch to editor screen
	if( LU_EDITOR_OPEN)
	{
		lu_editor_open( C);
		LU_EDITOR_OPEN = 0;
	}

	// Auto exec
	if( LU_LUA_AUTO_EXEC)
	{
		if(lu_editor_cmd_exec()) LU_LUA_AUTO_EXEC = 0;
		if( LU_LUA_AUTO_EXEC_FORCE) LU_LUA_AUTO_EXEC = 1;
	}

	// Exec editor sketch
	if( LUA_EXEC)
	{

		lu_lib_objects_delete();

		lu_lua_exec();
		LUA_EXEC = 0;
	}

	// Call every_frame stored functions
	lua_every_frame_call( LU_LUA_STATE);
}

static void lu_lua_module_add( t_context *C)
{
	t_module *module = mode_module_add( C->mode, "lua", NULL);
	module->update = lu_lua_module;
}

// Loads a sketch to be executed

void lu_lua_load_file( const char *filepath)
{
	if( filepath)
	{
		s_cp( LU_FILE_PATH, filepath, 4096);
		LU_LOAD = 1;
		LU_INIT = 0;
		if(LU_FILE)
		{
			file_close(LU_FILE);
			file_free(LU_FILE);
			LU_FILE = NULL;
		}
	}
}

// Load && exec sketch (from vim)

void lu_lua_exec_script( const char *filepath)
{
	lu_lua_load_file( filepath);
	LU_EDITOR_OPEN = 1;
	LU_LUA_AUTO_EXEC = 1;
}

// Load Configuration file

/*
void mn_lua_conf( void)
{
	lua_getglobal( LU_LUA_STATE, "version");
	if( !lua_isnumber( LU_LUA_STATE, -1))
	{
		luaL_error( LU_LUA_STATE, "version must be a number");
	}
	else
	{
	//	int version = lua_tonumber( LU_LUA_STATE, -1);
	}
}

void lu_lua_load_conf( void)
{
	if( sys_file_exists( filename))
	{
		if( !setjmp(lu_lua_env))
		{
			if( !luaL_loadfile( LU_LUA_STATE, filename))
			{
			       	if( !lua_pcall( LU_LUA_STATE, 0, 0, 0))
				{
					// Exec configuration file
					mn_lua_conf();
				}
				else
				{
					luaL_error( LU_LUA_STATE, "[LUA] Exec Error: %s", lua_tostring( LU_LUA_STATE, -1));
				}
			}
			else
			{
				luaL_error( LU_LUA_STATE, "[LUA] Load Error: %s", lua_tostring( LU_LUA_STATE, -1));
			}
		}

	}
	else
	{
		printf("mn_lua_load: error while loading %s\n", filename);
	}
}
*/

// Init Lua System

void lu_lua_scan_args( t_context *C)
{
	const char *arg = app_get_arg( C->app, 1);

	if( arg)
	{
		if( iseq( arg, "file"))
		{
			char *filename  = (char *) app_get_arg( C->app, 2);
			if( filename)
			{
				lu_lua_exec_script( filename);
			}
		}
	}

	/* no-stars +2 ?*/
	const char *h = app_get_arg( C->app, 4);
	const char *v = app_get_arg( C->app, 5);
	if( h && v)
	{
		if( iseq(h,"left")) { LU_POS_H = LU_POS_LEFT; }
		if( iseq(h,"right")) {LU_POS_H = LU_POS_RIGHT; }
		if( iseq(v,"top")) { LU_POS_V = LU_POS_TOP; }
		if( iseq(v,"down")) {LU_POS_V = LU_POS_DOWN; }
	}
}

void lustre_register( void(* func)(lua_State *L))
{
	func( LU_LUA_STATE);
}

int lustre_init( void)
{
	t_context *C = ctx_get();

	// New Lua state
	LU_LUA_STATE = luaL_newstate();

	// Open Lua Std Libs
	luaL_openlibs(LU_LUA_STATE);

	// Init Editor Screen
	lu_editor_screen_init( C);

	// Load configuration file
	//lu_lua_load_conf();

	// Add Lustre lib
	lu_lib_init( LU_LUA_STATE);

	// Add Lua module
	lu_lua_module_add( C);

	// Register Mud
	lua_mud_register( LU_LUA_STATE);
	lua_mat4_register( LU_LUA_STATE);
	lua_stdmath_register( LU_LUA_STATE);

	#ifdef HAVE_TINYSPLINE
	lua_mud_spline_register( LU_LUA_STATE);
	#endif

	#ifdef HAVE_LPEG
	luaopen_lpeg( LU_LUA_STATE);
	lua_setglobal( LU_LUA_STATE, "lpeg");
	#endif

	#ifdef HAVE_ETHERDREAM
	lua_etherdream_register( LU_LUA_STATE);
	#endif

	#ifdef HAVE_FLUXA
	lua_fluxa_register( LU_LUA_STATE);
	#endif

	MUD_BUILD_FUNCTION = lu_lib_object_build;

	// Scan args
	lu_lua_scan_args( C);

	return 1;
}
