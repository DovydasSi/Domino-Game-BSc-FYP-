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

bool random_chance( size_t chance)
{
	std::random_device rd;  //Will be used to obtain a seed for the random number engine
	std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
	std::uniform_int_distribution<> distrib(1, 57);
	return ( distrib(gen) % chance) == 0;
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
	// DO SOMETHING

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

void receiver(MessageQueue* q, array<size_t, MAX_PROC_NUM>& inc)
{
	array<int, MAX_PROC_NUM> alters;
	alters.fill(0);

	auto start = std::chrono::steady_clock::now();
	size_t epoch = 0;
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

		epoch++;
	}
}

int main()
{
	vector<array<size_t, MAX_PROC_NUM>> inconsistencies;
	inconsistencies.emplace_back(array<size_t, MAX_PROC_NUM>());
	inconsistencies.emplace_back(array<size_t, MAX_PROC_NUM>());
	inconsistencies[0].fill(0);
	inconsistencies[1].fill(0);

	V.emplace_back(new MessageQueue(0));
	V.emplace_back(new MessageQueue(1));
	V.emplace_back(new MessageQueue(2));
	V.emplace_back(new MessageQueue(3));

	thread first(sender, V[0]);
	thread second(receiver, V[1], ref(inconsistencies[0]));
	thread third(sender, V[2]);
	thread fourth(receiver, V[3], ref(inconsistencies[1]));

	//thread first(oversimplify, V[0]);
	//thread second(oversimplify, V[1]);
	//thread third(oversimplify,V[2]);


	first.join();
	second.join();
	third.join();
	fourth.join();


	/*for (int i = 0; i < inconsistencies.size(); i++)
	{
		array<size_t, MAX_PROC_NUM> inc = inconsistencies[i];
		cout << "Inconsistencies found by receiver" << i+1 <<" by ID: ";
		for (size_t j : inc)
		{
			cout << j << ", ";
		}
		cout << endl << endl;
	}*/

	cout << left;
	cout << endl;

	for (int p = 0; p < MAX_PROC_NUM; p++)
	{
		MessageQueue* q = V[p];
		cout << setw(57) << " No. of messages sent by V[" + to_string(p) + "] to each player: ";
		for (size_t j : q->sent_counts)
		{
			cout << setw(3) << j << ", ";
		}
		cout << endl;
		cout << setw(57) << " No. of messages received by V[" + to_string(p) + "] from each player: ";
		for (size_t j : q->received_counts)
		{
			cout << setw(3) << j << ", ";
		}
		cout << endl;
		cout << setw(57) <<  " No. of inconsistencies found by V[" + to_string(p) + "] from each player: ";
		for (size_t j : q->inconsistency_counts)
		{
			cout << setw(3) << j << ", ";
		}
		cout << endl << endl;
	}

	for (auto& ref : V)
		delete ref;

	cout << "\n\n Hello World";

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