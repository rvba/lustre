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
#include "viewport.h"
#include "scene.h"
#include "lustre.h"
#include "render.h"
#include "sketch.h"
#include "draw.h"

#ifdef HAVE_FREETYPE
#include "txt.h"
static int LU_HAVE_FREETYPE = 1;
#else
static int LU_HAVE_FREETYPE = 0;
#endif

#define LU_EDITOR_COMMAND 1
#define LU_EDITOR_INSERT 2
#define LU_EDITOR_SELECT 3

#define LU_RENDER_BITMAP 1
#define LU_RENDER_STROKE 2
#define LU_RENDER_TTF 3

static int LU_USE_PAGES = 0;
static int LU_USE_STROKE = 0;
static int LU_FILE_CURRENT = 0;
t_file *LU_FILES[10] = {NULL};
t_file *LU_FILE = NULL;
int LU_INIT = 0;
static int LU_EDITOR_DEBUG = 0;

t_file *LU_DB = NULL;

//static float LU_SCALE = .1;
static float LU_SCALE = 0.1;
static float LU_MIN_SCALE = 0.03;
static float LU_MAX_SCALE = 10;
//static float LU_FOCUS_X = 0;
//static float LU_FOCUS_Y = 0;

static int LU_MODE = LU_EDITOR_COMMAND;
static int lu_cursor_x = 0;
static int lu_cursor_y = 0;
static int lu_cursor_current_line = 0;

static int sx = 0;
static int sy = 0;

static int lu_editor_line_count;
static int lu_line_height = 20;

/* global dim for mono types */
#ifdef HAVE_FREETYPE
static float lu_char_height = 0;
static float lu_char_width = 0;
#endif

static int lu_console_line_count = 3;
static int lu_editor_margin_top = 50;

static int lu_select_init;
static int lu_select_start_point;
static int lu_select_start_line;
static int lu_select_end_line;

static void * lu_editor_font = GLUT_BITMAP_9_BY_15;
static int lu_editor_file_warning = 0;

static int LU_RENDER = LU_RENDER_BITMAP;
static void (* lu_func_draw_letter) ( int letter) = NULL;
static void lu_draw_letter_bitmap( int letter);
static void lu_draw_letter_vector( int letter);
static int lu_use_number = 0;
static int lu_use_debug = 0;
static int lu_use_debug_bb = 0;
static int lu_use_debug_bb_frame = 0;
static int lu_use_status = 0;
static int lu_use_db = 0;
static int lu_is_db = 0;
static int lu_start_ttf = 0;
static int lu_use_autofocus = 1;

static t_screen *LU_SCREEN = NULL;


// Utils

void lu_editor_set( const char *name, void *val)
{
	if( is(name,"pages"))
	{
		LU_USE_PAGES = *((int*)val);
	}
}

void lu_switch( int *target)
{
	*target = !(*target);
}

inline void lu_set_render( int render)
{
	LU_RENDER = render;
}

void lu_switch_rendering( void)
{
	if( LU_RENDER == LU_RENDER_BITMAP && LU_HAVE_FREETYPE) LU_RENDER = LU_RENDER_TTF;
	else if( LU_RENDER == LU_RENDER_BITMAP)
	{	
		if(LU_USE_STROKE) LU_RENDER = LU_RENDER_STROKE;
		else LU_RENDER = LU_RENDER_TTF;

	}
	else if( LU_RENDER == LU_RENDER_TTF)
	{
		if(LU_USE_STROKE) LU_RENDER = LU_RENDER_STROKE;
		else LU_RENDER = LU_RENDER_BITMAP;
	}	
	else LU_RENDER = LU_RENDER_BITMAP;
}

inline int lu_is_render( int render)
{
	if( LU_RENDER == render) return 1;
	else return 0;
}

static int lu_iseditkey( int key)
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

t_line *lu_line_get( int pos)
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
		lu_line_get( pos);
	}

	return NULL;
}

#ifdef HAVE_FREETYPE

