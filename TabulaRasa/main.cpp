
#include <iostream>
#include <iomanip>
#include <thread>
#include <sstream>
#include <optional>
#include <random>
#include <chrono>
#include <fstream>
#include "json.h"
#include "messageQueue.h"
#include "dominos.h"
#include "equivalence_Classes.h"
#include "Inconsistency.h"

using namespace std;
using namespace nlohmann;

std::vector< MessageQueue* > V;
std::vector< pair<size_t, double> > I;
size_t total_turns = 0;

std::mutex barrier;
condition_variable cond_barrier;
size_t barrier_count = 0;

bool finished_game = false;

std::random_device rd;  //Will be used to obtain a seed for the random number engine
std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
std::uniform_real_distribution<> distrib(0, 1);

bool random_chance( size_t chance)
{
	//return false;
	return distrib(gen) <= (1.0 / (double) chance);
}

void alter_message(json & msg)
{
	msg["txt_msg"] = "?!?!?!?!?";
}

void send(size_t to, const nlohmann::json& message) {

	bool sent = false;

	unique_lock<mutex> uni_loc(V[to]->m_queue_mutex);

#if DROP_MESSAGE
	if (sent = !random_chance(DROP_ALTER_CHANCE)) // checking for drop
#endif
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

		MESSAGE_TYPE type = out["type"];

#if ALTER_SEED
		if (type == SEED && random_chance(DROP_ALTER_CHANCE)) // checking for altering
		{
			std::uniform_int_distribution<> dist(0, 10000);
			out["seed"] = dist(gen);
		}
#endif
#if ALTER_HAS_WON
		if (type < DRAW && random_chance(DROP_ALTER_CHANCE)) // checking for altering
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

double calc_local_incons(const DominoInconsistency & di_1, const DominoInconsistency & di_2)
{
	double result = 0;

	double seed_inc = di_1.seed == di_2.seed ? 0.f : 1.f;
	double win_inc = di_1.player_won == di_2.player_won ? 0.f : 1.f;

	result = (seed_inc + win_inc);// / 2.f;

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

	//Send or Receive the seed
	if (q->id == 0)
	{
		std::uniform_int_distribution<> dist(0, 10000);
		seed = dist(gen);

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

		cout << " Player0 sent the seed: " + to_string(seed) + "\n";
		
		q->dinc.player_won = winner;
		q->dinc.seed = seed;

		unique_lock<mutex> uni_lock(barrier);
		barrier_count++;

		cond_barrier.wait(uni_lock);

		uni_lock.unlock();
		cout << endl;

#if REPAIR_SEED
		// Send out safe message confirming the seed
		for (int i = 0; i < MAX_PROC_NUM; i++)
		{
			json msg;
			msg["sender"] = q->id;
			msg["receiver"] = i;
			msg["type"] = SEED;
			msg["seed"] = seed;

			ssend(i, msg);
		}
#endif
		// We would usually do the equivalence class but since we are the host
		// we know what the true seed is
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

					q->dinc.seed = seed;

					cout << " Player" + to_string(q->id) + " received seed: " + to_string(seed) + "\n";
				}
				q->received_counts[from]++;

				break;
			}
		}

		unique_lock<mutex> uni_lock(barrier);
		barrier_count++;

		if (barrier_count >= MAX_PROC_NUM)
		{
			cond_barrier.notify_all();
			barrier_count = 0;
		}
		else
		{
			cond_barrier.wait(uni_lock);
		}

		uni_lock.unlock();
#if REPAIR_SEED
		while (true)
		{
			if (q->hasSnapshotMessage())
			{
				json msg;
				srecv(q->id, msg);
				int from = msg["sender"];
				if (msg["type"] == SEED)
				{
					seed = msg["seed"];
					q->dinc.seed = seed;
				}

				break;
			}
		}
#endif // REPAIR_SEED
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

