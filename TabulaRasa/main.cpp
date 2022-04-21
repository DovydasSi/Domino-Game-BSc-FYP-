
#include <iostream>
#include <iomanip>
#include <thread>
#include <sstream>
#include <optional>
#include <random>
#include <chrono>
#include "json.h"
#include "messageQueue.h"
#include "dominos.h"
#include "equivalence_Classes.h"
#include "Inconsistency.h"

using namespace std;
using namespace nlohmann;

std::vector< MessageQueue* > V;

std::random_device rd;  //Will be used to obtain a seed for the random number engine
std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
std::uniform_real_distribution<> distrib(0, 1);

bool random_chance( size_t chance)
{
	return false;
	//return distrib(gen) <= (1.0 / (double) chance);
}

void alter_message(json & msg)
{
	msg["txt_msg"] = "?!?!?!?!?";
}

void send(size_t to, const nlohmann::json& message) {

	bool sent;

	unique_lock<mutex> uni_loc(V[to]->m_queue_mutex);

	if (sent = !random_chance(DROP_ALTER_CHANCE)) // checking for drop
	{
		V[to]->ordinary_message_queue.push(message.dump());
	}
	uni_loc.unlock();

	if (sent)
	{
		V[to]->m_queue_cond.notify_one();
	}
}

void recv(size_t self, nlohmann::json& out) {
	auto ref = V[self];

	unique_lock<mutex> uni_loc(ref->m_queue_mutex);
	if (!ref->ordinary_message_queue.empty()) {
		std::string tmp = ref->ordinary_message_queue.front();
		out = json::parse(tmp);

#if ALTER_SEED
		MESSAGE_TYPE type = out["type"];
		if (type == SEED && random_chance(DROP_ALTER_CHANCE)) // checking for altering
		{
			std::uniform_int_distribution<> dist(0, 10000);
			out["seed"] = dist(gen);
		}
#endif
#if ALTER_HAS_WON
		if (random_chance(DROP_ALTER_CHANCE)) // checking for altering
		{
			bool has_won = out["has_won"];

			if(has_won)
			out["has_won"] = !has_won;
		}
#endif

		ref->ordinary_message_queue.pop();
	}
	uni_loc.unlock();
}

void ssend(size_t to, const nlohmann::json& message) {
	V[to]->snapshot_mx.lock();
	V[to]->snapshot_message_queue.push(message.dump());
	V[to]->snapshot_mx.unlock();
}

void srecv(size_t self, nlohmann::json& out) {
	auto ref = V[self];
	ref->snapshot_mx.lock();
	if (!ref->snapshot_message_queue.empty()) {
		std::string tmp = ref->snapshot_message_queue.front();
		out = json::parse(tmp);
		ref->snapshot_message_queue.pop();
	}
	ref->snapshot_mx.unlock();
}

bool inspect_message(const json msg)
{
	return false; //msg["txt_msg"] == "Hello World!";
}

template <
	class result_t = std::chrono::milliseconds,
	class clock_t = std::chrono::steady_clock,
	class duration_t = std::chrono::milliseconds >
	auto since(std::chrono::time_point < clock_t, duration_t > const& start) 
{
	return std::chrono::duration_cast <result_t> (clock_t::now() - start);
}

void generate_all_tiles(std::vector<DominoPiece>& deck)
{
	deck.clear();

	for (int i = MIN_TILE_NUM; i <= MAX_TILE_NUM; i++)
	{
		for (int j = i; j <= MAX_TILE_NUM; j++)
		{
			deck.push_back(DominoPiece(i, j));
		}
	}
}

float calc_local_incons(const DominoInconsistency & di_1, const DominoInconsistency & di_2)
{
	float result = 0.f;

	float seed_inc = di_1.seed == di_2.seed ? 0.f : 1.f;
	float win_inc = di_1.player_won == di_2.player_won ? 0.f : 1.f;

	result = (seed_inc + win_inc) / 2.f;

	return result;
}

ostream& operator<< (ostream& os, const DominoBoard& board)
{
	for (DominoPiece pc : board.board)
	{
		os << "[" << pc << "] ";
	}

	return os;
}

ostream& operator<< (ostream& os, const DominoPiece& pc)
{
	os << "[" << pc.head << ":" << pc.tail << "]";
	return os;
}

void oversimplify(MessageQueue* q)
{
	int i = 0;
	while (i < NUM_OF_CYCLES) //1530
	{

		q->stepping_counter();

		cout << i << endl;

		while (q->hasMessage()) {
			json msg;
			recv(q->id, msg);
			std::cout << msg.dump() << std::endl;
		}


		// If I need to send a message to everyone
		for (size_t j = 0; j < MAX_PROC_NUM; j++) {
			if (j != q->id) {
				json msg;
				q->stepping_counter();
				msg["sender"] = q->id;
				msg["time"] = q->counter;
				msg["receiver"] = j;
				send(j, msg);
			}
		}

		i++;
	}
}