float LU_BBOX_WIDTH = 0;
float LU_BBOX_HEIGHT = 0;

float LU_BBOX_MIN_X = 0;
float LU_BBOX_MAX_X = 0;
float LU_BBOX_MIN_Y = 0;
float LU_BBOX_MAX_Y = 0;

void lu_bbox_do(float x, float y)
{
	if ( x > LU_BBOX_MAX_X) LU_BBOX_MAX_X = x;
	if ( x < LU_BBOX_MIN_X) LU_BBOX_MIN_X = x;

	if ( y > LU_BBOX_MAX_Y) LU_BBOX_MAX_Y = y;
	if ( y < LU_BBOX_MIN_Y) LU_BBOX_MIN_Y = y;
	if(LU_EDITOR_DEBUG)
	{
		printf("bbox: x:%f y:%f scale:%f \n", x, y, LU_SCALE);
		printf("check max_x:%f width:%f\n",LU_BBOX_MAX_X,LU_BBOX_WIDTH);
	}
}

void lu_bbox_reset( void)
{
	LU_BBOX_MIN_X = 0;
	LU_BBOX_MAX_X = 0;
	LU_BBOX_MIN_Y = 0;
	LU_BBOX_MAX_Y = 0;
}

void lu_bbox_debug( const char *msg)
{
	/*
	float box_width = (LU_BBOX_MAX_X - LU_BBOX_MIN_X) ;
	float box_height = (LU_BBOX_MAX_Y - LU_BBOX_MIN_Y) ;
	float screen_width = LU_BBOX_WIDTH;
	float screen_height = LU_BBOX_HEIGHT;
	printf("%s:: sx:%d sy:%d sc:%0.1f bw:%0.1f bh:%0.1f sw:%0.1f sh:%0.1f\n", msg, sx, sy, LU_SCALE,box_width,box_height,screen_width,screen_height);
	*/

	if( lu_use_debug_bb) printf("%s", msg);

}

void lu_bbox_check( void)
{
	float var = 0.01;
	float box_width = (LU_BBOX_MAX_X - LU_BBOX_MIN_X) * LU_SCALE ;
	float box_height = (LU_BBOX_MAX_Y - LU_BBOX_MIN_Y) * LU_SCALE ;

	// hack
	box_width = sx;
	box_height = sy;

	float screen_width = LU_BBOX_WIDTH;
	float screen_height = LU_BBOX_HEIGHT;

	t_viewport *v = screen_viewport_get( LU_SCREEN);
	float top = v->top;
	float width = v->right;
	screen_height = top;
	screen_width = width;

	//printf("left:%f right:%f \n", v->left, v->right);
	//if( box_width > screen_width  && (box_width > 200))
	
	float error = 10;
	if( (box_width > (screen_width + error)))
	{
		lu_bbox_debug("w-");
		//LU_SCALE -= var;
		LU_SCALE *= 1 - var;
	}
	else if((box_width < (screen_width - error)) &&(box_height < (screen_height - error)))
	{
		lu_bbox_debug("w+");
		//LU_SCALE += var;
		LU_SCALE *= 1 + var;
	}
	else if( box_height > (screen_height + error))
	{
		lu_bbox_debug("h-");
		//LU_SCALE -= var;
		LU_SCALE *= 1 - var;
	}
	else if( (box_height < (screen_height - error)) && box_width < (screen_width - error))
	{
		lu_bbox_debug("h+");
		//LU_SCALE += var;
		LU_SCALE *= 1 + var;
	}
	else
	{
		lu_bbox_debug("_");
	}

	if( lu_use_debug_bb) printf(" %f < sc:%0.2f < %f bw:%0.1f sw:%0.1f bh:%0.1f sh:%0.1f \n", LU_MIN_SCALE, LU_SCALE, LU_MAX_SCALE, box_width, screen_width, box_height, screen_height);

	if( LU_SCALE > LU_MAX_SCALE) LU_SCALE = LU_MAX_SCALE;
	if( LU_SCALE < LU_MIN_SCALE) LU_SCALE = LU_MIN_SCALE;
}
#endif


