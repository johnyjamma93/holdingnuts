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


#include <cstdio>
#include <string>
#include <cstring>   // FIXME: try to remove this one
#include <algorithm>

#include "Config.h"
#include "Debug.h"
#include "GameController.hpp"
#include "GameLogic.hpp"
#include "Card.hpp"

#include "game.hpp"


using namespace std;

// temporary buffer for chat/snap data
char msg[1024];


GameController::GameController()
{
	game_id = -1;
	
	started = false;
	max_players = MAX_PLAYERS;
	
	type = SNG;
}

bool GameController::addPlayer(int client_id)
{
	if (started || players.size() == max_players)
		return false;
	
	Player p;
	p.client_id = client_id;
	p.stake = 1500.0f;
	p.next_action.valid = false;
	
	players.push_back(p);
	
	return true;
}

bool GameController::removePlayer(int client_id)
{
	if (started)
		return false;
		
	for (vector<Player>::iterator e = players.begin(); e != players.end(); e++)
	{
		if (e->client_id == client_id)
		{
			players.erase(e);
			return true;
		}
	}
	
	return false;
}

Player* GameController::findPlayer(int cid)
{
	for (unsigned int i=0; i < players.size(); i++)
	{
		if (players[i].client_id == cid)
			return &(players[i]);
	}
	
	return NULL;
}

bool GameController::setPlayerMax(unsigned int max)
{
	if (max < 2 || max > MAX_PLAYERS)
		return false;
	
	max_players = max;
	return true;
}

void GameController::chat(int tid, const char* msg)
{
	for (vector<Player>::iterator e = players.begin(); e != players.end(); e++)
		client_chat(game_id, tid, e->client_id, msg);
}

void GameController::chat(int cid, int tid, const char* msg)
{
	client_chat(game_id, tid, cid, msg);
}

void GameController::snap(int tid, int sid, const char* msg)
{
	for (vector<Player>::iterator e = players.begin(); e != players.end(); e++)
		client_snapshot(game_id, tid, e->client_id, sid, msg);
}

void GameController::snap(int cid, int tid, int sid, const char* msg)
{
	client_snapshot(game_id, tid, cid, sid, msg);
}

bool GameController::setPlayerAction(int cid, Player::PlayerAction action, float amount)
{
	Player *p = findPlayer(cid);
	
	if (!p)
		return false;
	
	// reset a previously set action
	if (action == Player::ResetAction)
	{
		p->next_action.valid = false;
		return true;
	}
	
	p->next_action.valid = true;
	p->next_action.action = action;
	p->next_action.amount = amount;
	
	return true;
}

bool GameController::createWinlist(Table *t, vector< vector<HandStrength> > &winlist)
{
	vector<HandStrength> wl;
	
	unsigned int showdown_player = t->last_bet_player;
	for (unsigned int i=0; i < t->countActivePlayers(); i++)
	{
		Player *p = t->seats[showdown_player].player;
		
		HandStrength strength;
		GameLogic::getStrength(&(p->holecards), &(t->communitycards), &strength);
		strength.setId(p->client_id);
		
		wl.push_back(strength);
		
		showdown_player = t->getNextActivePlayer(showdown_player);
	}
	
	return GameLogic::getWinList(wl, winlist);
}

void GameController::sendTableSnapshot(Table *t)
{
	// assemble community-cards string
	vector<Card> cards;
	string scards;
	
	t->communitycards.copyCards(&cards);
	
	for (unsigned int i=0; i < cards.size(); i++)
	{
		scards += cards[i].getName();
		
		if (i < cards.size() -1)
			scards += ':';
	}
	
	
	// assemble seats string
	string sseats;
	for (unsigned int i=0; i < t->seats.size(); i++)
	{
		Table::Seat *s = &(t->seats[i]);
		Player *p = s->player;
		
		char tmp[1024];
		
		snprintf(tmp, sizeof(tmp),
			"s%d:%d:%c:%.2f",
			s->seat_no, p->client_id, s->in_round ? '*' : '-', p->stake);
		
		sseats += tmp;
		
		if (i < t->seats.size() -1)
			sseats += ' ';
	}
	
	
	// assemble pots string
	string spots;
	for (unsigned int i=0; i < t->pots.size(); i++)
	{
		Table::Pot *pot = &(t->pots[i]);
		
		char tmp[1024];
		
		snprintf(tmp, sizeof(tmp),
			"p%d:%.2f",
			i, pot->amount);
		
		spots += tmp;
		
		if (i < t->pots.size() -1)
			spots += ' ';
	}
	
	
	snprintf(msg, sizeof(msg),
		"%d:%d "           // <state>:<betting-round>
		"%d:%d:%d:%d "     // <dealer>:<SB>:<BB>:<current>
		"cc:%s "           // <community-cards>
		"%s "              // seats
		"%s",              // pots
		t->state, t->betround,
		t->dealer, t->sb, t->bb, t->cur_player,
		scards.c_str(),
		sseats.c_str(),
		spots.c_str());
	
	snap(t->table_id, SnapTable, msg);
}

