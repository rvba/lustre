/* 
 * Copyright (c) 2015 Milovann Yanatchkov 
 *
 * This file is part of Lustre, a free software
 * licensed under the GNU General Public License v2
 * see /LICENSE for more information
 *
 */

#include <ctype.h>
#include "ctx.h"
#include "file.h"
#include "term.h"
#include "draw.h"
#include "app.h"
#include "ui.h"
#include "event.h"
#include "opengl.h"
#include "lst.h"
#include "base.h"
#include "node.h"
#include "screen.h"
#include "scene.h"
#include "lustre.h"

#define LU_EDITOR_COMMAND 1
#define LU_EDITOR_INSERT 2
#define LU_EDITOR_SELECT 3

t_file *LU_FILE = NULL;
int LU_INIT = 0;
static int LU_EDITOR_DEBUG = 0;
static float LU_SCALE = .1;

static int LU_MODE = LU_EDITOR_COMMAND;
static int lu_cursor_x = 0;
static int lu_cursor_y = 0;
static int lu_cursor_current_line = 0;

static int lu_editor_line_count;
static int lu_line_height = 20;
static int lu_console_line_count = 3;
static int lu_editor_margin_top = 50;

static int lu_select_init;
static int lu_select_start_point;
static int lu_select_start_line;
static int lu_select_end_line;

static void * lu_editor_font = GLUT_BITMAP_9_BY_15;
static int lu_editor_file_warning = 0;

static int lu_editor_stroke_rendering = 0;
static void (* lu_func_draw_letter) ( int letter) = NULL;
static void draw_letter_bitmap( int letter);
static void draw_letter_vector( int letter);

// Utils

