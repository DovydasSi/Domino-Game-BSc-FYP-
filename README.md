# Domino-Game-BSc-FYP-

My Bachelor's Final Year Project code. 

A Simple game of Domino with 4 execution threads acting as players and communicating with each other through network messages.

The communication is simulated by players passing network messages to other players' message queues.

The game starts with the first player broadcasting the randomizer seed to synchronize the way each player shuffles their own local copy of the domino deck. A starter tile is placed onto the board from the top of the deck and all players are dealt a hand of X number of domino tiles. Then, players continue taking turns placing a tile on the board or drawing from the deck, informing the others of the turn action and the others then try to apply the same action to their own local boards or decks. The game ends when a player places the last domino tile in their hand onto the board and broadcasts an 'I Have Won' message to the other players.

My paper uses this game to research the effects of network inconsistency in multiplayer games. 

Inconsistency is injected by altering the brodcasted seed information for some of the players receiving it. The corrupted players play their games just like the others but, since their decks will be shuffled in a different order, they will have a different starting tile on their board and will attempt to participate in the game based on an incorect board.

At the end of the game the program computes a score representing how much 'damage' has been done to the game because of the inconsisteny. This score is based off difference in players' boards, the hand histories (if different players have ever held the same tile in their hands), and the repeated tiles placed on the board of each player.