// FIXME: SB gets first (one) card; not very important because it doesn't really matter
void GameController::dealHole(Table *t)
{
	for (unsigned int i = t->cur_player, c=0; c < t->seats.size(); i = t->getNextPlayer(i), c++)
	{
		t->seats[i].in_round = true;
		Player *p = t->seats[i].player;
		
		HoleCards h;
		Card c1, c2;
		t->deck.pop(c1);
		t->deck.pop(c2);
		p->holecards.setCards(c1, c2);
		
		char card1[3], card2[3];
		strcpy(card1, c1.getName());
		strcpy(card2, c2.getName());
		snprintf(msg, sizeof(msg), "Your hole-cards: [%s %s]",
			card1, card2);
		
		chat(p->client_id, t->table_id, msg);
	}
}

void GameController::dealFlop(Table *t)
{
	Card f1, f2, f3;
	t->deck.pop(f1);
	t->deck.pop(f2);
	t->deck.pop(f3);
	t->communitycards.setFlop(f1, f2, f3);
	
	char card1[3], card2[3], card3[3];
	strcpy(card1, f1.getName());
	strcpy(card2, f2.getName());
	strcpy(card3, f3.getName());
	snprintf(msg, sizeof(msg), "The flop: [%s %s %s]",
		card1, card2, card3);
	
	chat(t->table_id, msg);
}

void GameController::dealTurn(Table *t)
{
	Card tc;
	t->deck.pop(tc);
	t->communitycards.setTurn(tc);
	
	char card[3];
	strcpy(card, tc.getName());
	snprintf(msg, sizeof(msg), "The turn: [%s]",
		card);
	
	chat(t->table_id, msg);
}

void GameController::dealRiver(Table *t)
{
	Card r;
	t->deck.pop(r);
	t->communitycards.setRiver(r);
	
	char card[3];
	strcpy(card, r.getName());
	snprintf(msg, sizeof(msg), "The river: [%s]",
		card);
	
	chat(t->table_id, msg);
}

void GameController::stateNewRound(Table *t)
{
	dbg_print("Table", "New Round");
	
	t->deck.fill();
	t->deck.shuffle();
	
	t->communitycards.clear();

#ifdef SERVER_TESTING
	chat(t->table_id, "---------------------------------------------");
	
	string ststr = "Stacks: ";
	for (unsigned int i=0; i < t->seats.size(); i++)
	{
		Player *p = t->seats[i].player;
		
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "[%d]=%0.2f ", p->client_id, p->stake);
		ststr += tmp;
	}
	chat(t->table_id, ststr.c_str());
#endif
	
	chat(t->table_id, "New round started, deck shuffled");
	
	// clear old pots and create initial main pot
	t->pots.clear();
	Table::Pot pot;
	pot.amount = 0.0f;
	pot.final = false;
	t->pots.push_back(pot);
	
	t->nomoreaction = false;
	t->state = Table::Blinds;
}

void GameController::stateBlinds(Table *t)
{
	// FIXME: handle non-SNG correctly (ask each player for blinds ...)
	
	t->bet_amount = (float)t->blind;
	
	bool headsup_rule = (t->seats.size() == 2);
	
	// determine who is SB and BB
	if (headsup_rule)   // heads-up rule: only 2 players remain, so swap blinds
	{
		t->bb = t->getNextPlayer(t->dealer);
		t->sb = t->getNextPlayer(t->bb);
	}
	else
	{
		t->sb = t->getNextPlayer(t->dealer);
		t->bb = t->getNextPlayer(t->sb);
	}
	
	Player *pDealer = t->seats[t->dealer].player;
	Player *pSmall = t->seats[t->sb].player;
	Player *pBig = t->seats[t->bb].player;
	
	
	// set the player's SB
	float amount = t->blind / 2;
	
	if (amount > pSmall->stake)
		amount = pSmall->stake;
	
	t->seats[t->sb].bet = amount;
	pSmall->stake -= amount;
	
	
	// set the player's BB
	amount = t->blind;
	
	if (amount > pBig->stake)
		amount = pBig->stake;
	
	t->seats[t->bb].bet = amount;
	pBig->stake -= amount;
	
	
	snprintf(msg, sizeof(msg), "[%d] is Dealer, [%d] is SB (%.2f), [%d] is BB (%.2f) %s",
		pDealer->client_id,
		pSmall->client_id, (float)t->blind / 2,
		pBig->client_id, (float)t->blind,
		(headsup_rule) ? "HEADS-UP" : "");
	chat(t->table_id, msg);
	
	// player under the gun
	t->cur_player = t->getNextPlayer(t->bb);
	t->last_bet_player = t->cur_player;
	
	// initialize the player's timeout
	timeout_start = time(NULL);
	
	// give out hole-cards
	dealHole(t);
	
	// tell current player
	Player *p = t->seats[t->cur_player].player;
	chat(p->client_id, t->table_id, "You're under the gun!");
	
	t->betround = Table::Preflop;
	sendTableSnapshot(t);
	
	t->state = Table::Betting;
}

