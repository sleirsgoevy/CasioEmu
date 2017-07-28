#include "Emulator.hpp"

#include <iostream>
#include <fstream>

namespace casioemu
{
	Emulator::Emulator(std::string _model_path, Uint32 _timer_interval, Uint32 _cycles_per_second) :
		config(_model_path),
		cycles(_cycles_per_second)
	{
		running = true;
		timer_interval = _timer_interval;
		model_path = _model_path;

		window = SDL_CreateWindow(
			config.model_name.c_str(),
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			config.interface_width,
			config.interface_height,
			SDL_WINDOW_SHOWN
		);
		if (!window)
			PANIC("SDL_CreateWindow failed: %s\n", SDL_GetError());

		cycles.Reset();

		SDL_AddTimer(timer_interval, [](Uint32 delay, void *param) {
			Emulator *emulator = (Emulator *)param;
			emulator->TimerCallback();
			return emulator->timer_interval;
		}, this);

		window_surface = SDL_GetWindowSurface(window);

		LoadInterfaceImage();

		SDL_FillRect(window_surface, nullptr, SDL_MapRGB(window_surface->format, 255, 255, 255));
		SDL_BlitSurface(interface_image_surface, nullptr, window_surface, nullptr);
		SDL_UpdateWindowSurface(window);
	}

	Emulator::~Emulator()
	{
	    SDL_FreeSurface(interface_image_surface);
		SDL_DestroyWindow(window);
	}

	void Emulator::LoadInterfaceImage()
	{
	    SDL_Surface *loaded_surface = IMG_Load((model_path + "/" + config.interface_image_path).c_str());
	    if (!loaded_surface)
	    	PANIC("IMG_Load failed: %s\n", IMG_GetError());

	    interface_image_surface = SDL_ConvertSurface(loaded_surface, window_surface->format, 0);
	    if (!interface_image_surface)
	    	PANIC("SDL_ConvertSurface failed: %s\n", SDL_GetError());

	    SDL_FreeSurface(loaded_surface);
	}

	void Emulator::TimerCallback()
	{
		Uint64 cycles_to_emulate = cycles.GetDelta();

		printf("Timer callback, will emulate %lu cycles\n", cycles_to_emulate);
	}

	bool Emulator::Running()
	{
		return running;
	}

	void Emulator::Shutdown()
	{
		running = false;
	}

	Emulator::Config::Config(std::string model_path)
	{
		// * Ugly config parser function.
		// * TODO: Maybe replace the whole thing with a smart map.

		std::ifstream config_file(model_path + "/" + MODEL_DEF_NAME);
		if (!config_file.is_open())
			PANIC("fopen failed: %s\n", strerror(errno));

		while (!config_file.eof())
		{
			std::string property_name;
			config_file >> property_name;
			if (config_file.eof())
				break;

			if (!property_name.compare("interface_image_path"))
			{
				config_file >> interface_image_path;
				std::cout << "[Config] interface_image_path: " << interface_image_path << std::endl;
			}

			if (!property_name.compare("model_name"))
			{
				config_file >> model_name;
				for (size_t ix = 0; ix != model_name.length(); ++ix)
					if (model_name[ix] == '_')
						model_name[ix] = ' ';
				std::cout << "[Config] model_name: " << model_name << std::endl;
			}

			if (!property_name.compare("rom_path"))
			{
				config_file >> rom_path;
				std::cout << "[Config] rom_path: " << rom_path << std::endl;
			}

			if (!property_name.compare("interface_size"))
			{
				config_file >> interface_width >> interface_height;
				std::cout << "[Config] interface_size: " << interface_width << " " << interface_height << std::endl;
			}

			if (config_file.fail())
				PANIC("config file failed to be read\n");
		}
	}

	Emulator::Cycles::Cycles(Uint64 _cycles_per_second)
	{
		cycles_per_second = _cycles_per_second;
		performance_frequency = SDL_GetPerformanceFrequency();
	}

	void Emulator::Cycles::Reset()
	{
		ticks_at_reset = SDL_GetPerformanceCounter();
		cycles_emulated = 0;
	}

	Uint64 Emulator::Cycles::GetDelta()
	{
		Uint64 ticks_now = SDL_GetPerformanceCounter();
		Uint64 cycles_to_have_been_emulated_by_now = (double)(ticks_now - ticks_at_reset) / performance_frequency * cycles_per_second;
		Uint64 diff = cycles_to_have_been_emulated_by_now - cycles_emulated;
		cycles_emulated = cycles_to_have_been_emulated_by_now;
		return diff;
	}
}