void receiver(MessageQueue* q)
{
	array<int, MAX_PROC_NUM> alters;
	alters.fill(0);

	auto start = std::chrono::steady_clock::now();
	while (since(start).count() < 6000)
	{
		q->stepping_counter();

		while (q->hasMessage()) {
			json msg;
			recv(q->id, msg);
			int from = msg["sender"];

			q->received_counts[from]++;

			if (!inspect_message(msg))
			{
				alters[from]++;
			}

			q->stepping_counter();

			q->inconsistency_counts[from] = msg["sent_count"] - (q->received_counts[from] - alters[from]);
		}
	}
}

void sender(MessageQueue* q)
{
	int i = 0;
	while (i < NUM_OF_CYCLES)
	{
		q->stepping_counter();

		for (int p = 0; p < MAX_PROC_NUM; p++)
		{
			if (p != q->id)
			{
				json msg;
				q->stepping_counter();
				q->sent_counts[p]++;
				//cout << "Sent. " + to_string(p) + "\n";
				msg["sender"] = q->id;
				msg["time"] = q->counter;
				msg["receiver"] = p;
				msg["sent_count"] = q->sent_counts[p];
				msg["txt_msg"] = "Hello World!";
				send(p, msg);
			}
		}

		i++;
	}
}

void send_receive(MessageQueue* q)
{
	array<int, MAX_PROC_NUM> alters;
	alters.fill(0);

	auto start = std::chrono::steady_clock::now();
	while (since(start).count() < 3000)
	{
		q->stepping_counter();

		// Receive messages in queue
		while (q->hasMessage()) {
			json msg;
			recv(q->id, msg);
			int from = msg["sender"];

			q->received_counts[from]++;

			if (!inspect_message(msg))
			{
				alters[from]++;
			}

			q->stepping_counter();

			q->inconsistency_counts[from] = msg["sent_count"] - (q->received_counts[from] - alters[from]);
		}

		// Randomly send messages to everyone
		if (!random_chance(2000))
		{
			for (int p = 0; p < MAX_PROC_NUM; p++)
			{
				if (p != q->id)
				{
					q->stepping_counter();
					q->sent_counts[p]++;

					json msg;
					msg["sender"] = q->id;
					msg["time"] = q->counter;
					msg["receiver"] = p;
					msg["sent_count"] = q->sent_counts[p];
					msg["txt_msg"] = "Hello World!";
					send(p, msg);
				}
			}
		}
	}
}

