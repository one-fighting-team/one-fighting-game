/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of the One Fighting Game
 *
 * Copyright (C) 2015 the One Fighting Team.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <sys/types.h> // ssize_t
#include <stdlib.h>    // exit(3), random(3)
#include <stdio.h>     // fprintf(3), perror(3)
#include <unistd.h>    // usleep(3), fcntl(2)
#include <fcntl.h>
#include <stdbool.h>
#include <ncurses.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define YBORDER 30
#define XBORDER 120
#define YMAX YBORDER-2
#define YMIN 1
#define XMAX XBORDER-2
#define XMIN 1

typedef struct {
	enum { STANDING, JUMPING, KICKING_LEFT, KICKING_RIGHT, FALLING } state;
	bool button_pushed;
	int kick_number;
	bool direction;
	ssize_t x;
	ssize_t y;
	bool hit;
	char avatar;
	int color;
} Character;

typedef struct Squad{
	Character *c;
	struct Squad *next;
	struct Squad *prev;
} Squad;

/* Context */
int last_kick = 0;

static int middle_pos (int min, int max, int len)
{
	return (max+min)/2-len/2;
}

static void display_character(WINDOW *win, Character player, int p)
{
	mvwprintw(win, 1, 4+(p*15), "%c(%zd, %zd, %zd)", player.avatar,
		       player.kick_number, player.x, player.y);
	wrefresh(win);
}

static void exit_winner(WINDOW *win, Character winner)
{
	const char* msg = "THE ONE WINNER IS";
	int xpos = middle_pos (XMIN, XMAX, strlen(msg));
	int ypos = middle_pos (YMIN, YMAX, 0);
	wattron(win, A_BOLD);
	wattron(win, COLOR_PAIR(1));
	mvwprintw(win, ypos, xpos, msg);
	xpos = middle_pos (XMIN, XMAX, 8);
	mvwprintw(win, ypos+1, xpos, "PLAYER %c", winner.avatar);
	wrefresh(win);
	sleep(5);
	endwin();
	exit(0);
}

static void configure_inputs()
{
	int status = fcntl(0, F_SETFL, O_NONBLOCK);
	if (status < 0) {
		perror("fcntl");
		exit(3);
	}

	cbreak();
}

static void color_init()
{
	start_color();
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_BLUE, COLOR_BLACK);
	init_pair(3, COLOR_GREEN, COLOR_BLACK);
	init_pair(4, COLOR_CYAN, COLOR_BLACK);
	init_pair(5, COLOR_YELLOW, COLOR_BLACK);
	init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(7, COLOR_WHITE, COLOR_BLACK);
}

static void add_player(Squad **l, Character *c)
{
	Squad *p = malloc(sizeof(Squad));
	if (NULL == p) exit(1);
	p->c = c;
	p->prev = NULL;
	p->next = *l;
	if (*l) (*l)->prev = p;
	
	*l = p;
}

static void remove_hitted_fighters(Squad **list)
{
	Squad *l = *list;
	while (l)
	{
		if (l->c->hit)
		{
			if (l->next) l->next->prev = l->prev;
			if (l->prev) l->prev->next = l->next;
			else *list = l->next;
			free(l->c);
			free(l);
		}
		l = l->next;
	}
}

static void draw_fighters(WINDOW *win, Squad *l)
{
	while (l)
	{
		wattron(win, COLOR_PAIR(l->c->color % 8));
		mvwaddch(win, l->c->y, l->c->x, l->c->avatar | A_BOLD);
		wattroff(win, COLOR_PAIR(l->c->color % 8));
		l = l->next;
	}
	wrefresh(win);

}

static int erase_fighters(WINDOW *win, Squad *l)
{
	int cpt = 0;
	while (l)
	{
		mvwaddch(win, l->c->y, l->c->x, ' ');
		l = l->next;
		cpt++;
	}
	return cpt;
}