static int iseditkey( int key)
{
	if(
		key == 8 || // BACKSPACE
		key == 9 || // HTAB
		(key >= 32 && key <= 126) 
	  )
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

t_line *line_get( int pos)
{
	int i = 0;
	t_link *l;
	if( LU_FILE->lines)
	{
		for( l = LU_FILE->lines->first; l; l = l->next)
		{
			t_line *line = ( t_line *) l->data;
			if ( i == pos) return line;
			i++;
		}
	}
	// File is empty, add first line
	else
	{
		file_line_add( LU_FILE, 0, "");
		line_get( pos);
	}

	return NULL;
}

static char char_under_cursor()
{
	t_line *line = line_get( lu_cursor_y);
	char *data = ( char *) line->data;
	return *( data + lu_cursor_x);
}

void cursor_show()
{
	printf("cursor: x:%d y:%d char:[%c]\n", lu_cursor_x, lu_cursor_y, char_under_cursor());
}

void key_show( int key)
{
	printf("This key:%d %s\n", key, event_name(key));
}

void mod_show( t_context *C)
{
	if( C->app->keyboard->ctrl) printf("CTRL ");
}

void key_debug( t_context *C, int key)
{
	mod_show( C);
	key_show( key);
}

// Cursor

int lu_editor_cursor_blink( void)
{
	double tt = clock_now_sec_precise();
	tt *= 10;
	long int xxx = (long int) tt;
	int ty = (int) xxx;
	if( ty % 4 == 0)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

static void lu_editor_cursor_move_at_end( void)
{
	t_line *line = line_get( lu_cursor_y);
	if( line)
	{
		int char_count = line->size;
		lu_cursor_x = char_count -1; // lu_cursor_x starts from 0
	}
}

static void editor_action_cursor_jump_line( int dir)
{
	t_line *line = line_get( lu_cursor_y);
	if( line)
	{
		int char_count = line->size;
		if( lu_cursor_x > char_count ) lu_cursor_x = char_count -1; // lu_cursor_x starts from 0
		if( char_count == 1) lu_cursor_x = 0;
	}
}

static void lu_editor_cursor_move( int dir)
{
	t_line *line = line_get( lu_cursor_y);
	int line_count = LU_FILE->tot_line;
	int char_count = line->size;
	switch(dir)
	{
		case UP_KEY:
			if( lu_cursor_y > 0) lu_cursor_y--;
			editor_action_cursor_jump_line( dir);
			if( lu_cursor_y < lu_cursor_current_line) lu_cursor_current_line--;
		break;

		case DOWN_KEY:
			if( lu_cursor_y < line_count - 1) lu_cursor_y++;
			editor_action_cursor_jump_line( dir);
			if( lu_cursor_y > lu_cursor_current_line + lu_editor_line_count) lu_cursor_current_line++;
		break;

		case RIGHT_KEY:
			if( lu_cursor_x < char_count -1) lu_cursor_x++;
		break;

		case LEFT_KEY:
			if( lu_cursor_x > 0) lu_cursor_x--;
		break;
	}

	if( LU_EDITOR_DEBUG) cursor_show();

}

// Actions

static void editor_action_delete( int offset)
{
	t_line *line = line_get( lu_cursor_y );
	if(line)
	{
		// If Line is One character long (empty)
		if( line->size == 1)
		{
			// Make sure it's not the last line
			if(LU_FILE->tot_line > 1)
			{
				// Remove this line
				line_remove( LU_FILE, lu_cursor_y);
				// Jump to previous line (or stay)
				if( lu_cursor_y + offset > 0)
				{
					lu_cursor_y = lu_cursor_y - 1 + offset;
					// if backspace go to end of previous line
					if( !offset) lu_editor_cursor_move_at_end();
				}
			}
		}
		// Regular case
		else
		{
			// If cursor not at line start (can be in suppr mode)
			if( lu_cursor_x  + offset > 0) // suppr can delete at start of line
			{
				// go backward (or stay)
				lu_cursor_x = lu_cursor_x -1 + offset;

				// delete a character
				
				// suppr
				if( offset)
				{
					// If end of line 
					// Check it's not the last line
					if( lu_cursor_y < LU_FILE->tot_line)
					{
						t_line *line = line_get( lu_cursor_y);
						// If end of line, join with next one
						if( line->size - 1 == lu_cursor_x) // before \0
						{
							// Check for next line to join
							if( lu_cursor_y < LU_FILE->tot_line-1)
							{
								file_line_join( LU_FILE, lu_cursor_y, lu_cursor_y + 1);
								if( lu_cursor_y > LU_FILE->tot_line) lu_cursor_y--;
							}
						}
						// else, delete a char
						else
						{
							line_remove_data( line, lu_cursor_x, 1);
						}
					}
				}
				// backspace
				else
				{
					// delete a char
					line_remove_data( line, lu_cursor_x, 1);
				}
			}
			// Backspace only
			else
			{
				// If 
				if( lu_cursor_y > 0)
				{
					// Join this line with previous one
					t_line *line_before = line_get( lu_cursor_y - 1);
					int pos = line_before->size;
					file_line_join( LU_FILE, lu_cursor_y -1, lu_cursor_y);
					lu_cursor_y--;
					lu_cursor_x = pos - 1;
				}
			}
		}
	}
}

static void editor_action_edit( int key)
{
	/*
	switch( key)
	{
		case 0:
		case 202:
		case LEFT_KEY:
		case RIGHT_KEY:
		case UP_KEY:
		case DOWN_KEY:

		       	return;
	}
	*/
	if( !iseditkey(key))
	{
		return;

	}
	
	if( LU_FILE)
	{
		t_line *line = line_get( lu_cursor_y );
		if( line)
		{
			char c = ( char) key;
			line_add_data( line, lu_cursor_x, 1, &c);
			lu_cursor_x++;
		}
	}
}

static void editor_action_split( void)
{
	t_line *line = line_get( lu_cursor_y);
	if( line)
	{
		line_split( LU_FILE, lu_cursor_y, lu_cursor_x);
		lu_cursor_y++;
		lu_cursor_x = 0;
	}
}

static void lu_editor_select_delete( void)
{
	int i;
	for( i = lu_select_end_line; i >= lu_select_start_line; i-- )
	{
		if( LU_FILE->tot_line > 0)
		{
			line_remove( LU_FILE, lu_select_start_line);
			lu_cursor_y--;
		}
	}

	lu_cursor_y++;
	lu_select_init = 0;
	LU_MODE = LU_EDITOR_COMMAND;
}

// Commands

static void editor_cmd_save( void)
{
	file_write_lines( LU_FILE);
}

void editor_open( struct Context *C)
{
	t_node *screen_node=scene_get_node_by_type_name( C->scene, dt_screen, "screen_editor");
	if( !screen_node)
	{
		screen_node=scene_get_node_by_type_name( C->scene, dt_screen, "screen_editor");
	}

	t_screen *screen_main=screen_node->data;
	C->ui->screen_active = screen_main;

	screen_main->is_active = 1;
	screen_main->is_visible = 1;
	/*
	printf("editor open\n");
	screen_switch_by_name( "screen_editor");
	*/
}

static void editor_close( t_context *C)
{
	t_node *screen_node_editor=scene_get_node_by_type_name( C->scene, dt_screen, "screen_editor");
	t_screen *screen_editor=screen_node_editor->data;

	screen_editor->is_active = 0;
	screen_editor->is_visible = 0;

	t_node *screen_node=scene_get_node_by_type_name( C->scene, dt_screen, "screen_main");
	t_screen *screen_main=screen_node->data;
	C->ui->screen_active = screen_main;

	/*
	printf("editor close!!\n");
	screen_switch_by_name( "screen_main");
	*/
}

int editor_cmd_exec( void)
{
	if( LU_FILE)
	{
		file_write_data( LU_FILE);
		lua_file = LU_FILE;
		LUA_EXEC = 1;
		return 1;
	}
	else
	{
		return 0;
	}
}

static void editor_cmd_new( void)
{
	LU_FILE = file_new("sketch.lua");
	file_make( LU_FILE);
	file_line_add( LU_FILE, 0, "");
	LU_INIT = 1;
	lu_cursor_x = 0;
}

static void op_editor_file_open(void)
{
	t_context *C=ctx_get();

	t_file *LU_FILE = file_new( C->app->path_file);
	file_init(LU_FILE);

	if(is(LU_FILE->ext,"lua"))
	{
		mn_lua_load_script( LU_FILE->path);
	}
	else
	{
		printf("Not a lua file\n");
	}

	file_free( LU_FILE);
}

static void *editor_file_open( void)
{
	t_context *C = ctx_get();
	editor_close( C);
	browser_enter( C, op_editor_file_open);
	return NULL;
}

// Keymap

void lu_editor_keymap( int key)
{
	t_context *C = ctx_get();

	if( LU_EDITOR_DEBUG) key_debug( C, key);

	if( LU_FILE)
	{
		// Commands
		if( C->app->keyboard->ctrl)
		{
			switch( key)
			{

				case 19: editor_cmd_save(); break;	// S
				case 5: editor_cmd_exec(); break;	// E
				case 15: editor_file_open(); break;	// O
				case 20: lu_editor_stroke_rendering = !lu_editor_stroke_rendering; break;	// T
				case 2: LU_EDITOR_DEBUG = !LU_EDITOR_DEBUG; break; //B
				case 1: mn_lua_exec_auto(); break;	// A

				case TABKEY: editor_close( C); break;

				case UP_KEY: 
				case DOWN_KEY:
				case LEFT_KEY:
				case RIGHT_KEY:
					lu_editor_cursor_move( key);
				break;
			}
		}
		else
		{
			// Actions
			switch(key)
			{
				case 0: break;
				case 200: break; // shift event problem (double event, see minuit)

				case UP_KEY: 
				case DOWN_KEY:
				case LEFT_KEY:
				case RIGHT_KEY:
					lu_editor_cursor_move( key);
				break;

				case UP_VKEY: LU_MODE = LU_EDITOR_SELECT; break;
			}

			// SELECT LU_MODE
			if( LU_MODE == LU_EDITOR_SELECT)
			{
				// Init selection point
				if( !lu_select_init)
				{
					lu_select_init = 1;
					lu_select_start_point = lu_cursor_y;
					lu_select_start_line = lu_cursor_y;
					lu_select_end_line = lu_cursor_y;
				}
				else
				{
					// Check if cursor > start point
					if( lu_cursor_y >= lu_select_start_point)
					{
						lu_select_start_line = lu_select_start_point;
						lu_select_end_line = lu_cursor_y;
					}
					// or < start point
					else
					{
						lu_select_start_line = lu_cursor_y;
						lu_select_end_line = lu_select_start_point;
					}
				}

				switch(key)
				{
					case ESCKEY: LU_MODE = LU_EDITOR_COMMAND; lu_select_init = 0;break;
					case DELKEY: lu_editor_select_delete(); break;
				}
			}

			// INSERT LU_MODE
			else if( LU_MODE == LU_EDITOR_INSERT)
			{
				switch(key)
				{
					case DELKEY: editor_action_delete(1); break; //SUPPR
					case BACKSPACEKEY: editor_action_delete( 0); break;
					case RETURNKEY: editor_action_split(); break;
					case ESCKEY: LU_MODE = LU_EDITOR_COMMAND;break;
					default: editor_action_edit( key); break;
				}
			}

			// COMMAND LU_MODE
			else
			{
				switch(key)
				{
					case IKEY: LU_MODE = LU_EDITOR_INSERT;break;
					case 43: LU_SCALE += .02; break;
					case 45: LU_SCALE -= .02; break;
					default: keymap_command( key); break;
				}
			}
		}
	}

	else
	{
		if( C->app->keyboard->ctrl)
		{
			switch( key)
			{
				case 14: editor_cmd_new(); break;	// N
				case 15: editor_file_open(); break;	// O
				case TABKEY: editor_close( C); break;
			}

		}
		else
		{
			switch( key)
			{
				case ESCKEY: break;
				case TABKEY: editor_close( C); break;
			}
		}
	}
}

// Screen

static void draw_letter_bitmap( int letter)
{
	glutBitmapCharacter(lu_editor_font,letter);
}

static void draw_letter_vector( int letter)
{
	glutStrokeCharacter(GLUT_STROKE_MONO_ROMAN, letter);
}

void lu_editor_draw_line_number( int y)
{
	char pos[6]; 
	sprintf(pos, "%d   ", y);
	glColor3f(.5,.5,.5);

	lu_func_draw_letter( pos[0]);
	lu_func_draw_letter( pos[1]);
	lu_func_draw_letter( pos[2]);
	lu_func_draw_letter( pos[3]);

}

void lu_editor_draw_line( char *string, int y, int blink)
{
	int x=0;
	char *letter;
	int b;
	if( lu_editor_stroke_rendering) b = (int) '_';
	else b = 2;

	for( letter = string; *letter; letter++)
	{
		int c = ( int) *letter;

		// lu_editor_cursor_blink
		if( blink && lu_cursor_x == x && lu_cursor_y == y && lu_editor_cursor_blink())
		{
			if( c == 9 )
			{
				lu_func_draw_letter( b);
				lu_func_draw_letter( b);
				lu_func_draw_letter( b);
				lu_func_draw_letter( b);

			}
			else
			{
				if( lu_editor_stroke_rendering) glColor3f(.5,.5,.5);
				lu_func_draw_letter( b);
				if( lu_editor_stroke_rendering) glColor3f(1,1,1);
			}
		}
		// tab
		else if( c == 9)
		{
			lu_func_draw_letter( 32);
			lu_func_draw_letter( 32);
			lu_func_draw_letter( 32);
			lu_func_draw_letter( 32);
		}
		else
		{
			lu_func_draw_letter( c);
		}

		x += 1;
	}
	
	// end of line
	if( blink && lu_cursor_x == x && lu_cursor_y == y && lu_editor_cursor_blink())
	{
		lu_func_draw_letter( b);
	}

}

void lu_editor_draw_line_empty( int lx, int ly)
{
	if( lu_cursor_y == ly && lu_editor_cursor_blink())
	{
		if( lu_editor_stroke_rendering)
		{
			glutStrokeCharacter(GLUT_STROKE_MONO_ROMAN, '_');
		}
		else
		{
			glutBitmapCharacter(lu_editor_font,2);
		}
	}
}

void lu_editor_draw_debug( t_context *C)
{
	int pos = ((lu_editor_line_count + 1)  * lu_line_height);
	glPushMatrix();
	glTranslatef( 0, -pos, 0);
	glRasterPos2i(0,0);


	if( LU_MODE == LU_EDITOR_COMMAND) lu_editor_draw_line( "COMMAND ", 0, 0);
	else if( LU_MODE == LU_EDITOR_SELECT) lu_editor_draw_line( "SELECT ", 0, 0);
	else lu_editor_draw_line( "INSERT ", 0, 0);

	if( LU_DEBUG_STATE) lu_editor_draw_line( LU_DEBUG_MSG, 0, 0);

	glPopMatrix();
}

void lu_editor_file_open( void)
{
	LU_FILE = file_new(LU_FILE_PATH);

	if( file_exists( LU_FILE))
	{
		if( file_open( LU_FILE, "r"))
		{
			printf("opening %s\n", LU_FILE_PATH);
			file_read( LU_FILE);
			file_read_lines(LU_FILE);
			LU_INIT = 1;
			file_close( LU_FILE);
		}
	}
	else
	{
		file_free( LU_FILE);
		LU_FILE = NULL;
		if( !lu_editor_file_warning)
		{
			printf("file not found %s\n", LU_FILE_PATH);
			lu_editor_file_warning = 1;
		}
	}
}

void lu_editor_draw_line_color( int lx, int ly)
{
	if( LU_MODE == LU_EDITOR_SELECT)
	{
		if(ly < lu_select_end_line && ly >= lu_select_start_line -1)
		{
			glColor3f(1,0,0);
		}
		else
		{
			glColor3f(1,1,1);
		}
	}
	else
	{
		glColor3f(1,1,1);
	}
}

void lu_editor_init( t_context *C)
{

	glColor3f(1,1,1);

	int wh = C->app->window->height;
	int h = 20;
	lu_editor_line_count = (( wh - lu_editor_margin_top) / h) - lu_console_line_count;

	if(lu_editor_stroke_rendering)
	{
		lu_func_draw_letter = draw_letter_vector;
	}
	else
	{
		lu_func_draw_letter = draw_letter_bitmap;
	}	
}

void lu_editor_draw_start( t_context *C)
{
	glPushMatrix();
	glLoadIdentity();
	glTranslatef( 50, C->app->window->height - lu_editor_margin_top, 0);

	if( lu_editor_stroke_rendering)
	{
		glScalef(LU_SCALE,LU_SCALE,LU_SCALE);
	}

	glRasterPos2i(0,0);
}

void lu_editor_draw_file( t_context *C)
{

	int y = -20;
	int lx = 0;
	int ly = 0;
	t_link *l;

	lu_editor_draw_start( C);

	for( l = LU_FILE->lines->first; l; l = l->next)
	{
		if( ly >= lu_cursor_current_line && ly < lu_cursor_current_line + lu_editor_line_count)
		{
			t_line *line = ( t_line *) l->data;

			glPushMatrix();

			lu_editor_draw_line_number( ly + 1);
			lu_editor_draw_line_color( lx, ly);

			if( *line->data	)	lu_editor_draw_line( line->data, ly, 1);
			else			lu_editor_draw_line_empty( lx, ly);

			glPopMatrix();

			if( lu_editor_stroke_rendering)
			{
				glTranslatef(0,-180,0);
			}
			else
			{
				glRasterPos2i(0,y);
			}


			y -= 20;
		}

		ly += 1;
		lx = 0;
	}


	lu_editor_draw_debug( C);

	glPopMatrix();
}

void lu_editor_draw_prompt( t_context *C)
{
	lu_editor_draw_start( C);
	glPushMatrix();
	char msg[] = ">";
	lu_editor_draw_line( msg, 0, 1);
	lu_cursor_x = strlen( msg);
	glPopMatrix();
	glPopMatrix();
}

void lu_editor_screen( t_screen *screen)
{
	t_context *C = ctx_get();

	if( !LU_INIT && LU_LOAD)  lu_editor_file_open();

	screen_switch_2d( screen);
	lu_editor_init( C);
	glDisable(GL_POINT);

	if( LU_FILE)	lu_editor_draw_file( C);
	else		lu_editor_draw_prompt( C);

	glEnable(GL_POINT);
}

t_screen *lu_editor_screen_init( t_context *C)
{
	t_screen *screen = screen_default( "screen_editor", lu_editor_screen);
	screen->keymap = lu_editor_keymap;
	return screen;
};