void GameController::stateBetting(Table *t)
{
	Player *p = t->seats[t->cur_player].player;
	bool allowed_action = false;  // is action allowed?
	
	Player::PlayerAction action;
	float amount;
	
	if (t->nomoreaction)  // early showdown, no more action at table possible
	{
		action = Player::None;
		allowed_action = true;
	}
	else if ((int)p->stake == 0)  // player is allin and has no more options
	{
		action = Player::None;
		allowed_action = true;
	}
	else if (p->next_action.valid)  // has player set an action?
	{
		action = p->next_action.action;
		
		dbg_print("game", "player %d choose an action", p->client_id);
		
		if (action == Player::Fold)
			allowed_action = true;
		else if (action == Player::Check)
		{
			// allowed to check?
			if (t->seats[t->cur_player].bet < t->bet_amount)
				chat(p->client_id, t->table_id, "Err: You cannot check! Try call.");
			else
				allowed_action = true;
		}
		else if (action == Player::Call)
		{
			if ((int)t->bet_amount == 0 || (int)t->bet_amount == t->seats[t->cur_player].bet)
				chat(p->client_id, t->table_id, "Err: You cannot call, nothing was bet! Try check.");
			else
			{
				allowed_action = true;
				amount = t->bet_amount - t->seats[t->cur_player].bet;
			}
		}
		else if (action == Player::Bet)
		{
			if ((unsigned int)t->bet_amount > 0)
				chat(p->client_id, t->table_id, "Err: You cannot bet, there was already a bet! Try raise.");
			else if (p->next_action.amount <= (unsigned int)t->bet_amount || p->next_action.amount < t->blind)
				chat(p->client_id, t->table_id, "Err: You cannot bet this amount.");
			else
			{
				allowed_action = true;
				amount = p->next_action.amount - t->seats[t->cur_player].bet;
			}
		}
		else if (action == Player::Raise)
		{
			if ((unsigned int)t->bet_amount == 0)
				chat(p->client_id, t->table_id, "Err: You cannot raise, nothing was bet! Try bet.");
			else if (p->next_action.amount <= (unsigned int)t->bet_amount)
				chat(p->client_id, t->table_id, "Err: You cannot raise this amount.");
			else
			{
				allowed_action = true;
				amount = p->next_action.amount - t->seats[t->cur_player].bet;
			}
		}
		else if (action == Player::Allin)
		{
			allowed_action = true;
			amount = p->stake;
		}
		
		// reset player action
		p->next_action.valid = false;
	}
	else
	{
		// handle player timeout
		const int timeout = 60;   // FIXME: configurable
		if ((int)difftime(time(NULL), timeout_start) > timeout)
		{
			// auto-action: fold, or check if possible
			if (t->seats[t->cur_player].bet < t->bet_amount)
				action = Player::Fold;
			else
				action = Player::Check;
			
			allowed_action = true;
		}
	}
	
	
	// return here if no or invalid action
	if (!allowed_action)
		return;
	
	
	// perform action
	if (action == Player::None)
	{
		// do nothing
	}
	else if (action == Player::Fold)
	{
		t->seats[t->cur_player].in_round = false;
		
		snprintf(msg, sizeof(msg), "Player %d folded.", p->client_id);
		chat(t->table_id, msg);
	}
	else if (action == Player::Check)
	{
		snprintf(msg, sizeof(msg), "Player %d checked.", p->client_id);
		chat(t->table_id, msg);
	}
	else
	{
		// player can't bet/raise more than this stake
		if (amount > p->stake)
			amount = p->stake;
		
		// move chips from player's stake to seat-bet
		t->seats[t->cur_player].bet += amount;
		p->stake -= amount;
		
		if (action == Player::Bet || action == Player::Raise || (action == Player::Allin && amount > (unsigned int)t->bet_amount))
		{
			// only re-open betting round if amount greater than table-bet
			if (amount > t->bet_amount /* && amount > bet_minimum*/ )
			{
				t->last_bet_player = t->cur_player;
				t->bet_amount = t->seats[t->cur_player].bet;
			}
			
			snprintf(msg, sizeof(msg), "Player %d bet/raised/allin $%.2f.", p->client_id, amount);
		}
		else
			snprintf(msg, sizeof(msg), "Player %d called $%.2f.", p->client_id, amount);
		
		
		chat(t->table_id, msg);
	}
	
	// all players except one folded, so end this hand
	if (t->countActivePlayers() == 1)
	{
		t->state = Table::AllFolded;
		sendTableSnapshot(t);
		return;
	}
	
	// is next the player who did the last bet/action? if yes, end this betting round
	if (t->getNextActivePlayer(t->cur_player) == (int)t->last_bet_player)
	{
		dbg_print("table", "betting round ended");
		
		// all (or all except one) players are allin
		if (t->isAllin())
		{
			// no further action at table possible
			t->nomoreaction = true;
			
			// FIXME: show up cards
		}
		
		// which betting round is next?
		switch ((int)t->betround)
		{
		case Table::Preflop:
			// deal flop
			dealFlop(t);
			
			t->betround = Table::Flop;
			break;
		
		case Table::Flop:
			// deal turn
			dealTurn(t);
			
			t->betround = Table::Turn;
			break;
		
		case Table::Turn:
			// deal river
			dealRiver(t);
			
			t->betround = Table::River;
			break;
		
		case Table::River:
			// end of hand, do showdown
			t->state = Table::Showdown;
			return;
		}
		
		// collect bets into pot
		t->collectBets();
		
		t->bet_amount = 0.0f;
		
		
		// set current player to SB (or next active behind SB)
		bool headsup_rule = (t->seats.size() == 2);
		if (headsup_rule)
			t->cur_player = t->getNextActivePlayer(t->getNextActivePlayer(t->dealer));
		else
			t->cur_player = t->getNextActivePlayer(t->dealer);
		
		// re-initialize the player's timeout
		timeout_start = time(NULL);
		
		// first action for next betting round is at this player
		t->last_bet_player = t->cur_player;
		
		sendTableSnapshot(t);
		
		Player *p = t->seats[t->cur_player].player;
		chat(p->client_id, t->table_id, "It's your turn!");
	}
	else
	{
		// find next player
		t->cur_player = t->getNextActivePlayer(t->cur_player);
		timeout_start = time(NULL);
		
		sendTableSnapshot(t);
		
		Player *p = t->seats[t->cur_player].player;
		chat(p->client_id, t->table_id, "It's your turn!");
	}
}

