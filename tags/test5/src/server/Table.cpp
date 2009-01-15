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


#include "Debug.h"
#include "Table.hpp"

using namespace std;


Table::Table()
{
	table_id = -1;
}

int Table::getNextPlayer(unsigned int pos)
{
	if (pos + 1 == seats.size())
		return 0;
	else
		return pos + 1;
}

int Table::getNextActivePlayer(unsigned int pos)
{
	unsigned int start = pos;
	unsigned int cur = pos;
	bool found = false;
	
	do
	{
		cur = getNextPlayer(cur);
		
		if (seats[cur].in_round)
			found = true;
		
		// no active player left
		if (start == cur)
			return -1;
	} while (!found);
	
	return cur;
}

unsigned int Table::countActivePlayers()
{
	unsigned int count = 0;
	
	for (unsigned int i=0; i < seats.size(); i++)
	{
		if (seats[i].in_round)
			count++;
	}
	
	return count;
}

// all (or except one) players are allin
bool Table::isAllin()
{
	unsigned int count = 0;
	
	for (unsigned int i=0; i < seats.size(); i++)
	{
		if (seats[i].in_round)
		{
			Player *p = seats[i].player;
			
			if ((int)p->getStake() == 0)
				count++;
		}
	}
	
	return (count >= seats.size() - 1);
}

bool Table::isPlayerInvolvedInPot(Pot *pot, Player *p)
{
	for (unsigned int i=0; i < pot->players.size(); i++)
	{
		if (pot->players[i] == p)
			return true;
	}
	
	return false;
}

unsigned int Table::getInvolvedInPotCount(Pot *pot, std::vector<HandStrength> &wl)
{
	unsigned int involved_count = 0;
	
	for (unsigned int i=0; i < pot->players.size(); i++)
	{
		int cid = pot->players[i]->getClientId();
		
		for (unsigned int j=0; j < wl.size(); j++)
		{
			if (wl[j].getId() == cid)
				involved_count++;
		}
	}
	
	return involved_count;
}

void Table::collectBets()
{
	do
	{
		// find smallest bet
		float smallest_bet = 0.0f;
		int smallest_bet_index = -1;
		
		for (unsigned int i=0; i < seats.size(); i++)
		{
			// skip folded and already handled players
			if (!seats[i].in_round || (int)seats[i].bet == 0)
				continue;
			
			// set an initial value
			if ((int)smallest_bet == 0)
				smallest_bet = seats[i].bet;
			else if (seats[i].bet < smallest_bet)
			{
				smallest_bet = seats[i].bet;
				smallest_bet_index = i;
			}
		}
		
#ifdef DEBUG
		dbg_print("collectBets", "smallest_bet: %d = %.2f", smallest_bet_index, smallest_bet);
#endif
		// there are no bets, do nothing
		if ((int)smallest_bet == 0)
			return;
		
		
		// last pot is current pot
		Pot *cur_pot = &(pots[pots.size() - 1]);
		
		// if current pot is final, create a new one
		if (cur_pot->final)
		{
			Pot pot;
			pot.amount = 0.0f;
			pot.final = false;
			pots.push_back(pot);
			
			cur_pot = &(pots[pots.size() - 1]);
		}
		
		
		// all player bets are the same
		if (smallest_bet_index == -1)
		{
			for (unsigned int i=0; i < seats.size(); i++)
			{
				// skip already handled players
				if ((int)seats[i].bet == 0)
					continue;
				
				// collect the bet into pot
				cur_pot->amount += seats[i].bet;
				seats[i].bet = 0.0f;
				
				// skip folded players
				if (!seats[i].in_round)
					continue;
				
				// mark pot as final if at least one player is allin
				Player *p = seats[i].player;
				if ((int)p->getStake() == 0)
					cur_pot->final = true;
				
				// set player involved in pot
				if (!isPlayerInvolvedInPot(cur_pot, p))
					cur_pot->players.push_back(p);
			}
			
			// get outa here
			break;
		}
		else  // side-pot needed
		{
			cur_pot->final = true;
			
			for (unsigned int i=0; i < seats.size(); i++)
			{
				// skip already handled players
				if ((int)seats[i].bet == 0)
					continue;
				
				cur_pot->amount += smallest_bet;
				seats[i].bet -= smallest_bet;
				
				// skip folded players
				if (!seats[i].in_round)
					continue;
				
				// set player involved in pot
				Player *p = seats[i].player;
				if (!isPlayerInvolvedInPot(cur_pot, p))
					cur_pot->players.push_back(p);
			}
		}
	} while (true);

#ifdef DEBUG
	for (unsigned int i=0; i < pots.size(); i++)
	{
		dbg_print("pot", "#%d: amount=%0.2f players=%d",
			i+1, pots[i].amount, (int)pots[i].players.size());
		
		for (unsigned int j=0; j < pots[i].players.size(); j++)
		{
			Player *p = pots[i].players[j];
			dbg_print("pot", "    player %d", p->getClientId());
		}
	}
	
	for (unsigned int i=0; i < seats.size(); i++)
	{
		dbg_print("seat-bets", "seat-%d: %.2f", i, seats[i].bet);
	}
#endif
}