#include "player.h"
#include <string>

extern Output_t LinuxDvbOutput;

Player::Player()
{
	output = &LinuxDvbOutput;
}

Player::~Player()
{
}
