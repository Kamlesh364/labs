#!/usr/bin/env python

from bug2 import Bug2
from map import Map
from logger import Logger

import curses

def show_map(robot, tab_token = "\n\t\t\t\t"):
    string = tab_token
    string += '   |' + ('-' * robot.map.width) + "|"  + tab_token
    for y in range(0, robot.map.height):
        temp = robot.map.map[y]
        if robot.current.y == y:
            temp = temp[:robot.current.x] + '@' + temp[robot.current.x+1:]
        string += ("{0:2}".format(repr(y))) + ":|" + temp + "|" + tab_token
    string += '   |' + ('-' * robot.map.width) + "|" 

    return string

if __name__ == '__main__':
    l = Logger()
    b = Bug2(Map('maps/map3', 20, 15), l)
    #b = Bug2(Map('maps/map2'), l)
    found, x = None, False

    #print show_map(b)
    screen = curses.initscr()
    curses.start_color()
    curses.init_pair(1, curses.COLOR_RED, curses.COLOR_WHITE)

    while x != ord('4'):
        screen.clear()
        screen.addstr(7, 20, "Line:  {0}".format(b.goal_line))
        screen.addstr(8, 20, "Moves: {0}".format(b.moves))
        screen.addstr(10, 20, show_map(b))
        screen.addstr(0, 0, l.show(4), curses.color_pair(1))

        if found:
            screen.addstr(4, 50, "Item found!", curses.color_pair(1))
        else:
            found = b.move()

        screen.refresh()
        x = screen.getch()

    curses.endwin()
