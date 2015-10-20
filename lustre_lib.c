/* 
 * Copyright (c) 2015 Milovann Yanatchkov 
 *
 * This file is part of Lustre, a free software
 * licensed under the GNU General Public License v2
 * see /LICENSE for more information
 *
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lustre.h"

#include "node.h"
#include "scene.h"
#include "object.h"
#include "ctx.h"
#include "app.h"
#include "base.h"
#include "mesh.h"
#include "vlst.h"
#include "object.h"
#include "op.h"

#include "stone.h"

#include "stone_lua.h"

#define L_MINUIT "L_MINUIT"

static t_lst *objects = NULL;
int stone_screen_init = 0;

lua_CFunction LUA_FRAME = NULL;
int LUA_EVERY_FRAME = 0;

int lu_lib_time( lua_State *L)
{
	lua_pushnumber( L, clock_now_sec_precise());
	return 1;
}

// Add and removes Mesh Objects

void lu_lib_object_add( struct Object *obj)
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


void lu_objects_delete( void)
{
	t_context *C = ctx_get();

		if( !objects) objects = lst_new("objects");
		mn_lua_remove_objects( C);
}

void mn_lua_free_object( struct Object *object)
{
	t_context *C = ctx_get();
	lst_link_delete_by_id( objects, object->id.id);
	scene_node_delete( C->scene, object->id.node);
}

static t_object *get_object( t_context *C, int id)
{
	t_lst *lst = scene_lst_get( C->scene, dt_object);
	if( lst)
	{
		t_link *link = lst_link_find_by_id( lst, id);
		if( link)
		{
			t_node *node = ( t_node *) link->data;
			t_object *object = ( t_object *) node->data;
			return object;
		}
	}
	
	return NULL;
}

int lu_lib_every_frame( lua_State *L)
{
	luaL_checktype( L, 1, LUA_TFUNCTION);
	lua_tocfunction( L, 1);
	lua_setglobal( L, "every_frame_function");
	LUA_EVERY_FRAME = 1; 
	return 1;
}

int lu_lib_mesh_set( lua_State *L)
{
	int id = luaL_checkinteger( L, 1);	// Object ID
	luaL_checktype( L, 2, LUA_TFUNCTION);	// CFunc
	lua_tocfunction( L, 2);

	lua_setglobal( L, "call");

	t_context *C = ctx_get();
	t_object *object = get_object( C, id);
	t_mesh *mesh = object->mesh;
	t_vlst *vlst = mesh->vertex;
	int count = vlst->count;
	float *v = ( float *) vlst->data;
	int i;

	float x,y,z;
	float rx, ry, rz;

	for( i = 0; i < count; i++)
	{
		x = v[0];
		y = v[1];
		z = v[2];

		lua_getglobal( L, "call");

		lua_pushinteger( L, i);
		lua_pushnumber( L, x);
		lua_pushnumber( L, y);
		lua_pushnumber( L, z);

		lua_pcall( L, 4, 3,0);

		rx = luaL_checknumber( L, 2);
		ry = luaL_checknumber( L, 3);
		rz = luaL_checknumber( L, 4);

		v[0] = rx;
		v[1] = ry;
		v[2] = rz;

		v += 3;

		lua_pop( L, 3);
	}

	return 0;
}

void lu_lib_object_build( t_lua_stone *lua_stone)
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

	lu_lib_object_add( object);

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

void lu_lib_register( lua_State *L, int (* f)( lua_State *L), const char *name)
{
	lua_pushcfunction( L, f);
	lua_setglobal( L, name);
}

void lu_lib_init( lua_State *L)
{
	lu_lib_register( L, lu_lib_every_frame, "every_frame");
	lu_lib_register( L, lu_lib_mesh_set, "set_mesh");
	lu_lib_register( L, lu_lib_time, "time");
}

