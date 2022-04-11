/******************************************************************************

							  Online C++ Compiler.
			   Code, Compile, Run and Debug C++ program online.
Write your code in this editor and press "Run" button to compile and execute it.

*******************************************************************************/

#include <iostream>
#include <iomanip>
#include <queue>
#include <thread>
#include <string>
#include <mutex>
#include <sstream>
#include <optional>
#include <random>
#include <chrono>

#include "json.h"

#define MAX_PROC_NUM         (4)
#define NUM_OF_CYCLES        (5)
#define DROP_ALTER_CHANCE    (2)
using namespace std;
using namespace nlohmann;

struct MessageQueue {
	size_t id, counter = 0;
	mutex m_queue_mutex, snapshot_mx;
	queue<std::string> ordinary_message_queue;
	queue<std::string> snapshot_message_queue;
	array<size_t, MAX_PROC_NUM> sent_counts;
	array<size_t, MAX_PROC_NUM> received_counts;
	array<size_t, MAX_PROC_NUM> inconsistency_counts;

	MessageQueue(size_t i) : id{ i }, sent_counts(), received_counts(), inconsistency_counts() {}

	void stepping_counter() {
		counter++;
	}

	bool hasMessage() {
		bool result = false;
		m_queue_mutex.lock();
		result = !ordinary_message_queue.empty();
		m_queue_mutex.unlock();
		return result;

	}

	bool hasSnapshotMessage() {
		bool result = false;
		snapshot_mx.lock();
		result = !snapshot_message_queue.empty();
		snapshot_mx.unlock();
		return result;

	}
};

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
	V[to]->m_queue_mutex.lock();
	if (!random_chance(DROP_ALTER_CHANCE)) // checking for drop
	{
		V[to]->ordinary_message_queue.push(message.dump());
	}
	else
	{
		//cout << "dropped \n";
	}
	V[to]->m_queue_mutex.unlock();
}

void recv(size_t self, nlohmann::json& out) {
	auto ref = V[self];
	ref->m_queue_mutex.lock();
	if (!ref->ordinary_message_queue.empty()) {
		std::string tmp = ref->ordinary_message_queue.front();
		out = json::parse(tmp);

		if (random_chance(DROP_ALTER_CHANCE)) // checking for altering
			alter_message(out);

		ref->ordinary_message_queue.pop();
	}
	ref->m_queue_mutex.unlock();
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
	return msg["txt_msg"] == "Hello World!";
}

template <
	class result_t = std::chrono::milliseconds,
	class clock_t = std::chrono::steady_clock,
	class duration_t = std::chrono::milliseconds >
	auto since(std::chrono::time_point < clock_t, duration_t >
		const& start) {
	return std::chrono::duration_cast <result_t> (clock_t::now() - start);
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
			std::cout << msg.dump() + "\n"; //+ " receiver cycle: " + to_string(epoch) + " receiver time: " + to_string(q->counter) + "\n";
			//cout << "Received " + to_string(from) << endl;

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

int main()
{
	V.emplace_back(new MessageQueue(0));
	V.emplace_back(new MessageQueue(1));
	V.emplace_back(new MessageQueue(2));
	V.emplace_back(new MessageQueue(3));

	thread first(send_receive, V[0]);
	thread second(send_receive, V[1]);
	thread third(send_receive, V[2]);
	thread fourth(send_receive, V[3]);

	first.join();
	second.join();
	third.join();
	fourth.join();

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

		Send END_TURN message
		increment the stage counter to next player's ID



	Randomly send out a snapshot after a specific amount of time



Resolve messages by message type:
	
	END_TURN:
		state = (state + 1) % no_of_players

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
	

*/