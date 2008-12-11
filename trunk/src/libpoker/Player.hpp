/*
 * Copyright 2008, Dominik Geyer
 *
 * This file is part of HoldingNuts.
 *
 * HoldingNuts is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HoldingNuts is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HoldingNuts.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _PLAYER_H
#define _PLAYER_H

#include "HoleCards.hpp"

class Player
{
friend class GameController;

public:
	typedef enum {
		ResetAction,
		Check,
		Fold,
		Call,
		Bet,
		Raise,
		Allin,
		Show
	} PlayerAction;
	
	typedef struct {
		bool valid;
		PlayerAction action;
		float amount;
	} SchedAction;
	
	Player();
	
private:
	int client_id;
	
	float stake;
	
	HoleCards holecards;
	
	SchedAction next_action;
};


#endif /* _PLAYER_H */