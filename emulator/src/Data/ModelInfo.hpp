#pragma once
#include "../Config.hpp"

#include <string>

namespace casioemu
{
	class Emulator;
	struct SpriteInfo;
	struct ColourInfo;

	struct ModelInfo
	{
		ModelInfo(Emulator &emulator, std::string key);
		Emulator &emulator;
		std::string key;

		operator std::string();
		operator int();
		operator SpriteInfo();
		operator ColourInfo();
	};
}