#if NEW_INITIAL_TILE
	if (q->id == 0)
	{
		DominoPiece pc = *deck.begin();

		for (int i = 0; i < MAX_PROC_NUM; i++)
		{
			if (q->id == i)
			{
				continue;
			}

			json msg;
			msg["sender"] = 0;
			msg["type"] = INITIAL_TILE;
			msg["time"] = q->counter;
			msg["tile"] = pc.toString();
			msg["receiver"] = i;

			send(i, msg);
		}

		//cout << " Player0 sent out the initial tile: " + pc.toString() + "\n\n";
		q->board.add_piece(pc, false, 0);
		deck.erase(deck.begin());
	}
	
	if(q->id != 0)
	{
		auto wait = std::chrono::steady_clock::now();

		while (true)
		{
			if (q->hasMessage())
			{
				json msg;
				recv(q->id, msg);
				int from = msg["sender"];
				if (msg["type"] == INITIAL_TILE)
				{
					DominoPiece pc;
					pc.tile_from_string(msg["tile"]);

					//cout << " Player" + to_string(q->id) + " received initial tile: " + pc.toString() + "\n";

					q->board.add_piece(pc, false, 0);
					deck.erase(deck.begin());
				}
				q->received_counts[from]++;
			}

			break;
		}
	}
#else
	q->board.add_piece(deck.front(), false, 0);
	deck.erase(deck.begin());
