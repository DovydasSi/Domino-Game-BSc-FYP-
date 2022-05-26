#pragma once

#include <string>
#include <mutex>
#include <queue>
#include <array>
#include <condition_variable>
#include <set>

#include "constants.h"
#include "dominos.h"


using namespace std;

enum MESSAGE_TYPE
{
	TILE_PLACED_FRONT = 0,
	TILE_PLACED_BACK,
	DRAW,
	SEED,
	INCONSISTENCY_CHECK,
	WINNER_CONFIRM,
	BOARD_HISTORY,
	INITIAL_TILE
};

struct MessageQueue {
	size_t id, counter = 0;
	mutex m_queue_mutex, snapshot_mx;
	condition_variable m_queue_cond;
	queue<std::string> ordinary_message_queue;
	queue<std::string> snapshot_message_queue;
	array<size_t, MAX_PROC_NUM> sent_counts;
	array<size_t, MAX_PROC_NUM> received_counts;
	array<size_t, MAX_PROC_NUM> inconsistency_counts;

	set<DominoPiece> hand_history;
	DominoBoard board;
	DominoInconsistency dinc;

	MessageQueue(size_t i) : id{ i }, sent_counts(), received_counts(), inconsistency_counts() {}

	void stepping_counter() {
		counter++;
	}

	bool hasMessage(int time = 100, bool async = false) {
		bool result = false;

		unique_lock<mutex> uni_lock(m_queue_mutex);
		if (!async)
		{
			if (time > 0)
			{
				chrono::milliseconds t(time * 1ms);
				m_queue_cond.wait_for(uni_lock, t);
			}
			else
			{
				m_queue_cond.wait(uni_lock);
			}
		}
		//m_queue_mutex.lock();
		result = !ordinary_message_queue.empty();
		uni_lock.unlock();
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