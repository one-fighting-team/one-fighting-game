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

typedef struct {
	enum { STANDING, JUMPING, KICKING_LEFT, KICKING_RIGHT } state;
	bool button_pushed;
	bool last_kicker;
	ssize_t x;
	ssize_t y;
	bool hit;
} Character;

static void display_characters(const Character *player_1, const Character *player_2)
{
	fprintf(stderr, "\r                                       "); // TODO
	fprintf(stderr, "\rp1: %zd, %zd vs. p2: %zd, %zd",
		player_1->x, player_1->y, player_2->x, player_2->y);
}

static void compute_next_step(Character *character, Character *opponent)
{
	if (character->button_pushed) {
		switch (character->state) {
		case STANDING:
			character->state = JUMPING;
			break;
		case JUMPING:
			character->state =
				(character->x < opponent->x
					? KICKING_RIGHT
					: KICKING_LEFT);
			character->last_kicker = true;
			opponent->last_kicker  = false;
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
		character->y++;
		break;
	case KICKING_LEFT:
		character->y--;
		character->x--;
		break;
	case KICKING_RIGHT:
		character->y--;
		character->x++;
		break;
	}

	if (   character->x == opponent->x
	    && character->y == opponent->y)
		character->hit = true;

	if (character->y == 0)
		character->state = STANDING;
}

static void exit_winner(int winner)
{
	fprintf(stderr, "\nwinner is player %d\n", winner);
	exit(winner);
}

static void configure_inputs()
{
	int status = fcntl(0, F_SETFL, O_NONBLOCK);
	if (status < 0) {
		perror("fcntl");
		exit(3);
	}

	// TODO: stty cbreak
}

int main()
{
	Character player_1 = {0};
	Character player_2 = {0};
	player_2.x = 10;

	configure_inputs();

	while (1) {
		display_characters(&player_1, &player_2);

		compute_next_step(&player_1, &player_2);
		compute_next_step(&player_2, &player_1);

		if (player_1.hit || player_2.hit) {
			if (player_1.last_kicker)
				exit_winner(1);
			else
				exit_winner(2);
		}

		usleep(30000);

		char c;
		while (read(STDIN_FILENO, &c, 1) > 0) {
			if (c == 'q')
				player_1.button_pushed = true;
			if (c == 'p')
				player_2.button_pushed = true;
		}
	}
}
