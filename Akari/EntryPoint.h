#pragma once

#include "Application.h"

extern Akari::Application* Akari::CreateApplication(int argc, char** argv);
bool g_ApplicationRunning = true;

namespace Akari {

	int Main(int argc, char** argv)
	{
		while (g_ApplicationRunning)
		{
			Akari::Application* app = Akari::CreateApplication(argc, argv);
			app->Run();
			delete app;
		}

		return 0;
	}

}

int main(int argc, char** argv)
{
	return Akari::Main(argc, argv);
}