void GameController::stateAllFolded(Table *t)
{
	// collect bets into pot
	t->collectBets();
	
	// get last remaining player
	Player *p = t->seats[t->getNextActivePlayer(t->cur_player)].player;
	
	// FIXME: ask player if he wants to show cards
	
	snprintf(msg, sizeof(msg), "Player %d wins %.2f", p->client_id, t->pots[0].amount);
	chat(t->table_id, msg);
	
	p->stake += t->pots[0].amount;
	
	t->state = Table::EndRound;
	
	sendTableSnapshot(t);
}

void GameController::stateShowdown(Table *t)
{
	chat(t->table_id, "Showdown");
	
	// the player who did the last action is first to show
	unsigned int showdown_player = t->last_bet_player;
	
	// FIXME: ask for showdown if player has the option
	for (unsigned int i=0; i < t->countActivePlayers(); i++)
	{
		Player *p = t->seats[showdown_player].player;
		
		HandStrength strength;
		GameLogic::getStrength(&(p->holecards), &(t->communitycards), &strength);
		
		vector<Card> cards;
		string hsstr = "rank: ";
		
		cards.clear();
		strength.copyRankCards(&cards);
		for (vector<Card>::iterator e = cards.begin(); e != cards.end(); e++)
		{
			sprintf(msg, "%s ", e->getName());
			hsstr += msg;
		}
		
		hsstr += "kicker: ";
		cards.clear();
		strength.copyKickerCards(&cards);
		for (vector<Card>::iterator e = cards.begin(); e != cards.end(); e++)
		{
			sprintf(msg, "%s ", e->getName());
			hsstr += msg;
		}
		
		snprintf(msg, sizeof(msg), "Player [%d] has: %s (%s)",
			p->client_id,
			HandStrength::getRankingName(strength.getRanking()),
			hsstr.c_str());
		chat(t->table_id, msg);
		
		showdown_player = t->getNextActivePlayer(showdown_player);
	}
	
	
	// determine winners
	vector< vector<HandStrength> > winlist;
	createWinlist(t, winlist);
	
	for (unsigned int i=0; i < winlist.size(); i++)
	{
		vector<HandStrength> &tw = winlist[i];
		const unsigned int winner_count = tw.size();
		
		// for each pot
		for (unsigned int poti=0; poti < t->pots.size(); poti++)
		{
			Table::Pot *pot = &(t->pots[poti]);
			unsigned int involved_count = t->getInvolvedInPotCount(pot, tw);
			
			float cashout_amount = 0.0f;
			
			// for each winning-player
			for (unsigned int pi=0; pi < winner_count; pi++)
			{
				Player *p = findPlayer(tw[pi].getId());
				// skip pot if player not involved in it
				if (!t->isPlayerInvolvedInPot(pot, p))
					continue;
#ifdef DEBUG
				dbg_print("winlist", "wl #%d: player #%d: pot #%d: involved-count=%d",
					i+1, pi+1, poti+1, involved_count);
#endif
				float win_amount = pot->amount / involved_count;
				
				if ((int) win_amount > 0)
				{
					p->stake += win_amount;
					cashout_amount += win_amount;
					
					snprintf(msg, sizeof(msg),
						"Player [%d] wins pot #%d with %.2f",
						p->client_id, poti+1, win_amount);
					chat(t->table_id, msg);
				}
			}
			
			pot->amount -= cashout_amount;
		}
	}
	
	t->pots.clear();
	
	
	t->state = Table::EndRound;
	
	sendTableSnapshot(t);
}

