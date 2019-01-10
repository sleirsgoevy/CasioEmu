#include "Config.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <thread>
#include <string>
#include <map>
#include <mutex>
#include <condition_variable>

#include "Emulator.hpp"
#include "Logger.hpp"
#include "Data/EventCode.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <readline/readline.h>
#include <readline/history.h>

using namespace casioemu;

std::mutex input_mx;
int main(int argc, char *argv[])
{
	std::map<std::string, std::string> argv_map;
	for (int ix = 1; ix != argc; ++ix)
	{
		std::string key, value;
		char *eq_pos = strchr(argv[ix], '=');
		if (eq_pos)
		{
			key = std::string(argv[ix], eq_pos);
			value = eq_pos + 1;
		}
		else
		{
			key = "model";
			value = argv[ix];
		}

		if (argv_map.find(key) == argv_map.end())
			argv_map[key] = value;
		else
			logger::Info("[argv] #%i: key '%s' already set\n", ix, key.c_str());
	}

	if (argv_map.find("model") == argv_map.end())
	{
		printf("No model path supplied\n");
		exit(2);
	}

	int sdlFlags = SDL_INIT_VIDEO & SDL_INIT_TIMER;
	if (SDL_Init(sdlFlags) != sdlFlags)
		PANIC("SDL_Init failed: %s\n", SDL_GetError());

	int imgFlags = IMG_INIT_PNG;
	if (IMG_Init(imgFlags) != imgFlags)
		PANIC("IMG_Init failed: %s\n", IMG_GetError());

	std::string history_filename;
	auto history_filename_iter = argv_map.find("history");
	if (history_filename_iter != argv_map.end())
		history_filename = history_filename_iter->second;

	if (!history_filename.empty())
	{
		int err = read_history(history_filename.c_str());
		if (err && err != ENOENT)
			PANIC("error while reading history file: %s\n", std::strerror(err));
	}

	{
		Emulator emulator(argv_map, 20, 128 * 1024);

		std::condition_variable input_cv;
		bool input_processed;
		std::string console_input_str;
		std::thread console_input_thread([&] {
			while (1)
			{
				char *console_input_c_str = readline("> ");
				if (console_input_c_str == NULL)
				{
					std::cout << '\n';

					SDL_Event event;
					SDL_zero(event);
					event.type = SDL_WINDOWEVENT;
					event.window.event = SDL_WINDOWEVENT_CLOSE;
					SDL_PushEvent(&event);
					break;
				}

				// Ignore empty lines.
				if (console_input_c_str[0] == 0)
					continue;

				add_history(console_input_c_str);
				console_input_str = console_input_c_str;
				free(console_input_c_str);

				input_processed = false;
				SDL_Event event;
				SDL_zero(event);
				event.type = SDL_USEREVENT;
				event.user.code = CE_EVENT_INPUT;
				SDL_PushEvent(&event);

				std::unique_lock<std::mutex> input_lock(input_mx);
				input_cv.wait(input_lock, [&] {
					return input_processed;
				});
			}
		});
		console_input_thread.detach();

		while (emulator.Running())
		{
			SDL_Event event;
			if (!SDL_WaitEvent(&event))
				PANIC("SDL_WaitEvent failed: %s\n", SDL_GetError());

			switch (event.type)
			{
			case SDL_USEREVENT:
				switch (event.user.code)
				{
				case CE_EVENT_INPUT:
					{
						std::lock_guard<std::mutex> input_lock(input_mx);
						emulator.ExecuteCommand(console_input_str);
						input_processed = true;
					}
					input_cv.notify_one();
					break;

				case CE_FRAME_REQUEST:
					emulator.Frame();
					break;
				}
				break;

			case SDL_WINDOWEVENT:
				switch (event.window.event)
				{
				case SDL_WINDOWEVENT_CLOSE:
					emulator.Shutdown();
					break;
				case SDL_WINDOWEVENT_RESIZED:
					{
						SDL_Event event;
						SDL_zero(event);
						event.type = SDL_USEREVENT;
						event.user.code = CE_FRAME_REQUEST;
						SDL_PushEvent(&event);
					}
					break;
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				emulator.UIEvent(event);
				break;
			}
		}

		input_mx.lock();
	}

	IMG_Quit();
	SDL_Quit();

	rl_free_line_state();
	rl_deprep_terminal();
	std::cout << '\n';

	if (!history_filename.empty())
	{
		int err = write_history(history_filename.c_str());
		if (err)
			PANIC("error while writing history file: %s\n", std::strerror(err));
	}

	return 0;
}