void start_game(MessageQueue* q)
{
	size_t seed = 100;
	int winner = -1;    // id of the winner, -1 if no one has won yet

	if (q->id == 0)
	{
		std::uniform_int_distribution<> dist(0, 10000);
		seed = dist(gen);
		seed = 100;

		cout << seed;

		json msg;
		msg["sender"] = q->id;

		for (int i = 0; i < MAX_PROC_NUM; i++)
		{
			if (q->id != i)
			{
				msg["receiver"] = i;
				msg["type"] = SEED;
				msg["seed"] = seed;
				send(i, msg);

				q->sent_counts[i]++;
			}
		}

		/*DominoInconsistency dinc[MAX_PROC_NUM];
		dinc[q->id].player_won = winner;
		dinc[q->id].seed = seed;
		size_t msg_count = 0;

		while (msg_count < MAX_PROC_NUM-1)
		{
			if (q->hasSnapshotMessage())
			{
				json msg;
				recv(q->id, msg);
				int from = msg["sender"];

				if (msg["type"] == INCONSISTENCY_CHECK)
				{
					dinc[from].seed = msg["seed"];
					dinc[from].player_won = msg["player_won"];
				}
				msg_count++;

				// comment out unless we decide to count special messages q->received_counts[from]++;
			}
		}

		equivalence_class<int> eq_class;

		for (int i = 0; i < MAX_PROC_NUM; i++)
		{
			for (int j = i + 1; j < MAX_PROC_NUM; j++)
			{
				float res = calc_local_incons(dinc[i], dinc[j]);

				if (res == 0)
				{
					// do the equivalence class

					eq_class.insert(i, j);
				}
			}
		}

		size_t max_size = 0;
		unordered_set<int> candidate_set;
		for (const auto & pair : eq_class.calculateEquivalenceClass())
		{
			if (pair.second.size() > max_size)
			{
				candidate_set = pair.second;
				max_size = pair.second.size();
			}
		}*/


	}
	else
	{
		// Wait for seed to be sent
		while (true)
		{
			if (q->hasMessage())
			{
				json msg;
				recv(q->id, msg);
				int from = msg["sender"];
				if (msg["type"] == SEED)
				{
					seed = msg["seed"];
				}
				q->received_counts[from]++;

				//cout << seed;

				//send snapshot to "from" or just 0
				/*json out_msg;
				out_msg["type"] = INCONSISTENCY_CHECK;
				out_msg["receiver"] = from;
				out_msg["sender"] = q->id;
				out_msg["seed"] = seed;
				out_msg["player_won"] = winner;

				ssend(from, out_msg);*/

				break;
			}
		}
	}

	vector<DominoPiece> deck;

	generate_all_tiles(deck);

	std::shuffle(deck.begin(), deck.end(), default_random_engine(seed));

	vector<DominoPiece> hand;

	// Draw tiles
	for (size_t i = q->id * TILES_IN_HAND; i < q->id * TILES_IN_HAND + TILES_IN_HAND; i++)
	{
		// Draw
		hand.emplace_back(deck[i]);
		q->hand_history.insert(deck[i]);
	}

	// Remove the cards from the deck for each player
	for (int i = 0; i < TILES_IN_HAND * MAX_PROC_NUM; i++)
	{
		deck.erase(deck.begin());
	}

	q->board.add_piece(deck.front(), false, 0);
	deck.erase(deck.begin());

	int stage = 0;

	array<int, MAX_PROC_NUM> alters;
	alters.fill(0);

	size_t skip_counter = 0;

	auto start = std::chrono::steady_clock::now();

	while (since(start).count() < 10000)
	{
		q->stepping_counter();

		json msg;

		// Do turn
		if (stage == q->id)
		{
			//each turn will end with a message so start filling it now
			msg["sender"] = q->id;

			q->stepping_counter();

			//check for cards in hand against the board
			bool can_place = false;

			for (size_t i = 0; i < hand.size(); i++)
			{
				DominoPiece pc = hand[i];

				// check if I'm trying to a card that's already on the borad, run consensus if is
				// cons 

				if (can_place = q->board.check_back(pc))
				{
					q->board.add_piece(pc, false, q->id);
					msg["type"] = TILE_PLACED_BACK;
					msg["tile"] = pc.toString();

					auto iter = hand.begin() + i;
					hand.erase(iter);

					skip_counter = 0;
					break;
				}
				else if (can_place = q->board.check_front(pc))
				{
					q->board.add_piece(pc, true, q->id);
					msg["type"] = TILE_PLACED_FRONT;
					msg["tile"] = pc.toString();

					auto iter = hand.begin() + i;
					hand.erase(iter);

					skip_counter = 0;
					break;
				}
			}
			if (!can_place)
			{
				// draw
				if (!deck.empty())
				{
					// check if I drew a card that's already on the borad, run consensus if is
					hand.emplace_back(deck.front());
					q->hand_history.insert(deck.front());
					deck.erase(deck.begin());
				}
				else
				{
					skip_counter++;
				}
				
				msg["type"] = DRAW;
			}

			msg["has_won"] = hand.empty();

			for (int p = 0; p < MAX_PROC_NUM; p++)
			{
				if (q->id != p)
				{
					q->stepping_counter();

					msg["receiver"] = p;
					msg["time"] = q->counter; // ask about the counter

					send(p, msg);
					cout << "Message sent to " + to_string(p) + " \n";

					q->sent_counts[p]++;

					string t = can_place ? msg["tile"].dump() : "drew";
					//cout << "message sent: " << t << endl;
				}
			}

			stage = (stage + 1) % MAX_PROC_NUM;

			if (hand.empty() || skip_counter >= MAX_PROC_NUM)
			{
				//cout << endl << "this is " + to_string(q->id) << " " << q->board << endl;

				if (hand.empty())
				{
					winner = q->id;
				}

				break;
			}
		}
		// Receive message
		else if (q->hasMessage())
		{
			recv(q->id, msg);
			int from = msg["sender"];


			q->received_counts[from]++;

			if (!inspect_message(msg))
			{
				alters[from]++;
			}

			MESSAGE_TYPE type = msg["type"];
			size_t msg_time = msg["time"];

			switch (type)
			{
				case TILE_PLACED_BACK:
				case TILE_PLACED_FRONT:
				{
					string tile = msg["tile"];
					DominoPiece pc;
					pc.tile_from_string(tile);
					q->board.add_piece(pc, type == TILE_PLACED_FRONT, from);

					skip_counter = 0;

					string t = msg["tile"].dump();
					cout << "this is: " << q->id << ", receiving from:" << from << ", message received: " << t << endl;

					break;
				}
				case DRAW:
				{
					if (!deck.empty())
					{
						deck.erase(deck.begin());
						skip_counter = 0;
					}
					else
					{
						skip_counter++;
					}
					break;
				}
				default: break;
			}

			//q->counter = q->counter < msg_time ? msg_time : q->counter; // talk about this as well

			q->stepping_counter();
			stage = (stage + 1) % MAX_PROC_NUM;

			if (msg["has_won"] || skip_counter >= MAX_PROC_NUM)
			{
				//cout << endl << "this is " + to_string(q->id) << " " << q->board << endl;
				if (msg["has_won"])
				{
					//cout << to_string(msg["sender"]);

					winner = msg["sender"];
				}
				
				break;
			}
		}
	}
}

