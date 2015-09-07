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
#include "object.h"
#include "scene.h"
#include "ctx.h"
#include "op.h"
#include "mesh.h"
#include "vlst.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lustre_lib.h"
#include "lustre.h"
#include "stone.h"
#include "stone_lua.h"
#include "stdmath.h"
#include "stdmath_lua.h"

lua_State *LUA_STATE = NULL;
char LUA_SKETCH_PATH[4096];
int lua_file_open = 0;
static t_lst *objects = NULL;
int LUA_EXEC = 0;
static jmp_buf env;
t_file *lua_file = NULL;
int USE_PANIC = 0;
static int SHOW_DATA = 0;
int LUA_LOAD = 0;
char LUA_DEBUG[4096];
int LUA_DEBUG_STATE = 0;
static int LUA_OPEN = 0;
static int LUA_AUTO_EXEC = 0;
static int LUA_AUTO_EXEC_FORCE = 0;

int stone_screen_init = 0;

// Add and removes Mesh Objects

void mn_lua_add_object( struct Object *obj)
{
	lst_add( objects, obj, "obj");
}

void mn_lua_remove_objects( t_context *C)
{
	t_link *l;
	for( l = objects->first; l; l = l->next)
	{
		t_object *object = ( t_object *) l->data;
		scene_node_delete( C->scene, object->id.node);
	}

	lst_cleanup( objects);
}

void mn_lua_free_object( struct Object *object)
{
	t_context *C = ctx_get();
	lst_link_delete_by_id( objects, object->id.id);
	scene_node_delete( C->scene, object->id.node);
}

// Error managment

static int panic( lua_State *L)
{
	longjmp( env, 1);
	return 1;
}

void mn_lua_error( void)
{
	sprintf( LUA_DEBUG, "%s\n", lua_tostring( LUA_STATE, -1));
	LUA_DEBUG_STATE = 1;
}

// Lua exec script from editor

void mn_lua_exec( void)
{
	if( SHOW_DATA) printf("%s", lua_file->data);
	if( USE_PANIC) lua_atpanic( LUA_STATE, panic);

	LUA_DEBUG_STATE = 0;
	LUA_EVERY_FRAME = 0;
	bzero( LUA_DEBUG, 4096);

	if( !setjmp(env))
	{
		if( luaL_loadbuffer( LUA_STATE, lua_file->data, lua_file->data_size, "buf") == LUA_OK )
		{
			if( !lua_pcall( LUA_STATE, 0, 0, 0))
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
		if( !objects) objects = lst_new("objects");
		mn_lua_remove_objects( C);
		mn_lua_exec();
		LUA_EXEC = 0;
	}

	// Call every_frame stored functions
	lua_every_frame_call( LUA_STATE);
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
		s_cp( LUA_SKETCH_PATH, filepath, 4096);
		LUA_LOAD = 1;
		LUA_INIT = 0;
		if(LUA_FILE)
		{
			file_close(LUA_FILE);
			file_free(LUA_FILE);
			LUA_FILE = NULL;
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
	lua_getglobal( LUA_STATE, "version");
	if( !lua_isnumber( LUA_STATE, -1))
	{
		luaL_error( LUA_STATE, "version must be a number");
	}
	else
	{
	//	int version = lua_tonumber( LUA_STATE, -1);
	}
}

void mn_lua_load_conf( void)
{
	/*
	if( sys_file_exists( filename))
	{
		if( !setjmp(env))
		{
			if( !luaL_loadfile( LUA_STATE, filename))
			{
			       	if( !lua_pcall( LUA_STATE, 0, 0, 0))
				{
					// Exec configuration file
					mn_lua_conf();
				}
				else
				{
					luaL_error( LUA_STATE, "[LUA] Exec Error: %s", lua_tostring( LUA_STATE, -1));
				}
			}
			else
			{
				luaL_error( LUA_STATE, "[LUA] Load Error: %s", lua_tostring( LUA_STATE, -1));
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
	return LUA_STATE;
}

void lustre_build( t_lua_stone *lua_stone)
{
	t_context *C = ctx_get();
	t_stone *stone = lua_stone->stone;
	stone_merge( stone, NULL);

	scene_store(C->scene,1);
	float *vertex = stone_get_vertex_buffer( stone);
	int quad_count = stone_get_quad_count( stone);
	int tri_count = stone_get_tri_count( stone);
	int *quads = stone_get_quad_buffer( stone, quad_count);
	int *tris = stone_get_tri_buffer( stone, tri_count);
	int *edges = stone_get_edge_buffer( stone);

	printf("face count %d\n", stone->face_count);

	t_object *object = lua_stone->object = op_add_mesh_data( "stone", 
			stone->vertex_count,
			quad_count,
			tri_count,
			vertex,
			quads,
			tris
			);

	mn_lua_add_object( object);

	if( edges)
	{
		int edge_count = stone->edge_count;
		t_mesh *mesh = object->mesh;
		mesh->edges = vlst_make( "edges", dt_uint, 2, edge_count, edges);
		mesh->state.with_line =1;
	}


	free(vertex);
	free(quads);
	free(tris);

	if( !stone_screen_init)
	{
		op_add_screen( NULL);
		op_add_light(NULL);
		stone_screen_init = 1;
	}

	scene_store(C->scene,0);

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

int mn_lua_init( void)
{
	t_context *C = ctx_get();

	// New Lua state
	LUA_STATE = luaL_newstate();

	// Open Lua Std Libs
	luaL_openlibs(LUA_STATE);

	// Init Editor Screen
	screen_editor_make( C);

	// Load configuration file
	mn_lua_load_conf();

	// Add Mn Lua libs
	lua_minuit_init( LUA_STATE);

	// Add Lua module
	mn_lua_module_add( C);

	// Register Stucco
	lua_stone_register( LUA_STATE);
	lua_mat4_register( LUA_STATE);
	lua_stdmath_register( LUA_STATE);

	STONE_BUILD_FUNCTION = lustre_build;

	editor_scan_args( C);

	return 1;
}
