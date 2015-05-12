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
#include <stdlib.h>    // exit(3)
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

typedef struct {
	enum { STANDING, JUMPING, KICKING_LEFT, KICKING_RIGHT, FALLING, STICK } state;
	bool button_pushed;
	bool last_kicker;
	ssize_t x;
	ssize_t y;
	bool hit;
	char avatar;
} Character;

static int middle_pos (int min, int max, int len)
{
	return (max+min)/2-len/2;
}

static void display_characters(WINDOW *win, const Character *player_1, const Character *player_2)
{
	mvwprintw(win, 1, 4, "p1: %zd, %zd vs. p2: %zd, %zd",
			player_1->x, player_1->y, player_2->x, player_2->y);
	wrefresh(win);
}

static void compute_next_step(Character *character, Character *opponent)
{
	if (character->button_pushed) {
		switch (character->state) {
		case STANDING:
			character->state = JUMPING;
			break;
		case JUMPING:
		case FALLING:
			character->state =
				(character->x < opponent->x
					? KICKING_RIGHT
					: KICKING_LEFT);
			character->last_kicker = true;
			opponent->last_kicker  = false;
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

	if (   character->x == opponent->x
	    && character->y == opponent->y)
		character->hit = true;

	if (character->x < XMIN) {
		character->state = STICK;
		character->x++;
	} else if (character->x > XMAX) {
		character->state = STICK;
		character->x--;
	}

	if (character->y == YMAX)
		character->state = STANDING;
}

static void exit_winner(WINDOW *win, int winner)
{
	const char* msg = "THE ONE WINNER IS";
	int xpos = middle_pos (XMIN, XMAX, strlen(msg));
	int ypos = middle_pos (YMIN, YMAX, 0);
	wattron(win, A_BOLD);
	wattron(win, COLOR_PAIR(1));
	mvwprintw(win, ypos, xpos, msg);
	xpos = middle_pos (XMIN, XMAX, 8);
	mvwprintw(win, ypos+1, xpos, "PLAYER %d", winner);
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

static void erase_characters(WINDOW *win, const Character *player_1, const Character *player_2)
{
	mvwaddch(win, player_1->y, player_1->x, ' ');
	mvwaddch(win, player_2->y, player_2->x, ' ');
}

static void draw_characters(WINDOW *win, const Character *player_1, const Character *player_2)
{
	wattron(win, COLOR_PAIR(2));
	mvwaddch(win, player_1->y, player_1->x, player_1->avatar | A_BOLD);
	wattroff(win, COLOR_PAIR(2));
	wattron(win, COLOR_PAIR(3));
	mvwaddch(win, player_2->y, player_2->x, player_2->avatar | A_BOLD);
	wattroff(win, COLOR_PAIR(3));
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

int main()
{
	WINDOW *title, *board, *debug;
	int ch;
	int pos;
	const char* banner = "*** The ONE Fighting Game ***";
	Character player_1 = {0};
	Character player_2 = {0};
	player_1.y = YMAX;
	player_2.y = YMAX;
	player_1.x = XMIN+20;
	player_2.x = XMAX-20;
	player_1.avatar = 'o';
	player_2.avatar = 'x';

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
		display_characters(debug, &player_1, &player_2);

		erase_characters(board, &player_1, &player_2);
		compute_next_step(&player_1, &player_2);
		compute_next_step(&player_2, &player_1);
		draw_characters(board, &player_1, &player_2);

		if (player_1.hit || player_2.hit) {
			if (player_1.last_kicker)
				exit_winner(board, 1);
			else
				exit_winner(board, 2);
		}

		usleep(30000);

		ch = getch();
		switch (ch) {
			case 'q':
				player_1.button_pushed = true;
				break;
			case 'p':
				player_2.button_pushed = true;
				break;
			default:
				break;
		}
	}
	endwin();
}
