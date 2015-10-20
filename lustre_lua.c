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

#include "lustre.h"

#include "stone.h" //FIXME
#include "stone_lua.h"
#include "stdmath.h"
#include "stdmath_lua.h"

lua_State *LU_LUA_STATE = NULL;
char LU_FILE_PATH[4096];
int lua_file_open = 0;
int LUA_EXEC = 0;
static jmp_buf env;
t_file *lua_file = NULL;
int USE_PANIC = 0;
static int SHOW_DATA = 0;
int LU_LOAD = 0;
char LU_DEBUG_MSG[4096];
int LU_DEBUG_STATE = 0;
static int LUA_OPEN = 0;
static int LUA_AUTO_EXEC = 0;
static int LUA_AUTO_EXEC_FORCE = 0;



// Error managment

static int panic( lua_State *L)
{
	longjmp( env, 1);
	return 1;
}

void mn_lua_error( void)
{
	sprintf( LU_DEBUG_MSG, "%s\n", lua_tostring( LU_LUA_STATE, -1));
	LU_DEBUG_STATE = 1;
}

// Lua exec script from editor

void mn_lua_exec( void)
{
	if( SHOW_DATA) printf("%s", lua_file->data);
	if( USE_PANIC) lua_atpanic( LU_LUA_STATE, panic);

	LU_DEBUG_STATE = 0;
	LUA_EVERY_FRAME = 0;
	bzero( LU_DEBUG_MSG, 4096);

	if( !setjmp(env))
	{
		if( luaL_loadbuffer( LU_LUA_STATE, lua_file->data, lua_file->data_size, "buf") == LUA_OK )
		{
			if( !lua_pcall( LU_LUA_STATE, 0, 0, 0))
			{
			}
			else
			{
				mn_lua_error();
			}
		}
		else
		{
			mn_lua_error();
		}
	}
	else
	{
		printf("[LUA] Panic error\n");
	}
}

// Switch to auto-exec mode

void mn_lua_exec_auto( void)
{
	LUA_AUTO_EXEC_FORCE = !LUA_AUTO_EXEC_FORCE;
	LUA_AUTO_EXEC = LUA_AUTO_EXEC_FORCE;
}

// Lua Module : called every frame

static void mn_lua_module( t_module *module)
{
	t_context *C = ctx_get();

	// Auto switch to editor screen
	if( LUA_OPEN)
	{
		editor_open( C);
		LUA_OPEN = 0;
	}

	// Auto exec
	if( LUA_AUTO_EXEC)
	{
		if(editor_cmd_exec()) LUA_AUTO_EXEC = 0;
		if( LUA_AUTO_EXEC_FORCE) LUA_AUTO_EXEC = 1;
	}

	// Exec editor sketch
	if( LUA_EXEC)
	{

		lu_objects_delete();

		mn_lua_exec();
		LUA_EXEC = 0;
	}

	// Call every_frame stored functions
	lua_every_frame_call( LU_LUA_STATE);
}

static void mn_lua_module_add( t_context *C)
{
	t_module *module = mode_module_add( C->mode, "lua", NULL);
	module->update = mn_lua_module;
}

// Loads a sketch to be executed

void mn_lua_load_script( const char *filepath)
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

void mn_lua_exec_script( const char *filepath)
{
	mn_lua_load_script( filepath);
	LUA_OPEN = 1;
	LUA_AUTO_EXEC = 1;
}

// Load Configuration file

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

void mn_lua_load_conf( void)
{
	/*
	if( sys_file_exists( filename))
	{
		if( !setjmp(env))
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
	*/
}

// Get lua_State *L

lua_State *mn_lua_get( void)
{
	return LU_LUA_STATE;
}


// Init Lua System

void editor_scan_args( t_context *C)
{
	const char *arg = app_get_arg( C->app, 1);

	if( arg)
	{
		if( is( arg, "file"))
		{
			char *filename  = (char *) app_get_arg( C->app, 2);
			if( filename)
			{
				printf("loading: %s\n", filename);
				mn_lua_exec_script( filename);
			}
		}
	}
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
	mn_lua_load_conf();

	// Add Mn Lua libs
	lua_minuit_init( LU_LUA_STATE);

	// Add Lua module
	mn_lua_module_add( C);

	// Register Stucco
	lua_stone_register( LU_LUA_STATE);
	lua_mat4_register( LU_LUA_STATE);
	lua_stdmath_register( LU_LUA_STATE);

	STONE_BUILD_FUNCTION = lustre_build;

	editor_scan_args( C);

	return 1;
}
