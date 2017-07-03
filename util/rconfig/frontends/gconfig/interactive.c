/*
 * util/rconfig/interactive.c
 * Copyright (C) 2017 Christopher Chianelli
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>

#include <rconfig.h>

#include "interactive.h"

#define ENTER (10)

static void clear_window_from(int pos);

static int interactive_bool(struct rconfig_config *conf)
{
	curs_set(0);
	move(3,0);
	printw("%s\nDefault: %s\n",conf->desc,(conf->default_val)? "YES" : "NO");

	move(5, 0);
	printw("Use Left/Right keys or y/n keys to navigate.\n"
	"Press enter to select the highlighted selection.\n");

	int action;
	int selected = conf->default_val;

	move(7,0);
	if (selected)
	{
		attrset(A_BOLD);
		printw("YES");
		attrset(A_NORMAL);
		printw("|");
		attrset(A_DIM);
		printw("no");
	}
	else
	{
		attrset(A_DIM);
		printw("yes");
		attrset(A_NORMAL);
		printw("|");
		attrset(A_BOLD);
		printw("NO");
	}
	refresh();

	while ((action = getch()) != ENTER)
	{
	    switch (action)
        {
            case KEY_LEFT: case 'y': case 'Y':
		        selected = 1;
				move(7,0);
				attrset(A_BOLD);
				printw("YES");
				attrset(A_NORMAL);
				printw("|");
				attrset(A_DIM);
				printw("no");
				refresh();
                break;

		    case KEY_RIGHT: case 'n': case 'N':
		        selected = 0;
				move(7,0);
				attrset(A_DIM);
				printw("yes");
				attrset(A_NORMAL);
				printw("|");
				attrset(A_BOLD);
				printw("NO");
				refresh();
	            break;
        }
    }

	return selected;
}

static int valid_number(const char *buf, int *num, int min, int max)
{
	int neg = 0;


	if (*buf == '-') {
		++buf;
		neg = 1;
	} else if (*buf == '\n') {
		move(6,0);
		attrset(A_BOLD);
		printw("No number entered, try again: ");
		attrset(A_NORMAL);
		return 0;
	}

	*num = 0;
	for (; *buf && *buf != '\n'; ++buf) {
		if (*buf < '0' || *buf > '9') {
			move(6,0);
			attrset(A_BOLD);
			printw("Invalid number, try again: ");
			attrset(A_NORMAL);
			return 0;
		}

		*num *= 10;
		*num += *buf - '0';
	}

	*num = neg ? -(*num) : *num;

	if (*num < min || *num > max) {
		move(6,0);
		attrset(A_BOLD);
		printw("Number out of range, try again: ");
		attrset(A_NORMAL);
		return 0;
	}

	return 1;
}

static int interactive_int(struct rconfig_config *conf)
{
	char buf[256];
	int num;
	int digits;
	int action;
	int cursor_loc;
	curs_set(1);

	printw("%s\nRange: %d-%d\nDefault: [%d]\n\n", conf->desc, conf->lim.min,
	       conf->lim.max, conf->default_val);

	digits = cursor_loc = sprintf(buf,"%d",conf->default_val);

	move(7,0);
	printw("%s",buf);

	do
	{
		move(7,cursor_loc);
	    while ((action = getch()) != ENTER)
	    {
	        switch (action)
            {
                case KEY_LEFT:
				    if (cursor_loc > 0) {
				        cursor_loc--;
					    move(7,cursor_loc);
						refresh();
				    }
				    break;

		        case KEY_RIGHT:
				    if (cursor_loc < digits) {
				        cursor_loc++;
				        move(7,cursor_loc);
						refresh();
					}
				    break;

				case KEY_BACKSPACE: case KEY_DC: case 127:
				    if (cursor_loc > 0) {
				        cursor_loc--;
						digits--;
					    move(7,cursor_loc);
					    strcpy(buf + cursor_loc,buf + cursor_loc + 1);
					    delch();
					    refresh();
				    }
					break;

				default:
				    if (('0' <= action && action <= '9') || (cursor_loc == 0 &&
						buf[0] != '-' && action == '-')) {
						strcpy(buf + cursor_loc + 1, buf + cursor_loc);
						buf[cursor_loc] = (char) action;
						insch(action);
						cursor_loc++;
						digits++;
						move(7,cursor_loc);
						refresh();

					}
					break;
            }
		}
    }
	while (!valid_number(buf, &num, conf->lim.min, conf->lim.max));

	return num;
}

static int interactive_options(struct rconfig_config *conf)
{
	char buf[256];
	int action;
	int choice = conf->default_val;
	size_t i;
	curs_set(0);

	printw("%s [%d]\n", conf->desc, conf->default_val);
	for (i = 0; i < conf->opts.num_options; ++i)
	{
		if (i + 1 == conf->default_val)
		{
			attrset(A_BOLD);
		}
		else
		{
			attrset(A_NORMAL);
		}
		printw("(%lu) %s\n", i + 1, conf->opts.options[i].desc);
	}

	while ((action = getch()) != ENTER)
	{
		switch (action)
	    {
	            case KEY_UP:
				    if (choice > 1)
					{
						choice++;
						chgat(-1, A_NORMAL, 1, NULL);
						move(choice + 3,0);
						chgat(-1, A_BOLD, 1, NULL);
						refresh();
					}
					break;

			    case KEY_DOWN:
				    if (choice < conf->opts.num_options)
					{
						choice--;
						chgat(-1, A_NORMAL, 1, NULL);
						move(choice + 3,0);
						chgat(-1, A_BOLD, 1, NULL);
						refresh();
					}
	    }
	}

	return choice;
}

#define CURR_BUFSIZE 256

static char current_file[CURR_BUFSIZE];
static char current_section[CURR_BUFSIZE];

/*
 * config_interactive:
 * Interactive rconfig callback function which prompts the user for input.
 */

 static void clear_window_from(int pos)
 {
	 int win_width, win_height;
	 int restore = pos;
	 getmaxyx(stdscr, win_width, win_height);
	 while (pos <= win_height) {
		 move(pos,0);
		 clrtoeol();
		 pos++;
	 }
	 move(restore,0);
 }
int config_interactive(struct rconfig_config *conf)
{
	int ret;

	if (strcmp(conf->section->name, current_section) != 0) {
		if (strcmp(conf->section->file->name, current_file) != 0) {
			clear_window_from(0);
			strncpy(current_file, conf->section->file->name,
			        CURR_BUFSIZE);
			attrset(A_STANDOUT | A_UNDERLINE);
			printw("%s Configuration", current_file);
		}
		else
		{
			clear_window_from(1);
		}
		move(1,0);
		attrset(A_BOLD);
		strncpy(current_section, conf->section->name, CURR_BUFSIZE);
		printw("%s", current_section);
	}
	else
	{
		clear_window_from(2);
	}

	attrset(A_NORMAL);
	move(3,0);
	switch (conf->type) {
	case RCONFIG_BOOL:
		ret = interactive_bool(conf);
		break;
	case RCONFIG_INT:
		ret = interactive_int(conf);
		break;
	case RCONFIG_OPTIONS:
		ret = interactive_options(conf);
		break;
	default:
	    endwin();
		exit(1);
	}

	return ret;
}