int main()
{

	for (int i = 0; i < MAX_PROC_NUM; i++)
	{
		V.emplace_back(new MessageQueue(i));
	}

	vector<thread*> T;

	for (int i = 0; i < MAX_PROC_NUM; i++)
	{
		T.emplace_back(new thread(start_game, V[i]));
	}

	for (thread* th : T)
	{
		th->join();
	}

	cout << endl;

	// Print the objects
	for (int p = 0; p < MAX_PROC_NUM; p++)
	{
		MessageQueue* q = V[p];
		cout << left << setw(57) << " No. of messages sent by V[" + to_string(p) + "] to each player: ";
		for (size_t j : q->sent_counts)
		{
			cout << right << setw(4) << j << ", ";
		}
		cout << endl;
		cout << left << setw(57) << " No. of messages received by V[" + to_string(p) + "] from each player: ";
		for (size_t j : q->received_counts)
		{
			cout << right << setw(4) << j << ", ";
		}
		cout << endl;
		cout << left << setw(57) <<  " No. of inconsistencies found by V[" + to_string(p) + "] from each player: ";
		for (size_t j : q->inconsistency_counts)
		{
			cout << right << setw(4) << j << ", ";
		}
		cout << endl;
	}

	cout << "Total Inconsistency: " << global_inconsistency(V) << endl;

	for (auto& ref : V)
		delete ref;

	return 0;
}

/*
	have an array that stores a counter of how many messages a player sent to each other player
	have an array that stores a counter of how many messages a player received from each other player
	send regular messages from one thread to the other
	the message content is "how many messages the sender has now sent to the receiver"
	the receiver then checks how many total messages it has received from the sender and calculate the inconsistency (if any)

	start from one sender and one receiver

*/

/*

Main:

	Set the randomiser seed for the deck	// I really want to talk about this, how to synchronise the randomiser seed between players


Start of the thread:
	
	Initialise the board (probably a std::list !!, since you can emplace to the back and front)
	Initialise the deck array
	Initialise the hand vector
	Retrieve 4-6 random tiles (per player)
	Set state to 0
	start the player loop

Player loop:
	
	While (hasMessage()):
		resolve messages		// Different part of the model

	if it's your turn (state == id):
		
		check your hand against the board
		if you have a tile available:
			send a TILE_PLACED message to each player with tile's information and its position on the board
			place the tile on the board
			remove it from your hand
		if no tile is available:
			send a PLAYER_DRAW message
			draw the top tile from the pile


		increment the stage counter to next player's ID



	Randomly send out a snapshot after a specific amount of time



Resolve messages by message type:
	
	PLAYER_DRAW:
		pop the top tile off the deck

	TILE_PLACED:
		place the received tile to the specified end of the board

	TILE_CHECK (optional/not sure if necessary):
		Check if the received tile is in your hand 
		Send a possitive or negative response back

	TILE_CHECK_RESPONSE (same as above):
		wait for everyone's responses 
		if check passes - continue on
		if not - inconsistency detected, possibly launch a snapshot algorithm



	
Drawing tiles:




*/

/*
	Message structure:
	sender
	receiver
	time
	type - tile_placed/draw_from_deck/seed
	tile - e.g. "1:6"
	has_won - true/false
*/

/*
  Discarded idea 1: 
  measuring the inconsistency for each unique pair of the players by
  measuring th levenshtein distance between their boards (between 0 and 1) + checking if the players have the same winner (0 or 1) 
  https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C++ 3rd example, convert boards into arrays for that, add == to DominoPiece


  run consensus after seed, after game's over

  add alterations to seed and has_won, for the time being

  discarded idea 2:
  distance between hands 1 - (h1 intersection h2)/(h1 union h2), use sets for hands
   (1haswon + 1equalseed)/2 //+ hand distance)/3

   global incons. = number_of_repeated_tiles_on_board + number_of_repeated_tiles_in_hand 

   1. Implement the local inconsistency calculation, based on "who won" and "seed"
   2. Add board/play history
   3. Alter seed and "has_won" messages, make it so that we can turn each on/off
   4. Once the game is done, galculate the global incons.



   3. Use Equivalence class to see who is right in the consensus
   4. Generating the correct board history in the cons 
   5. 

*/

/*
- fix the deadlock
- check if the inconsistency metric is working
- commit the working version, 

- redo the changes of checking and confirming the seed 


*/