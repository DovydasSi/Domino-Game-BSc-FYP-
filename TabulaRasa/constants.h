#pragma once

#define MAX_PROC_NUM         (4) // Number of players
#define NUM_OF_CYCLES        (1) 
#define DROP_ALTER_CHANCE    (2)
#define TILES_IN_HAND        (5)

// Normal Dominos go from 0 to 6 
// but we might want to change that 
#define MIN_TILE_NUM         (0)
#define MAX_TILE_NUM         (6)

//bools
#define DROP_MESSAGE         (0)
#define ALTER_SEED           (1)
#define REPAIR_SEED          (0)
#define ALTER_HAS_WON        (0)
#define REPAIR_HAS_WON       (0)
#define REPAIR_BOARD_HISTORY (0)

// "Host" broadcasts the first tile if 1
// First tile is local to everybody if 0
#define NEW_INITIAL_TILE     (1)