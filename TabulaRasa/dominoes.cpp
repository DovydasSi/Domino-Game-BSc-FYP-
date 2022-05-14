#include "dominos.h"
#include "constants.h"

#include "json.h"
#include "messageQueue.h"

using namespace nlohmann;
bool DominoBoard::create_board_history(json msg, vector<PlayerMoveInfo>& new_bh)
{
	if (!msg.contains("type"))
	{
		return false;
	}

	new_bh.clear();

	MESSAGE_TYPE type = msg["type"];
	
	if (type == BOARD_HISTORY)
	{
		size_t size = msg["size"];
		for (size_t i = 0; i < size; i++)
		{
			json info_obj = msg["info" + to_string(i)]; // replace below with this and test it out 

			PlayerMoveInfo info;
			info.m_player_id = msg["info" + to_string(i)]["player"];
			info.m_piece.tile_from_string(msg["info" + to_string(i)]["tile"]);
			info.m_in_front = msg["info" + to_string(i)]["front"];

			new_bh.push_back(info);
		}
	}

	return true;
}