#endif // NEW_INITIAL_TILE

	int stage = 0;

	array<int, MAX_PROC_NUM> alters;
	alters.fill(0);

	size_t skip_counter = 0;
	bool snap_started = false;
	int coordinator = -1;

	auto start = std::chrono::steady_clock::now();
	auto idle = std::chrono::steady_clock::now();

	while (since(start).count() < 10000)
	{
		q->stepping_counter();

		json msg;

		// Do your turn
		if (stage == q->id)
		{
			idle = std::chrono::steady_clock::now();

			//each turn will end with a message so start filling it now
			msg["sender"] = q->id;

			q->stepping_counter();

			total_turns++;

			//check for cards in hand against the board
			bool can_place = false;

			for (size_t i = 0; i < hand.size(); i++)
			{
				DominoPiece pc = hand[i];

				if (can_place = q->board.check_back(pc))
				{
					q->board.add_piece(pc, false, q->id);
					msg["type"] = TILE_PLACED_BACK;
					msg["tile"] = pc.toString();

					auto iter = hand.begin() + i;
					hand.erase(iter);

					cout << " P" + to_string(q->id) + " placed " + pc.toString() + " in the back \n";

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

					cout << " P" + to_string(q->id) + " placed " + pc.toString() + " in the front \n";

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

					cout << " P" + to_string(q->id) + " drew \n";
					skip_counter = 0;
				}
				else
				{
					cout << " P" + to_string(q->id) + " skipped turn \n";
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

					q->sent_counts[p]++;

					string t = can_place ? msg["tile"].dump() : "drew";
				}
			}

			stage = (stage + 1) % MAX_PROC_NUM;

			cout << " " << q->board << endl << endl;

			if (hand.empty() || skip_counter >= MAX_PROC_NUM)
			{

				if (hand.empty())
				{
					winner = q->id;
					q->dinc.player_won = winner;

					cout << " This is Player" + to_string(q->id) + ", I have won \n";

					for (int p = 0; p < MAX_PROC_NUM; p++)
					{
						json msg;
						msg["sender"] = q->id;
						msg["receiver"] = p;
						msg["type"] = WINNER_CONFIRM;

						ssend(p, msg);
					}
				}

				unique_lock<mutex> uni_lock(barrier);
				barrier_count = 0;
				finished_game = true;
				cond_barrier.notify_all();

				uni_lock.unlock();

				break;
			}
			
			if (!finished_game)
			{
				unique_lock<mutex> uni_lock(barrier);
				barrier_count++;

				cond_barrier.wait(uni_lock);

				uni_lock.unlock();
			}
		}
		// Receive message
		else if (q->hasMessage())
		{
			recv(q->id, msg);
			int from = msg["sender"];

			idle = std::chrono::steady_clock::now();

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
#if REPAIR_BOARD_HISTORY
					// Compare tile with your hand
					for (const DominoPiece& hand_piece : hand)
					{
						if (pc == hand_piece)
						{
							// Start snapshot if it's the same
							json msg;
							msg["type"] = INCONSISTENCY_CHECK;
							msg["sender"] = q->id;
							msg["time"] = q->counter;

							for (size_t i = 0; i < MAX_PROC_NUM; i++)
							{
								if (q->id == i)
								{
									continue;
								}

								ssend(i, msg);

								snap_started = true;
								coordinator = q->id;
							}
						}
					}
#endif

					q->board.add_piece(pc, type == TILE_PLACED_FRONT, from);

					skip_counter = 0;

					string t = msg["tile"].dump();

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

			unique_lock<mutex> uni_lock(barrier);
			if (!finished_game)
			{
				barrier_count++;
				if (barrier_count == MAX_PROC_NUM)
				{
					barrier_count = 0;
					I.emplace_back(total_turns, global_inconsistency(V));
					cond_barrier.notify_all();
				}
				else
				{
					cond_barrier.wait(uni_lock);
				}
			}

			uni_lock.unlock();

			q->stepping_counter();
			stage = (stage + 1) % MAX_PROC_NUM;

			if (msg["has_won"] || skip_counter >= MAX_PROC_NUM)
			{
				if (msg["has_won"])
				{
					//cout << to_string(msg["sender"]);

					winner = msg["sender"];
					q->dinc.player_won = winner;

					cout << " Player " + to_string(q->id) + " received that Player" + to_string(winner) + " has won \n";
				}
				
				break;
			}
		}
		else if (since(idle).count() > 2000)
		{
			if (q->hasSnapshotMessage())
			{
				json msg;
				srecv(q->id, msg);
				int from = msg["sender"];

#if REPAIR_HAS_WON // Check for missed winner
				if (msg["type"] == WINNER_CONFIRM)
				{
					winner = from;
					q->dinc.player_won = winner;

					cout << "Winner needed confirming \n";

					break;
				}
#endif
#if REPAIR_BOARD_HISTORY
				if (msg["type"] == INCONSISTENCY_CHECK)
				{
					// send seed and winner information to "from"

					json out_msg;
					out_msg["type"] = SEED;
					out_msg["sender"] = q->id;
					out_msg["receiver"] = from;
					out_msg["seed"] = seed;
					out_msg["winner"] = winner;

					ssend(from, out_msg);

					snap_started = true;
					coordinator = from;
				}
#endif
			}
			
		}
#if REPAIR_BOARD_HISTORY
		if (snap_started && coordinator== q->id)
		{
			size_t msg_count = 0;
			equivalence_class<size_t> eq;
			array<DominoInconsistency, MAX_PROC_NUM> inc_arr;

			while (msg_count < MAX_PROC_NUM)
			{
				if (q->hasSnapshotMessage())
				{
					json msg;
					srecv(q->id, msg);

					size_t from = msg["sender"];
					MESSAGE_TYPE type = msg["type"];

					if (type == SEED) // If we started the snapshot and got their info
					{
						// equivalence classes somewhere
						int s = msg["seed"];
						int w = msg["winner"];

						inc_arr[from].player_won = w;
						inc_arr[from].seed = s;
						msg_count++;
					}
				}
			}

			for (int i = 0; i < MAX_PROC_NUM; i++)
			{
				for (int j = i; j < MAX_PROC_NUM; j++)
				{
					if (inc_arr[i].seed == inc_arr[j].seed && inc_arr[i].player_won == inc_arr[j].player_won)
					{
						eq.insert(i, j);
					}
				}
			}


			size_t max_size = 0;
			size_t which = 0;

			auto eq_pair = eq.calculateEquivalenceClass();

			for (const auto & cp : eq_pair) 
			{
				if (max_size < cp.second.size())
				{
					max_size = cp.second.size();
					which = cp.first;
				}
			}

			auto & correct = eq_pair[which];
			size_t corr_seed = inc_arr[*correct.begin()].seed;
			size_t corr_winner = inc_arr[*correct.begin()].player_won;



			// scan the board for the first turn of the player that isn't in the "correct"
			// assume
			// cut off board history from there
			// refill your deck and reshuffle based on the seed from a "correct" player

		}
		else if (snap_started)
		{
			while (snap_started)
			{
				if (q->hasSnapshotMessage())
				{
					json msg;
					srecv(q->id, msg);

					size_t from = msg["sender"];
					MESSAGE_TYPE type = msg["type"];

					if (type == BOARD_HISTORY) // receiving the 
					{

					}
				}
			}
		}
#endif // REPAIR_BOARD_HISTORY
	}

}


