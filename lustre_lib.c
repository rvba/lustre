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
#include "dict.h"

#include "stone.h"
#include "stone_lua.h"

static t_dict *lu_lib_objects = NULL;
static int lu_lib_screen_init = 0;
int LU_EVERY_FRAME = 0;

int lu_lib_time( lua_State *L)
{
	lua_pushnumber( L, clock_now_sec_precise());
	return 1;
}

// Add and removes Mesh Objects

void lu_lib_object_add( struct Object *obj)
{
	if (!lu_lib_objects) lu_lib_objects = dict_make("lustre");
	t_symbol *symbol = dict_pop(lu_lib_objects,obj->id.name);
	if( !symbol)
	{
		printf("added %s\n", obj->id.name);
		dict_symbol_add( lu_lib_objects, obj->id.name, 0, obj);
	}
	else
	{
		printf("[lustre] Error can't add existing name: %s\n", obj->id.name);
	}
}

void lu_lib_objects_delete( void)
{
	t_context *C = ctx_get();
	t_scene *sc = C->scene;

	if( lu_lib_objects)
	{
		t_dict *dict = lu_lib_objects;
		if(dict->symbols)
		{
			t_link *l;
			t_symbol *s;
			for(l=dict->symbols->first;l;l=l->next)
			{
				s = l->data;
				t_object *obj = ( t_object *) s->data;
				scene_node_delete( C->scene, obj->id.node);
			}

			// delete dict
			scene_delete(sc,dict);
			lu_lib_objects = NULL;
		}
	}
}

static t_object *lu_lib_object_get( t_context *C, const char *name)
{
	if( lu_lib_objects)
	{
		t_symbol *symbol = dict_pop(lu_lib_objects,name);
		if( symbol)
		{
			t_object *object = ( t_object *) symbol->data;
			return object;
		}
		else
		{
			printf("[lustre] Error Object not found %s\n ", name);
			return NULL;
		}
	}
	else
	{
		printf("[lustre] Error No object is stored\n ");
		return NULL;
	}
}

int lu_lib_every_frame( lua_State *L)
{
	luaL_checktype( L, 1, LUA_TFUNCTION);
	lua_tocfunction( L, 1);
	lua_setglobal( L, "every_frame_function");
	LU_EVERY_FRAME = 1; 
	return 1;
}

int lu_lib_mesh_set( lua_State *L)
{
	const char *name = luaL_checkstring( L, 1);	// Object ID
	luaL_checktype( L, 2, LUA_TFUNCTION);	// CFunc
	lua_tocfunction( L, 2);

	lua_setglobal( L, "call");

	t_context *C = ctx_get();
	t_object *object = lu_lib_object_get( C, name);
	t_mesh *mesh = object->mesh;
	t_vlst *vlst = mesh->vertex;
	if( vlst)
	{

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
	}
	else
	{
		printf("[lustre] Error, can't find mesh for object: %s\n", object->id.name);

	}

	return 0;
}

int lu_lib_mesh_update( lua_State *L)
{
	//printf("[lustre] Update\n");
	t_context *C = ctx_get();

	const char *name = luaL_checkstring( L, 1);	
	int indice = luaL_checkinteger( L, 2);
	float x = luaL_checknumber( L, 3);
	float y = luaL_checknumber( L, 4);
	float z = luaL_checknumber( L, 5);

	t_object *object = lu_lib_object_get( C, name);
	t_mesh *mesh = object->mesh;
	t_vlst *vlst = mesh->vertex;
	float *v = ( float *) vlst->data;

	v[indice*3+0] = x;
	v[indice*3+1] = y;
	v[indice*3+2] = z;

	return 0;
}

int lu_lib_set_object_position( lua_State * L)
{
	//printf("[lustre] Update\n");
	t_context *C = ctx_get();

	const char *name = luaL_checkstring( L, 1);	
	float x = luaL_checknumber( L, 2);
	float y = luaL_checknumber( L, 3);
	float z = luaL_checknumber( L, 4);

	t_object *object = lu_lib_object_get( C, name);
	if( object)
	{

	object->loc[0] = x;
	object->loc[1] = y;
	object->loc[2] = z;
	}

	return 0;
}

int lu_lib_set_object_visibility( lua_State * L)
{
	t_context *C = ctx_get();

	const char *name = luaL_checkstring( L, 1);	
	int val = luaL_checkinteger( L, 2);

	t_object *object = lu_lib_object_get( C, name);
	if( object)
	{
		/* True means hidden */
		object->is_visible = !val;
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

	t_object *object = op_add_mesh_data( stone->name, 
			stone->vertex_count,
			quad_count,
			tri_count,
			vertex,
			quads,
			tris
			);


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

	if( !lu_lib_screen_init)
	{
		op_add_screen( NULL);
		op_add_light(NULL);
		lu_lib_screen_init = 1;
	}

	if( lua_stone->is_built)
	{
		t_object *obj = lu_lib_object_get( C, lua_stone->name);
		scene_node_delete( C->scene, obj->id.node);
		t_symbol *symbol = dict_pop(lu_lib_objects, lua_stone->name);
		symbol->data = object;
	}
	else
	{
		lu_lib_object_add( object);
	}


	lua_stone->is_built = 1;

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
	lu_lib_register( L, lu_lib_mesh_update, "update_mesh");
	lu_lib_register( L, lu_lib_set_object_position, "set_object_position");
	lu_lib_register( L, lu_lib_set_object_visibility, "set_object_visibility");
	lu_lib_register( L, lu_lib_time, "time");
}