/*...not used...*/
#if 0
static int lu_line_length( void)
{
	if( LU_FILE)
	{
	t_line *line = lu_line_get(0);
	if( !line) return 0;
	else
	{
		if( line->data) { return strlen( line->data); }
		else return 0;
	}
	}
	else
		return 0;
}
#endif

static char lu_get_char_under_cursor()
{
	t_line *line = lu_line_get( lu_cursor_y);
	char *data = ( char *) line->data;
	return *( data + lu_cursor_x);
}

void lu_cursor_show()
{
	printf("cursor: x:%d y:%d char:[%c]\n", lu_cursor_x, lu_cursor_y, lu_get_char_under_cursor());
}

void lu_key_show( int key)
{
	printf("This key:%d %s\n", key, event_name(key));
}

void lu_mod_show( t_context *C)
{
	if( C->app->keyboard->ctrl) printf("CTRL ");
}

void lu_key_debug( t_context *C, int key)
{
	lu_mod_show( C);
	lu_key_show( key);
}

// Cursor

int lu_editor_cursor_blink( void)
{
	double tt = clock_now_sec_precise();
	tt *= 10;
	long int xxx = (long int) tt;
	int ty = (int) xxx;
	if( ty % 8 == 0)
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
	t_line *line = lu_line_get( lu_cursor_y);
	if( line)
	{
		int char_count = line->size;
		lu_cursor_x = char_count -1; // lu_cursor_x starts from 0
	}
}

static void lu_editor_cursor_jump_to_line( int dir)
{
	t_line *line = lu_line_get( lu_cursor_y);
	if( line)
	{
		int char_count = line->size;
		if( lu_cursor_x > char_count ) lu_cursor_x = char_count -1; // lu_cursor_x starts from 0
		if( char_count == 1) lu_cursor_x = 0;
	}
}

static void lu_editor_cursor_move( int dir)
{
	t_line *line = lu_line_get( lu_cursor_y);
	int line_count = LU_FILE->tot_line;
	int char_count = line->size;
	switch(dir)
	{
		case UP_KEY:
			if( lu_cursor_y > 0) lu_cursor_y--;
			lu_editor_cursor_jump_to_line( dir);
			if( lu_cursor_y < lu_cursor_current_line) lu_cursor_current_line--;
		break;

		case DOWN_KEY:
			if( lu_cursor_y < line_count - 1) lu_cursor_y++;
			lu_editor_cursor_jump_to_line( dir);
			if( lu_cursor_y > lu_cursor_current_line + lu_editor_line_count) lu_cursor_current_line++;
		break;

		case RIGHT_KEY:
			if( lu_cursor_x < char_count -1) lu_cursor_x++;
		break;

		case LEFT_KEY:
			if( lu_cursor_x > 0) lu_cursor_x--;
		break;
	}

	if( LU_EDITOR_DEBUG) lu_cursor_show();

}

// Actions

static void lu_editor_char_delete( int offset)
{
	t_line *line = lu_line_get( lu_cursor_y );
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
						t_line *line = lu_line_get( lu_cursor_y);
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
					t_line *line_before = lu_line_get( lu_cursor_y - 1);
					int pos = line_before->size;
					file_line_join( LU_FILE, lu_cursor_y -1, lu_cursor_y);
					lu_cursor_y--;
					lu_cursor_x = pos - 1;
				}
			}
		}
	}

	//lu_bbox_do( lu_char_width * lu_cursor_x * LU_SCALE, lu_char_height * lu_cursor_y * LU_SCALE);
}

static void lu_editor_char_add( int key)
{
	if( !lu_iseditkey(key))
	{
		return;
	}
	
	if( LU_FILE)
	{
		t_line *line = lu_line_get( lu_cursor_y );
		if( line)
		{
			char c = ( char) key;
			line_add_data( line, lu_cursor_x, 1, &c);
			lu_cursor_x++;
			//lu_bbox_do( lu_char_width * lu_cursor_x * LU_SCALE, lu_char_height * lu_cursor_y * LU_SCALE);
		}
	}
}

