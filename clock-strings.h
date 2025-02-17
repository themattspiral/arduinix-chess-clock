#ifndef _CLOCK_STRINGS_H
#define _CLOCK_STRINGS_H

#define playerString(leftPlayersTurn) (leftPlayersTurn ? "Left" : "Right")

const char* NEW_GAME_MSG = "New Game Started! Turn Limit: %s | Starting Player: %s";
const char* TURN_CHANGE_MSG = "Your Move!";
const char* TIMEOUT_MSG = "Game Over! %s Player Timed Out";

#endif _CLOCK_STRINGS_H
