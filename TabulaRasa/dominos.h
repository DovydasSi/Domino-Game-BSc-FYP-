#pragma once
#include <list>
#include <vector>
#include <iostream>
#include <string>
#include <algorithm>

using namespace std;

struct DominoPiece
{
	int head;
	int tail;

	DominoPiece(int h, int t) : head{ h }, tail{ t }{}
	DominoPiece(const DominoPiece& pc) : head{ pc.head }, tail{ pc.tail } {};
	DominoPiece(DominoPiece&& pc) = default;
	DominoPiece() : head{ -1 }, tail{ -1 } {}

	DominoPiece& operator=(const DominoPiece& rhs) = default;
	bool operator<(const DominoPiece& rhs) const
	{
		return head < rhs.head || (head == rhs.head && tail < rhs.tail);
	}

	bool operator==(const DominoPiece& rhs) const
	{
		return (head == rhs.head && tail == rhs.tail) || (head == rhs.tail && tail == rhs.head);
	}

	friend ostream& operator<< (ostream& os, const DominoPiece& pc);



	DominoPiece flip() const
	{
		return { tail, head };
	}

	string toString()
	{
		return to_string(head) + ":" + to_string(tail);
	}

	bool tile_from_string(string s)
	{
		size_t col = s.find(":");
		string h = s.substr(0, col);
		string t = s.substr(col + 1, s.length());

		head = stoi(h);
		tail = stoi(t);

		return true;
	}
};

struct PlayerMoveInfo
{
	size_t m_player_id;
	DominoPiece m_piece;
	bool m_in_front;

	PlayerMoveInfo(size_t id, DominoPiece pc, bool front) : m_player_id(id), m_piece(pc), m_in_front(front)
	{}
};

struct DominoBoard
{
	list<DominoPiece> board; // 
	vector<PlayerMoveInfo> board_history;

	// add an array of chronological actions


	void add_piece(const DominoPiece pc, bool front, size_t p_id)
	{
		if (board.empty())
		{
			board.emplace_front(pc);
		}
		else if (front)
		{
			if (board.front().head == pc.tail)
			{
				board.emplace_front(pc);
				board_history.push_back(PlayerMoveInfo(p_id, pc, front));
			}
			else if (board.front().head == pc.head)
			{
				board.emplace_front(pc.flip()); // flip this
				board_history.push_back(PlayerMoveInfo(p_id, pc.flip(), front));
			}
		}
		else
		{
			if (board.back().tail == pc.head)
			{
				board.emplace_back(pc);
				board_history.push_back(PlayerMoveInfo(p_id, pc, front));
			}
			else if (board.back().tail == pc.tail)
			{
				board.emplace_back(pc.flip()); // flip this
				board_history.push_back(PlayerMoveInfo(p_id, pc.flip(), front));
			}
		}
	}

	bool check_back(DominoPiece pc)
	{
		return board.back().tail == pc.head || board.back().tail == pc.tail;
	}

	bool check_front(DominoPiece pc)
	{
		return board.front().head == pc.head || board.front().head == pc.tail;
	}

	size_t repeated_tiles() const
	{
		vector<DominoPiece> sorted_board;
		for (const DominoPiece & pc : board)
		{
			if (pc.head > pc.tail)
			{
				sorted_board.emplace_back(pc.flip());
			}
			else
			{
				sorted_board.emplace_back(pc);
			}
		}

		sort(sorted_board.begin(), sorted_board.end(), [](auto&& x, auto&& y) { return x < y; });

		size_t total = 0;
		auto beg = begin(sorted_board) + 1;
		for (; beg != end(sorted_board); ++beg) {
			if (*beg == *(beg - 1)) {
				total++;
			}
		}

		return total;
	}

	friend ostream& operator<< (ostream& os, const DominoBoard& pc);
};

void generate_all_tiles(std::vector<DominoPiece>& deck);

struct DominoInconsistency
{
	int seed;
	int player_won;
};
