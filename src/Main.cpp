#include "SDL.h"
#include "app.h"
#include <exception>

int main(int argc, char** argv) {
#if !DEBUG
	try {
#endif
		App app;
		app.Run();
		return 0;
#if !DEBUG
	} catch (std::exception& e) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", e.what(), nullptr);
		return 1;
	}
#endif
}
