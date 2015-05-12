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

#define YBORDER 30
#define XBORDER 120
#define YMAX YBORDER-2
#define YMIN 1
#define XMAX XBORDER-2
#define XMIN 1

#define MAXPLAYERS 10

typedef struct {
	enum { DISABLED, STANDING, JUMPING, KICKING_LEFT, KICKING_RIGHT, FALLING, STICK } state;
	bool button_pushed;
	int kick_number;
	bool direction;
	ssize_t x;
	ssize_t y;
	bool hit;
	char avatar;
} Character;

/* Context */
int last_kick = 0;
int nb_players = 0;

static int middle_pos (int min, int max, int len)
{
	return (max+min)/2-len/2;
}

static void display_characters(WINDOW *win, const Character players[])
{
	int p;
	for (p=0;p<MAXPLAYERS;p++)
	{
		if (players[p].state==DISABLED) continue;
		mvwprintw(win, 1, 4+(p*15), "%d(%zd, %zd, %zd)", p, players[p].state, players[p].x, players[p].y);
	}
	
	wrefresh(win);
}

static int check_collision(WINDOW *win, Character *character, Character players[])
{
	int p;
	Character *opponent;
	
	for (p=0;p<MAXPLAYERS;p++)
	{
		opponent = &players[p];

		if (character == opponent) 
			continue;
			
		if (opponent->state==DISABLED)
			continue;

		if (   character->x == opponent->x
		    && character->y == opponent->y)
		{
			if (character->kick_number < opponent->kick_number)
			{
				character->hit = true;
				character->avatar=' ';
				character->state = DISABLED;
				nb_players--;
			}
			else
			{
				opponent->hit = true;
				opponent->avatar=' ';
				opponent->state = DISABLED;
				nb_players--;
			}
		}
		if (character->x < XMIN) {
			character->state = STICK;
			character->x++;
		} else if (character->x > XMAX) {
			character->state = STICK;
			character->x--;
		}

		if (character->y >= YMAX)
		{
			character->state = STANDING;
		}

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
		case STICK:
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
	case STICK:
		character->state = FALLING;
		character->y++;
		break;
	}
}

static void compute_next_step(WINDOW *win, Character players[])
{
	int p;

	/* choose the best direction : TBD */
	int xaverage=(XMIN+XMAX)/2;
	
	for (p=0;p<MAXPLAYERS;p++)
	{
		if (players[p].state==DISABLED) continue;
		compute_next_player_step(&players[p], (players[p].x<xaverage) );

		check_collision(win, &players[p], players);
	}
}

static void exit_winner(WINDOW *win, Character players[])
{
	const char* msg = "THE ONE WINNER IS";
	int xpos = middle_pos (XMIN, XMAX, strlen(msg));
	int ypos = middle_pos (YMIN, YMAX, 0);
	int winner=-1, p;
	for (p=0;p<MAXPLAYERS;p++)
	{
		if (players[p].state!=DISABLED) 
		{
			winner=p;
			break;
		}
	}
		
	wattron(win, A_BOLD);
	wattron(win, COLOR_PAIR(1));
	mvwprintw(win, ypos, xpos, msg);
	xpos = middle_pos (XMIN, XMAX, 8);
	mvwprintw(win, ypos+1, xpos, "PLAYER %c", players[winner].avatar);
	wrefresh(win);
	sleep(5);
	endwin();
	exit(winner);
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

static void erase_all_characters(WINDOW *win, const Character players[])
{
	int p;
	for (p=0;p<MAXPLAYERS;p++)
	{
		if (players[p].state==DISABLED) continue;
		mvwaddch(win, players[p].y, players[p].x, ' ');
	}
}

static void draw_all_characters(WINDOW *win, const Character players[])
{
	int p;
	for (p=0;p<MAXPLAYERS;p++)
	{
		if (players[p].state==DISABLED) continue;
		wattron(win, COLOR_PAIR(2));
		mvwaddch(win, players[p].y, players[p].x, players[p].avatar | A_BOLD);
		wattroff(win, COLOR_PAIR(2));
	}
	wrefresh(win);
}


static void color_init() 
{
	start_color();
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_BLUE, COLOR_BLACK);
	init_pair(3, COLOR_GREEN, COLOR_BLACK);
	init_pair(4, COLOR_CYAN, COLOR_BLACK);
	init_pair(5, COLOR_YELLOW, COLOR_BLACK);
}


int main(int argc, char **argv)
{

	WINDOW *title, *board, *debug;
	int ch;
	int pos;
	const char* banner = "*** The ONE Fighting Game ***";
	int i, p=0;
	
	Character players[MAXPLAYERS] = {0};
	
	for (i=1; i<argc; i++)
	{
		players[p].y = YMAX;
		players[p].x = (random()%(XMAX-XMIN))+XMIN;
		players[p].avatar = *argv[i];
		players[p].state = STANDING;
		p++;
	}
	nb_players = p;
	
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

	while (1) {
		display_characters(debug, players);

		erase_all_characters(board, players);
		compute_next_step(board, players);
		draw_all_characters(board, players);

		if (nb_players==1) 
		{
			exit_winner(board, players);
		
		}
		usleep(30000);

		ch = getch();
		for (p=0; p<MAXPLAYERS; p++)
		{
			if (ch == players[p].avatar) {
				players[p].button_pushed = true;
			}
		}
	}
	endwin();
}
