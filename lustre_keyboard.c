/* 
 * Copyright (c) 2015 Milovann Yanatchkov 
 *
 * This file is part of Lustre, a free software
 * licensed under the GNU General Public License v2
 * see /LICENSE for more information
 *
 */

#include "ctx.h"
#include "event.h"
#include "app.h"
#include "lustre.h"

void editor_keyboard( int key)
{
	t_context *C = ctx_get();
	
	// Main Keymap
	keymap_main( key);

	switch( key)
	{
		case TABKEY:
			if(C->app->keyboard->ctrl) editor_open( C);
		       	break;
	}
}