static Squad *select_fighters(WINDOW *win, WINDOW *win2)
{
	// TODO: Implement it as a timer. 

	Squad *list = NULL;
	int xpos, ypos, ch, cpt = 0;
	const char *msg = "Choose your avatar Figthers (Enter to stop)";

	xpos = middle_pos(XMIN, XMAX, strlen(msg));
	ypos = middle_pos(YMIN, YMAX, 0);
	wattron(win, A_BOLD);
	mvwaddstr(win, ypos, xpos, msg);
	wrefresh(win);

	while(1){
		Character *c;
		ch = getch();
		if (ch == '\n') break;
		if (isascii(ch) && ch != ' ' && ch != '\t')
		{
			cpt++;
			c = (Character *)malloc(sizeof(Character));
			c->y = YMAX;
			c->x = (random()%(XMAX-XMIN))+XMIN;
			c->avatar = ch;
			c->state = STANDING;
			c->kick_number = 0;
			c->color = cpt;
			add_player(&list, c);
			display_character(win2, *c, cpt);
		}
	}

	wclear(win);
	box(win, 0, 0);
	return list;
}

static void debug_list(WINDOW *win, Squad *l)
{
	int cpt = 0;
	wclrtoeol(win);
	box(win, 0, 0);
	while(l)
	{
		cpt++;
		display_character(win, *l->c, cpt);
		l = l->next;
	}
}

static void check_button(Squad *l)
{
	int ch = getch();
	while (l)
	{
		if (ch == l->c->avatar)
			l->c->button_pushed = true;
		l = l->next;
	}
}

static void check_collision(WINDOW *win, Character *c, Squad *l)
{
	while (l)
	{
		if (c->x < XMIN) {
			c->x++;
		} else if (c->x > XMAX) {
			c->x--;
		}

		if (c->y >= YMAX) c->state = STANDING;

		if (c == l->c) 
		{
			l = l->next;
			continue;
		}

		if (c->x == l->c->x && c->y == l->c->y)
		{
			if (c->kick_number < l->c->kick_number)
				c->hit = true;
			else
				l->c->hit = true;
		}

		l = l->next;
	}
}

static void compute_next_player_step(Character *character, int kick_right)
{
	if (character->button_pushed) {
		switch (character->state) {
		case STANDING:
			character->state = JUMPING;
			break;
		case JUMPING:
		case FALLING:
			if ( kick_right ) 
				character->state = KICKING_RIGHT;
			else
				character->state = KICKING_LEFT;
			character->kick_number = last_kick++;
			break;
		case KICKING_LEFT:
		case KICKING_RIGHT:
			// TODO
			break;
		}
		character->button_pushed = false;
	}

	switch (character->state) {
	case STANDING:
		break;
	case JUMPING:
		character->y--;
		if (character->y < YMIN) {
			character->state = FALLING;
			character->y++;
		}
		break;
	case FALLING:
		character->y++;
		break;
	case KICKING_LEFT:
		character->y++;
		character->x--;
		break;
	case KICKING_RIGHT:
		character->y++;
		character->x++;
		break;
	}
}

static void compute_next_step(WINDOW *win, Squad *fighters, int nbp)
{
	Squad *i = fighters;
	while (i)
	{
		Squad *j = fighters;
		int nbr = 0;
		while (j)
		{
			if (i->c->x < j->c->x) nbr++;
			j = j->next;
		}

		compute_next_player_step(i->c, nbr >= nbp/2);
		check_collision(win, i->c, i);
		i = i->next;
	}
}

int main(int argc, char **argv)
{

	WINDOW *title, *board, *debug;
	int pos;
	const char* banner = "*** The ONE Fighting Game ***";
	int nbp = 0;
	
	Squad *fighters = NULL;

	configure_inputs();

	initscr();
	curs_set(0);
	noecho();
	color_init();

	title = subwin(stdscr, 3, XBORDER, 0 ,0 );
	board = subwin(stdscr, YBORDER, XBORDER, 3, 0);
	debug = subwin(stdscr, 3, XBORDER, YBORDER+3, 0);

	box(title,0,0);
	box(board,0,0);
	box(debug,0,0);

	wattron(title, A_BOLD);
	wattron(title, COLOR_PAIR(1));
	pos = middle_pos(XMIN, XMAX, strlen(banner));
	mvwaddstr(title, 1, pos, banner);
	refresh();

	fighters = select_fighters(board, debug);

	while (1) {
		debug_list(debug, fighters);

		nbp = erase_fighters(board, fighters);
		compute_next_step(board, fighters, nbp);
		remove_hitted_fighters(&fighters);
		draw_fighters(board, fighters);

		if (nbp == 1) exit_winner(board, *fighters->c);
		
		usleep(30000);

		check_button(fighters);
	}
	endwin();
}