static void lu_editor_line_split( void)
{
	t_line *line = lu_line_get( lu_cursor_y);
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

static void lu_editor_cmd_save( void)
{
	file_write_lines( LU_FILE);
}

void lu_editor_open( struct Context *C)
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
}

static void lu_editor_close( t_context *C)
{
	t_node *screen_node_editor=scene_get_node_by_type_name( C->scene, dt_screen, "screen_editor");
	t_screen *screen_editor=screen_node_editor->data;

	screen_editor->is_active = 0;
	screen_editor->is_visible = 0;

	t_node *screen_node=scene_get_node_by_type_name( C->scene, dt_screen, "screen_main");
	t_screen *screen_main=screen_node->data;
	C->ui->screen_active = screen_main;
}

int lu_editor_cmd_exec( void)
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

// File

static void lu_editor_cmd_file_new( void)
{
	LU_FILE = file_new("sketch.lua");
	file_make( LU_FILE);
	file_line_add( LU_FILE, 0, "");
	LU_INIT = 1;
	lu_cursor_x = 0;
	LU_FILES[LU_FILE_CURRENT] = LU_FILE;
	
}

static void lu_editor_db_open( void)
{
	if( !LU_DB)
	{
		//t_file
		LU_DB = file_new("db.lua");
		file_line_add( LU_DB, 0, "(debug) >  ");
	}
}

void lu_editor_db_clear( void)
{
	//leaks ?
	file_free( LU_DB);
	LU_DB = NULL;
}

void lu_editor_log_add( const char *msg)
{
	lu_editor_db_open();
	//t_file
	/*
	 * tot_line -> print 3 (last
	 * tot_line -1 print debug (first)
	 * */
	file_line_add( LU_DB, LU_DB->tot_line, msg);
	//file_line_add( LU_DB, 1, msg);
	//file_line_add( LU_DB, 2, "hello");
}

static void lu_editor_op_file_open(void)
{
	t_context *C=ctx_get();

	t_file *LU_FILE = file_new( C->app->path_file);
	file_init(LU_FILE);

	if(is(LU_FILE->ext,"lua"))
	{
		lu_lua_load_file( LU_FILE->path);
	}
	else
	{
		printf("Not a lua file\n");
	}

	file_free( LU_FILE);
}

static void *lu_editor_cmd_file_open( void)
{
	t_context *C = ctx_get();
	lu_editor_close( C);
	browser_enter( C, lu_editor_op_file_open);
	return NULL;
}

