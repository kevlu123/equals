#pragma once
#include "SDL.h"
#include "SDL_ttf.h"
#include "FontRenderer.h"
#include "Entry.h"
#include "EqualityTable.h"

#include <memory>
#include <optional>
#include <string>
#include <stdint.h>

struct App {
	App();
	~App();
	void Run();
private:
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	int clientWidth = 0;
	int clientHeight = 0;

	TTF_Font* mainFont = nullptr;
	std::unique_ptr<FontRenderer> mainFontRenderer;

	SDL_Cursor* currentCursor = nullptr;
	SDL_Cursor* targetCursor = nullptr;
	SDL_Cursor* pointerCursor = nullptr;
	SDL_Cursor* handCursor = nullptr;
	bool tempCursorActive = false;

	const Uint8* keyboardState = nullptr;
	bool clicked = false;
	bool rightClicked = false;
	SDL_Point mousePos = {};

	EqualityTable equalityTable;
	float scroll = 0;

	void HandleEvent(SDL_Event& e);
	void Loop();
	void CopyToClipboard(const std::string& str);
	template <class T> void TextHovering(FileResult<T>& fileResult, const std::string& text);
	void SetTemporaryCursor(SDL_Cursor* cursor);
	void UpdateTitle();
	void ClampScroll();
	void UpdateTable();
	bool ShiftIsDown();
	void DrawEntry(
		const std::string& path,
		const std::string& filesize,
		const std::string& precrc32,
		const std::string& crc32,
		const SDL_Color& colour,
		const SDL_Color& backgroundColour,
		int y,
		int* whichTextHovered = nullptr);
};
