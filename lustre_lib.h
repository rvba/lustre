/* 
 * Copyright (c) 2015 Milovann Yanatchkov 
 *
 * This file is part of Lustre, a free software
 * licensed under the GNU General Public License v2
 * see /LICENSE for more information
 *
 */

#ifndef __LUSTRE_LIB_H__
#define __LUSTRE_LIB_H__

struct lua_State;
int lua_every_frame_call( struct lua_State *L);
void lua_minuit_init( struct lua_State *L);

#endif
