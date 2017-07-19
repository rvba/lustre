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
#include "sketch.h"
#include "draw.h"
#include "event.h"
#include "ui.h"

#include "mud.h"
#include "mud_lua.h"

static t_dict *lu_lib_objects = NULL;
static int lu_lib_screen_init = 0;
int LU_EVERY_FRAME = 0;
static int debug_set_mesh = 0;
static int debug_object_get = 0;

// void mesh_add_default_color(t_mesh *mesh)
// void draw_mesh(t_draw *draw, t_scene *scene, t_mesh *mesh)
// void skt_point(float *pos,int width,float *color) 
void lu_lib_dark( t_context *C)
{
	C->draw->color=color_black;

	vset4i(C->draw->background_color,0,0,0,0);
	vset4i(C->ui->background_color,0,0,0,0);
	vset4f(C->ui->front_color,1,1,1,0);
	vset4f(C->draw->front_color,1,1,1,0);
	vset4f(C->draw->back_color,0,0,0,0);
	vset4f(C->ui->back_color,0,0,0,0);

	C->event->color_transition = 0;
	C->event->color_transition_set = 0;
}

int lu_lib_time( lua_State *L)
{
	lua_pushnumber( L, clock_now_sec_precise());
	return 1;
}

int lu_lib_log_add( lua_State * L)
{
	const char *msg = luaL_checkstring( L, 1);	// Object ID
	lu_editor_log_add( msg);
	return 0;
}

int lu_lib_reshape( lua_State * L)
{
	int width = luaL_checkinteger( L, 1);
	int height = luaL_checkinteger( L, 2);
	t_context *C = ctx_get();
	C->app->window->width_def = width;
	C->app->window->height_def = height;
	app_gl_reshape(width,height);
	app_screen_set_fullscreen(C->app,0);
	return 0;
}

int lu_lib_win_move( lua_State * L)
{
	int x = luaL_checkinteger( L, 1);
	int y = luaL_checkinteger( L, 2);
	app_gl_move(x,y);
	return 0;
}

int lu_lib_win_get( lua_State *L)
{
	int pos_h = lu_lua_get_pos_h();
	int pos_v = lu_lua_get_pos_v();

	/* 1 top-right
	 * 2 down-right
	 * 3 down-left
	 * 4 top-left
	 */

	if( pos_v == LU_POS_TOP && pos_h == LU_POS_RIGHT) { lua_pushinteger( L, 1); }
	if( pos_v == LU_POS_DOWN && pos_h == LU_POS_RIGHT) lua_pushinteger( L, 2);
	if( pos_v == LU_POS_DOWN && pos_h == LU_POS_LEFT) lua_pushinteger( L, 3);
	if( pos_v == LU_POS_TOP && pos_h == LU_POS_LEFT) lua_pushinteger( L, 4);

	return 1;
}

int lu_lib_set( lua_State * L)
{
	const char *name = luaL_checkstring( L ,1);
	t_context *C = ctx_get();

	printf("[lustre] set:%s\n",name);
	if( is(name,"pages"))
	{
		int val = lua_toboolean( L, 2);
		lu_editor_set("pages",&val);
	}
	else if( is(name,"selection"))
	{
		int val = lua_toboolean( L, 2);
		C->draw->with_object_selection = val;
	}
	else if( is(name,"outline"))
	{
		int val = lua_toboolean( L, 2);
		C->event->with_face_outline = val;
	}
	else if( is(name,"line_width"))
	{
		float val = luaL_checknumber(L,2);
		C->skt->line_width = val;
	}
	else if( is(name,"point_size"))
	{
		float val = luaL_checknumber(L,2);
		C->skt->point_size = val;
	}
	else if( is(name,"draw_edge"))
	{
		int val = lua_toboolean( L, 2);
		C->event->with_edge = val;
	}
	else if( is(name,"edge_white"))
	{
		int val = lua_toboolean( L, 2);
		C->draw->edge_use_front_color = val;
	}
	else if( is(name,"draw_face"))
	{
		int val = lua_toboolean( L, 2);
		C->event->with_face = val;
	}
	else if( is(name,"debug_set_mesh"))
	{
		int val = lua_toboolean( L, 2);
		debug_set_mesh = val;
	}
	else if( is(name,"debug_object_get"))
	{
		int val = lua_toboolean( L, 2);
		debug_object_get = val;
	}
	else if( is(name,"dark"))
	{
		lu_lib_dark( C);
	}
	else
	{
		printf("[lustre] set not found:%s\n",name);
	}

	return 0;
}

int lu_lib_quitting( lua_State * L)
{
	t_context *C = ctx_get();
	lua_pushboolean( L, C->app->quit);
	return 1;
}

