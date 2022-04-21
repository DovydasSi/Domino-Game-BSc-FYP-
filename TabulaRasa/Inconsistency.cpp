#include "Inconsistency.h"

float total_inconsistency(std::function<float(const DominoInconsistency&, const DominoInconsistency&)> inconsistency,
	const DominoInconsistency* arr, size_t size)
{
	float count = 0.f, average = 0.f;

	for (int i = 0; i < size; i++)
	{
		for (int j = 0; j < i; j++)
		{
			count++;
			average += inconsistency(arr[i], arr[j]);
		}
	}
	return average / count;
}

float global_inconsistency(const std::vector<MessageQueue*> & q)
{
	float total = 0.f;
	for (int i = 0; i < q.size(); i++)
	{
		for (int j = 0; j < i; j++)
		{
			std::vector <DominoPiece> vec;
			set_intersection(q[i]->hand_history.begin(), q[i]->hand_history.end(), q[j]->hand_history.begin(), q[j]->hand_history.end(), back_inserter(vec));
			total += vec.size();
		}
	}

	total += q[0]->board.repeated_tiles();

	return total;
}