void lu_editor_file_open( void)
{
	LU_FILE = file_new(LU_FILE_PATH);

	if( file_exists( LU_FILE))
	{
		if( file_open( LU_FILE, "r"))
		{
			printf("[lustre] loading %s\n", LU_FILE_PATH);
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

void lu_editor_page_change( int page)
{
	printf("chnge!!\n");
	LU_FILE = LU_FILES[page];
	LU_FILE_CURRENT = page;
}

// Keymap

void lu_editor_keymap( int key)
{
	t_context *C = ctx_get();

	if( lu_use_debug) lu_key_debug( C, key);

	if( LU_FILE)
	{
		// Commands
		if( C->app->keyboard->ctrl)
		{
			switch( key)
			{

				case 19: lu_editor_cmd_save(); break;	// S
				case 5: lu_editor_cmd_exec(); break;	// E
				case 15: lu_editor_cmd_file_open(); break;	// O
				case 20: lu_switch_rendering(); break;	// T
				case 1: lu_lua_exec_auto(); break;	// A

				case 14:lu_switch(&lu_use_number); break;	// N
				case 4: lu_switch(&lu_use_debug); break; // D
				case 2: lu_switch(&lu_use_db); break; //B 
				case 6: lu_switch(&lu_use_autofocus); break;	// F

				case TABKEY: lu_editor_close( C); break;

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

			// SELECT MODE
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

			// INSERT MODE
			else if( LU_MODE == LU_EDITOR_INSERT)
			{
				switch(key)
				{
					case DELKEY: lu_editor_char_delete(1); break; 
					case BACKSPACEKEY: lu_editor_char_delete( 0); break;
					case RETURNKEY: lu_editor_line_split(); break;
					case ESCKEY: LU_MODE = LU_EDITOR_COMMAND;break;
					default: lu_editor_char_add( key); break;
				}
			}

			// COMMAND MODE
			else
			{
				switch(key)
				{
					case IKEY: LU_MODE = LU_EDITOR_INSERT;break;
					case 43: LU_SCALE += .02; break;
					case 45: LU_SCALE -= .02; break;
					case PKEY: paint_start(C);break;


					default: keymap_command( key); break;

				}

				/* Pages */
				if( LU_USE_PAGES)
				{
					switch(key)
					{
						case 48: /*0*/
						case 49:
						case 50:
						case 51:
						case 52:
						case 53:
						case 54:
						case 55:
						case 56:
						case 57:
							lu_editor_page_change(key-48);
							;break;
					}
				}
			}
		}
	}
	// NO FILE
	else
	{
		if( C->app->keyboard->ctrl)
		{
			switch( key)
			{
				case 14: lu_editor_cmd_file_new(); break;	// N
				case 15: lu_editor_cmd_file_open(); break;	// O
				case TABKEY: lu_editor_close( C); break;
			}

		}
		else
		{
			switch( key)
			{
				case ESCKEY: break;
				case TABKEY: lu_editor_close( C); break;

			}

			if(LU_USE_PAGES)
			{
				switch( key)
				{

					/* Pages */
					case 48: /*0*/
					case 49:
					case 50:
					case 51:
					case 52:
					case 53:
					case 54:
					case 55:
					case 56:
					case 57:
						lu_editor_page_change(key-48);
						;break;
				}
			}
		}
	}
}

// Screen

static void lu_draw_cursor( void)
{
	float width = 60;
	float height = 300;
	float t = 100;
	float y = 280;
	
	glPushMatrix();
	glTranslatef(t,y,0);

	t_context *C = ctx_get();
	float *f = C->ui->front_color;
	glColor3f(f[0],f[1],f[2]);

	glBegin(GL_QUADS);
	glVertex2f(width,-height);
	glVertex2f(width,height);
	glVertex2f(-width,height);
	glVertex2f(-width,-height);
	glEnd();
	glPopMatrix();
}


static void lu_draw_letter_bitmap( int letter)
{
	t_context *C = ctx_get();
	float *f = C->ui->front_color;
	glColor3f(f[0],f[1],f[2]);
	glutBitmapCharacter(lu_editor_font,letter);
}

static void lu_draw_letter_vector( int letter)
{
	glutStrokeCharacter(GLUT_STROKE_MONO_ROMAN, letter);
}

#ifdef HAVE_FREETYPE

static void lu_draw_letter_ttf( int letter)
{
	txt_ttf_draw_char( letter);
	//printf("letter %c width: %f\n", (char) letter, txt_ttf_glyph_get_width( letter)); 
}
#endif

void lu_editor_draw_line_number( int y)
{
	char pos[6]; 
	sprintf(pos, "%d   ", y);
	//glColor3f(.5,.5,.5);

	lu_func_draw_letter( pos[0]);
	lu_func_draw_letter( pos[1]);
	lu_func_draw_letter( pos[2]);
	lu_func_draw_letter( pos[3]);
}

int lu_editor_draw_line( char *string, int y, int blink)
{
	int x=0;
	char *letter;
	int b;
	if( lu_is_render( LU_RENDER_STROKE)) b = (int) '_';
	else b = 2;

	for( letter = string; *letter; letter++)
	{
		int c = ( int) *letter;

		// lu_editor_cursor_blink
		
		if( lu_is_render(LU_RENDER_TTF))
		{
			if( blink && lu_cursor_x == x && lu_cursor_y == y && lu_editor_cursor_blink())
				lu_draw_cursor();

			if( c == 9 )
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
		}
		else
		if( blink && lu_cursor_x == x && lu_cursor_y == y && lu_editor_cursor_blink())
		{
			//tab
			if( c == 9 )
			{
				lu_func_draw_letter( b);
				lu_func_draw_letter( b);
				lu_func_draw_letter( b);
				lu_func_draw_letter( b);

			}
			else
			{
				if( lu_is_render(LU_RENDER_TTF))
				{
					lu_draw_cursor();
				}
				else
				{
					if( lu_is_render( LU_RENDER_STROKE)) glColor3f(.5,.5,.5);
					lu_func_draw_letter( b);
					if( lu_is_render( LU_RENDER_STROKE)) glColor3f(1,1,1);
				}
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
		if( lu_is_render(LU_RENDER_TTF))
		{
			lu_draw_cursor();
		}
		else
		{
			lu_func_draw_letter( b);
		}
	}
	return x;
}

void lu_editor_draw_line_empty( int lx, int ly)
{
	if( lu_cursor_y == ly && lu_editor_cursor_blink())
	{
		if( lu_is_render(LU_RENDER_TTF))
		{
			lu_draw_cursor();
		}
		else if( lu_is_render( LU_RENDER_STROKE))
		{
			glutStrokeCharacter(GLUT_STROKE_MONO_ROMAN, '_');
		}
		else
		{
			glutBitmapCharacter(lu_editor_font,2);
		}
	}
}

void lu_editor_draw_console( t_context *C)
{
	int pos = ((lu_editor_line_count + 1)  * lu_line_height);
	glPushMatrix();
	glTranslatef( 0, -pos, 0);
	glRasterPos2i(0,0);

	glDisable(GL_POINT);

	if( lu_use_status)
	{

	if( LU_MODE == LU_EDITOR_COMMAND) lu_editor_draw_line( "COMMAND ", 0, 0);
	else if( LU_MODE == LU_EDITOR_SELECT) lu_editor_draw_line( "SELECT ", 0, 0);
	else lu_editor_draw_line( "INSERT ", 0, 0);
	}

	if( LU_DEBUG_STATE) lu_editor_draw_line( LU_DEBUG_MSG, 0, 0);

	glEnable(GL_POINT);

	glPopMatrix();
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
			t_context *C = ctx_get();
			float *f = C->ui->front_color;
			glColor3f(f[0],f[1],f[2]);
		}
	}
	else
	{
		t_context *C = ctx_get();
		float *f = C->ui->front_color;
		glColor3f(f[0],f[1],f[2]);
	}
}

void lu_editor_init( t_context *C)
{
	float *f = C->ui->front_color;
	glColor3f(f[0],f[1],f[2]);

	int wh = C->app->window->height;
	int h = 20;
	lu_editor_line_count = (( wh - lu_editor_margin_top) / h) - lu_console_line_count;

	if( lu_is_render(LU_RENDER_TTF) && LU_HAVE_FREETYPE)
	{
		#ifdef HAVE_FREETYPE
		lu_func_draw_letter = lu_draw_letter_ttf;
		#endif
	}
	else if(lu_is_render( LU_RENDER_STROKE))
	{
		lu_func_draw_letter = lu_draw_letter_vector;
	}
	else
	{
		lu_func_draw_letter = lu_draw_letter_bitmap;
	}	

}

int old_length = 0;
int sign;


void lu_db( void)
{
	static int done = 0;
	if( !done)
	{
		done = 1;
		#ifdef HAVE_FREETYPE
		#if 0
		t_viewport *v = screen_viewport_get( LU_SCREEN);
		printf("viewport values:\n");
		printf("left:%f right:%f bottom:%f top:%f near:%f far: %f\n", v->left, v->right, v->bottom, v->top, v->near, v->far);
		printf("a: %f %f\n",txt_ttf_glyph_get_width((int)'a'), txt_ttf_glyph_get_height('a'));
		printf("b: %f %f\n",txt_ttf_glyph_get_width((int)'b'), txt_ttf_glyph_get_height('b'));
		printf("c: %f %f\n",txt_ttf_glyph_get_width((int)'c'), txt_ttf_glyph_get_height('c'));
		printf("d: %f %f\n",txt_ttf_glyph_get_width((int)'d'), txt_ttf_glyph_get_height('d'));
		printf("e: %f %f\n",txt_ttf_glyph_get_width((int)'e'), txt_ttf_glyph_get_height('e'));
		#endif
		#endif
	}
}

void lu_editor_draw_start( t_context *C)
{
	glPushMatrix();
	glLoadIdentity();
	float *f = C->ui->front_color;
	glColor3f(f[0],f[1],f[2]);

	if( lu_use_autofocus && lu_is_render( LU_RENDER_TTF))
	{
		#ifdef HAVE_FREETYPE
		/* debug screen */
		t_viewport *v = screen_viewport_get( LU_SCREEN);

		glDisable(GL_LIGHTING);
		glDisable(GL_DEPTH_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glPolygonMode(GL_FRONT,GL_FILL);

		double margin = 1;

		/* Draw Frame */
		if(lu_use_debug_bb_frame)
		{
			glBegin(GL_LINE_LOOP);
			glVertex3f( v->left + margin, v->top - margin ,0);
			glVertex3f( v->right - margin, v->top - margin ,0);
			glVertex3f( v->right - margin , v->bottom + margin ,0);
			glVertex3f( v->left + margin , v->bottom + margin ,0);
			glEnd();
		}

		//printf("left:%f right:%f %f\n", v->left + margin, v->right - margin);
		//printf("top:%f bottom:%f \n", v->top - margin, v->bottom + margin );

		//glTranslatef(v->left,v->top - 100 ,0);
		//float s = 0.1f;
		glTranslatef( 0, C->app->window->height, 0);
		float s = 1;
		glScalef(s * LU_SCALE, s * LU_SCALE, 1);
		float ssy = lu_char_height * LU_SCALE *2;
		glTranslatef( 0, -ssy, 0);
		lu_db();
		#endif

	}
	else
	{
		glTranslatef( 50, C->app->window->height - lu_editor_margin_top, 0);
	}

	if( lu_is_render(LU_RENDER_TTF))
	{
		/*
		float _s = .2;
		glScalef(LU_SCALE*_s,LU_SCALE*_s,LU_SCALE*_s);
		*/
	}
	else if( lu_is_render(LU_RENDER_STROKE))
	{
		glScalef(LU_SCALE,LU_SCALE,LU_SCALE);
	}

	glRasterPos2i(0,0);
}

void lu_editor_draw_file( t_context *C, t_file *file)
{

	int y = -20;
	int lx = 0;
	int ly = 0;
	int xcount = 0;
	int ycount = 1;
	t_link *l;

	if(lu_use_autofocus)  lu_bbox_reset();

	for( l = file->lines->first; l; l = l->next)
	{
		if( ly >= lu_cursor_current_line && ly < lu_cursor_current_line + lu_editor_line_count)
		{
			t_line *line = ( t_line *) l->data;

			glPushMatrix();

			if( lu_use_number) lu_editor_draw_line_number( ly + 1);
			lu_editor_draw_line_color( lx, ly);

			if( *line->data	)
			{	
				xcount = lu_editor_draw_line( line->data, ly, 1);
			}
			else
			{
				lu_editor_draw_line_empty( lx, ly);
			}

			glPopMatrix();

			if( lu_is_render(LU_RENDER_TTF) && LU_HAVE_FREETYPE)
			{
				#ifdef HAVE_FREETYPE
				txt_ttf_vertical_offset(-1);
				#endif
			}
			else if(lu_is_render( LU_RENDER_STROKE))
			{
				glTranslatef(0,-180,0);
			}
			else
			{
				glRasterPos2i(0,y);
			}


			y -= 20;
		}

		if(lu_use_autofocus)
		{
			sy = lu_char_height * ycount * LU_SCALE;
			sx = lu_char_width * xcount * LU_SCALE;
			lu_bbox_do( sx, sy);
		}

		ly += 1;
		lx = 0;
		xcount = 0;
		ycount += 1;

	}
 
	if(lu_use_autofocus)
	{
		lu_bbox_do( sx, sy);
		lu_bbox_check();
	}	

	if( !lu_is_db) lu_editor_draw_console( C);

	glPopMatrix();
}

void lu_editor_draw_db( t_context *C)
{
	lu_editor_db_open();

	int use_auto = lu_use_autofocus;
	lu_use_autofocus = 0;
	lu_is_db = 1;

	float width = (float)(C->app->window->width / 2);
	float height = (float)(C->app->window->height - lu_editor_margin_top);

	glPushMatrix();
	glLoadIdentity();
	float *f = C->ui->front_color;
	glColor3f(f[0],f[1],f[2]);
	glRasterPos2i(width,height);
				glRasterPos2i(width,height-100);

		int _lu_cursor_current_line  = lu_cursor_current_line;
		int _lu_cursor_x = lu_cursor_x;
		int _lu_cursor_y = lu_cursor_y;

		lu_cursor_x = 0;
		lu_cursor_y = 0;

		/* uncomment this to block scrolling */
		/*
		lu_cursor_current_line = 0;
		*/

		lu_editor_draw_file( C, LU_DB);

	glPopMatrix();

	lu_is_db = 0;
	lu_use_autofocus = use_auto;
	lu_cursor_current_line  = _lu_cursor_current_line;
	lu_cursor_x = _lu_cursor_x;
	lu_cursor_y = _lu_cursor_y;
}

void lu_editor_draw_prompt( t_context *C)
{
	lu_editor_draw_start( C);
	glPushMatrix();
	char msg[] = "";
	lu_editor_draw_line( msg, 0, 1);
	lu_cursor_x = strlen( msg);
	glPopMatrix();
	glPopMatrix();
}
int iinit = 0;

void lu_editor_screen( t_screen *screen)
{
	t_context *C = ctx_get();

	if( !LU_INIT && LU_LOAD)  lu_editor_file_open();

	screen_switch_2d( screen);
	lu_editor_init( C);
	glDisable(GL_POINT);

	LU_SCREEN = screen;

	lu_editor_draw_start( C);

	if( LU_FILE)	lu_editor_draw_file( C, LU_FILE);
	else		lu_editor_draw_prompt( C);

	if( lu_use_db) lu_editor_draw_db(C);

	glEnable(GL_POINT);

	if(!iinit)
	{
		iinit = 1;

		t_viewport *v = screen_viewport_get( LU_SCREEN);
		lu_bbox_reset();
		LU_BBOX_WIDTH = v->right;
		LU_BBOX_HEIGHT = v->top;
		if(LU_EDITOR_DEBUG)
			printf("set bbox %f %f\n",LU_BBOX_WIDTH, LU_BBOX_HEIGHT);
	}

	/*
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0,1.0);
	*/

	glEnable(GL_DEPTH_TEST);

}

t_screen *lu_editor_screen_init( t_context *C)
{
	t_screen *screen = screen_default( "screen_editor", lu_editor_screen);
	screen->keymap = lu_editor_keymap;

	#ifdef HAVE_FREETYPE
	if( ! txt_ttf_init())
	{
		LU_HAVE_FREETYPE=0;
	}
	else
	{
		lu_char_width = txt_ttf_glyph_get_width((int)'b');
		lu_char_height = txt_ttf_glyph_get_height((int)'b');
	}	

	#endif

	if( lu_start_ttf) lu_set_render( LU_RENDER_TTF);
	else lu_set_render( LU_RENDER_BITMAP);

	return screen;
};