void launch_program()
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

	for (auto& th : T)
	{
		delete th;
	}
	
	cout << endl << endl << " Boards: " << endl;
	for (int p = 0; p < V.size(); p++)
	{
		cout << " P" << p << ": " << V[p]->board << endl;
	}

	cout << endl << endl << " Hand Histories: " << endl;
	for (int p = 0; p < V.size(); p++)
	{
		cout << " P" << p << ": ";
		for (const DominoPiece& pc : V[p]->hand_history)
		{
			cout << "[" << pc << "] ";
		}
		cout << endl;
	}

	cout << endl << endl;
	
	cout << setw(3) << " ";
	for (int p = 0; p < MAX_PROC_NUM; p++)
	{
		cout << right << setw(6) << " P" + to_string(p) << setw(2) << " |";
	}
	cout << endl << endl;

	// Print the objects
	for (int p = 0; p < MAX_PROC_NUM; p++)
	{
		MessageQueue* q = V[p];
		cout << left << setw(35) << " No. of messages sent by P" + to_string(p) + ": ";
		for (size_t j : q->sent_counts)
		{
			cout << right << setw(6) << j << " |";
		}
		cout << endl;
		cout << left << setw(35) << " No. of messages received by P" + to_string(p) + ": ";
		for (size_t j : q->received_counts)
		{
			cout << right << setw(6) << j << " |";
		}
		cout << endl;
		
		//cout << left << setw(57) <<  " No. of inconsistencies found by V[" + to_string(p) + "] from each player: ";
		//for (size_t j : q->inconsistency_counts)
		//{
		//	cout << right << setw(4) << j << ", ";
		//}
		//cout << endl;
		
		cout << endl;
	}

	
	vector<DominoInconsistency> domvec;

	for (int p = 0; p < MAX_PROC_NUM; p++)
	{
		domvec.emplace_back(V[p]->dinc);
	}

	float global = global_inconsistency(V);
	float total = total_inconsistency(calc_local_incons, domvec);
	I.emplace_back(total_turns++, global);

	cout << endl;
	cout << " Total Inconsistency:  " << total << endl;
	cout << " Global Inconsistency: " << global << endl;

	cout << endl;
	cout << " Global Inconsistency throughout the game:" << endl;
	for (const auto& inc : I)
	{
		cout << " Turn " << inc.first << ": " << inc.second << endl;
	}

	for (auto& ref : V)
		delete ref;
}


int main()
{
	std::ofstream myfile;
	myfile.open("example.csv");
	
	std::vector<DominoPiece> deck;
	generate_all_tiles(deck);

	cout << endl << endl;

	for (int i = 0; i < NUM_OF_CYCLES; i++)
	{
		//resetting the variables
		I.clear();
		V.clear();
		barrier_count = 0;
		total_turns = 0;
		cond_barrier.notify_all();
		finished_game = false;

		launch_program();

		//load I into csv
		for (const auto& pair : I)
		{
			myfile << pair.second << ", ";
		}

		myfile << endl;
		//cout << i << endl;
	}


	myfile.close();

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

- Future work: without repairing the seed, if a player detects a tile placed by another that is in their hand, 
start a consensus on the seed and send back the correct board history


*/