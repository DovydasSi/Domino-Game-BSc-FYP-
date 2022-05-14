#include "Inconsistency.h"

// call this total message inc 
// calculates pairwise message inc (that's the lambda), pairwise function uses local information from each player
double total_inconsistency(std::function<double(const DominoInconsistency&, const DominoInconsistency&)> inconsistency,
	const vector<DominoInconsistency>& arr)
{
	double count = 0, average = 0;

	for (int i = 0; i < arr.size(); i++)
	{
		for (int j = 0; j < i; j++)
		{
			count++;
			average += inconsistency(arr[i], arr[j]);
		}
	}
	return average; /// count;
}

double global_inconsistency(const std::vector<MessageQueue*> & q)
{
	double total = 0;
	double board = 0;
	for (int i = 0; i < q.size(); i++)
	{
		const auto& bg_iter_1 = q[i]->hand_history.begin();
		const auto& en_iter_1 = q[i]->hand_history.end();
		for (int j = 0; j < i; j++)
		{
			std::vector <DominoPiece> vec;
			const auto& bg_iter_2 = q[j]->hand_history.begin();
			const auto& en_iter_2 = q[j]->hand_history.end();

			set_intersection(bg_iter_1, en_iter_1, bg_iter_2, en_iter_2, back_inserter(vec));
			total += vec.size() + board_distance(q[i]->board, q[j]->board);
		}

		board += q[i]->board.repeated_tiles();
	}

	total += board; // / q.size();

	return total;
}

double board_distance(const DominoBoard& left, const DominoBoard& right)
{
	vector<DominoPiece> l(left.board.begin(), left.board.end());
	vector<DominoPiece> r(right.board.begin(), right.board.end());

	double dist = GeneralizedLevenshteinDistance(l, r);

	return dist; // /(dist + 1);
}