// Add and removes Mesh Objects

void lu_lib_object_add( struct Object *obj)
{
	if (!lu_lib_objects) lu_lib_objects = dict_make("lustre");
	t_symbol *symbol = dict_pop(lu_lib_objects,obj->id.name);
	if( !symbol)
	{
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
			printf("[lustre] ERROR Object get:not found %s\n", name);
			if( debug_object_get) dict_show( lu_lib_objects);
			return NULL;
		}
	}
	else
	{
		printf("[lustre] Error No object is stored\n");
		return NULL;
	}
}

static void print_jit_status(lua_State *L)
{
  int n;
  const char *s;
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, -1, "jit");  /* Get jit.* module table. */
  lua_remove(L, -2);
  lua_getfield(L, -1, "status");
  lua_remove(L, -2);
  n = lua_gettop(L);
  lua_call(L, 0, LUA_MULTRET);
  fputs(lua_toboolean(L, n) ? "JIT: ON" : "JIT: OFF", stdout);
  for (n++; (s = lua_tostring(L, n)); n++) {
    putc(' ', stdout);
    fputs(s, stdout);
  }
  putc('\n', stdout);
}

int lu_lib_every_frame( lua_State *L)
{
	luaL_checktype( L, 1, LUA_TFUNCTION);
	//lua_tocfunction( L, 1);
	lua_setglobal( L, "every_frame_function");
	/* fix luajit but break lua 5.1 attempt to call a nil value */
	/* in lua_every_frame_call */
	/* or just attempt to index a nil value from print_jit_status */
	#ifdef HAVE_LUAJIT
	print_jit_status(L);
	#endif
	LU_EVERY_FRAME = 1; 
	return 1;
}

int lu_lib_mesh_set( lua_State *L)
{
	if( debug_set_mesh) printf("[lustre] debug set mesh\n");
	const char *name = luaL_checkstring( L, 1);	// Object ID
	luaL_checktype( L, 2, LUA_TFUNCTION);		// CFunc
	lua_tocfunction( L, 2);

	lua_setglobal( L, "call");

	t_context *C = ctx_get();
	t_object *object = lu_lib_object_get( C, name);
	if( object)
	{
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
	}

	return 0;
}

int lu_lib_mesh_update( lua_State *L)
{
	t_context *C = ctx_get();

	const char *name = luaL_checkstring( L, 1);	
	int indice = luaL_checkinteger( L, 2);
	float x = luaL_checknumber( L, 3);
	float y = luaL_checknumber( L, 4);
	float z = luaL_checknumber( L, 5);

	t_object *object = lu_lib_object_get( C, name);
	if( object)
	{
		t_mesh *mesh = object->mesh;
		t_vlst *vlst = mesh->vertex;
		float *v = ( float *) vlst->data;

		v[indice*3+0] = x;
		v[indice*3+1] = y;
		v[indice*3+2] = z;
	}

	return 0;
}

int lu_lib_set_object_position( lua_State * L)
{
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
		object->is_visible = !val;
	}

	return 0;
}

void lu_lib_object_build( t_lua_mud *lua_mud)
{
	t_context *C = ctx_get();

	t_mud *mud = lua_mud->mud;
	mud_merge( mud, NULL);

	scene_store(C->scene,1);
	float *vertex = mud_get_vertex_buffer( mud);
	int quad_count = mud_get_quad_count( mud);
	int tri_count = mud_get_tri_count( mud);
	int *quads = mud_get_quad_buffer( mud, quad_count);
	int *tris = mud_get_tri_buffer( mud, tri_count);
	int *edges = mud_get_edge_buffer( mud);

	t_object *object = op_add_mesh_data( mud->name, 
			mud->vertex_count,
			quad_count,
			tri_count,
			vertex,
			quads,
			tris
			);


	if( edges)
	{
		int edge_count = mud->edge_count;
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
		lu_lib_screen_init = 1;
	}

	if( lua_mud->is_built)
	{
		t_object *obj = lu_lib_object_get( C, lua_mud->name);
		if( obj)
		{
			scene_node_delete( C->scene, obj->id.node);
			t_symbol *symbol = dict_pop(lu_lib_objects, lua_mud->name);
			symbol->data = object;
		}
	}
	else
	{
		lu_lib_object_add( object);
	}


	lua_mud->is_built = 1;

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
	lu_lib_register( L, lu_lib_log_add, "log");
	lu_lib_register( L, lu_lib_reshape, "reshape");
	lu_lib_register( L, lu_lib_win_move, "win_move");
	lu_lib_register( L, lu_lib_win_get, "win_get");
	lu_lib_register( L, lu_lib_quitting, "quitting");
	lu_lib_register( L, lu_lib_set, "set");
}

