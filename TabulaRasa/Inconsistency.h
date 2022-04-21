#pragma once

#include "dominos.h"
#include "messageQueue.h"
#include <functional>

float total_inconsistency(std::function<float(const DominoInconsistency&, const DominoInconsistency&)> inconsistency,
	const DominoInconsistency* arr, size_t size);

float global_inconsistency(const std::vector<MessageQueue*>& q);