void GameController::stateEndRound(Table *t)
{
	// remove broken players from seats
	bool removed_player;
	do
	{
		removed_player = false;
		
		for (vector<Table::Seat>::iterator e = t->seats.begin(); e != t->seats.end(); e++)
		{
			Player *p = e->player;
			
			if ((int)p->stake == 0)
			{
				dbg_print("stateEndRound", "removed player %d", p->client_id);
				chat(p->client_id, t->table_id, "You broke!");
				
				t->seats.erase(e);
				
				removed_player = true;
				break;
			}
		}
	} while (removed_player);
	
	t->dealer = t->getNextPlayer(t->dealer);
	t->state = Table::NewRound;
}

int GameController::handleTable(Table *t)
{
	if (t->state == Table::ElectDealer)
	{
		// TODO: implement me
	}
	else if (t->state == Table::NewRound)
		stateNewRound(t);
	else if (t->state == Table::Blinds)
		stateBlinds(t);
	else if (t->state == Table::Betting)
		stateBetting(t);
	else if (t->state == Table::AllFolded)
		stateAllFolded(t);
	else if (t->state == Table::Showdown)
		stateShowdown(t);
	else if (t->state == Table::EndRound)
	{
		stateEndRound(t);
		
		// only 1 player left? close table
		if (t->seats.size() == 1)
			return -1;
	}
	
	return 0;
}

void GameController::tick()
{
	if (!started)
	{
		// start a game
		if (getPlayerCount() == max_players)
		{
			dbg_print("game", "game %d has been started", game_id);
			
			started = true;
			
			// TODO: support more than 1 table
			const int tid = 0;
			Table table;
			table.setTableId(tid);
			for (unsigned int i=0; i < players.size(); i++)
			{
				Table::Seat seat;
				memset(&seat, 1, sizeof(Table::Seat));
				seat.seat_no = i;
				seat.player = &(players[i]);
				table.seats.push_back(seat);
			}
			table.dealer = 0;
			table.blind = 10;
			table.state = Table::NewRound;
			tables.push_back(table);
			
			snap(-1, SnapGameState, "start");
			
			// FIXME: tell client its table-no
		}
		else
			return;
	}
	
	// handle all tables
	for (unsigned int i=0; i < tables.size(); i++)
	{
		Table *t = &(tables[i]);
		
		// table closed?
		if (handleTable(t) < 0)
		{
			Player *p = t->seats[0].player;
			chat(p->client_id, t->table_id, "You won!");
			
			// FIXME: what to do here?
			started = false;
			players.clear();
			tables.clear();
			
			snap(-1, SnapGameState, "end");
		}
	}
}