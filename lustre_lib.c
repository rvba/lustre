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

#include "lustre_lib.h"

#include "node.h"
#include "scene.h"
#include "object.h"
#include "ctx.h"
#include "app.h"
#include "base.h"
#include "mesh.h"
#include "vlst.h"

#define L_MINUIT "L_MINUIT"

lua_CFunction LUA_FRAME = NULL;
int LUA_EVERY_FRAME = 0;

int mlua_time( lua_State *L)
{
	double t = clock_now_sec_precise();
	lua_pushnumber( L, clock_now_sec_precise());
	return 1;
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

int lua_every_frame_call( lua_State *L)
{
	if( LUA_EVERY_FRAME)
	{
		t_context *C = ctx_get();
		lua_getglobal( L, "every_frame_function");
		if( lua_pcall( L, 0,0,0) != LUA_OK)
		{
			mn_lua_error();
			return 0;
		}
	}
	return 0;
}

int lua_every_frame( lua_State *L)
{
	luaL_checktype( L, 1, LUA_TFUNCTION);
	lua_tocfunction( L, 1);
	lua_setglobal( L, "every_frame_function");
	LUA_EVERY_FRAME = 1; 
	return 1;
}

int lua_set_mesh( lua_State *L)
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

int lua_get_object( lua_State *L)
{
	int id = luaL_checkinteger( L, 1 );
	t_context *C = ctx_get();
	t_node *node = scene_get_node_by_id( C->scene, id);
	if( node)
	{
		t_object *object = ( t_object *) node->data;
		printf("object:%s\n", object->id.name);
	}
	return 0;
}

void mlua_register( lua_State *L, int (* f)( lua_State *L), const char *name)
{
	lua_pushcfunction( L, f);
	lua_setglobal( L, name);
}

void lua_minuit_init( lua_State *L)
{
	mlua_register( L, lua_every_frame, "every_frame");
	mlua_register( L, lua_get_object, "get_object");
	mlua_register( L, lua_set_mesh, "set_mesh");
	mlua_register( L, mlua_time, "time");
}

