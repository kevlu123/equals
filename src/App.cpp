#include "app.h"
#include "font/JetBrainsMono-Regular.h"

#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <filesystem>

App::App() {
	int clientWidth = 800;
	int clientHeight = 400;
	Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE /*| SDL_WINDOW_ALWAYS_ON_TOP*/;
	SDL_RWops* fontStream = nullptr;

	if (SDL_Init(SDL_INIT_EVERYTHING))
		goto error;

	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

	if (TTF_Init())
		goto error;

	window = SDL_CreateWindow("Equivalence", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, clientWidth, clientHeight, windowFlags);
	if (window == nullptr)
		goto error;

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (renderer == nullptr)
		goto error;

	fontStream = SDL_RWFromConstMem(JET_BRAINS_MONO_FONT, sizeof(JET_BRAINS_MONO_FONT));
	if (fontStream == nullptr)
		goto error;

	mainFont = TTF_OpenFontRW(fontStream, 1, 14);
	fontStream = nullptr;
	if (mainFont == nullptr)
		goto error;

	mainFontRenderer = std::make_unique<FontRenderer>(renderer, mainFont);

	pointerCursor = SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_ARROW);
	handCursor = SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_HAND);
	currentCursor = pointerCursor;

	keyboardState = SDL_GetKeyboardState(nullptr);

	this->clientWidth = clientWidth;
	this->clientHeight = clientHeight;
	return;

error:
	if (mainFont)
		TTF_CloseFont(mainFont);
	if (TTF_WasInit())
		TTF_Quit();
	if (renderer)
		SDL_DestroyRenderer(renderer);
	if (window)
		SDL_DestroyWindow(window);
	SDL_Quit();
	throw std::runtime_error(SDL_GetError());
}

App::~App() {
	SDL_FreeCursor(pointerCursor);
	SDL_FreeCursor(handCursor);
	TTF_CloseFont(mainFont);
	TTF_Quit();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void App::Run() {
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

	SDL_Event e;
	while (true) {
		clicked = false;
		rightClicked = false;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				return;
			HandleEvent(e);
		}
		SDL_GetMouseState(&mousePos.x, &mousePos.y);

		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_RenderClear(renderer);
		Loop();
		SDL_RenderPresent(renderer);

		SDL_Delay(10);
	}
}

void App::HandleEvent(SDL_Event& e) {
	switch (e.type) {
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
				clientWidth = (int)e.window.data1;
				clientHeight = (int)e.window.data2;
			}
			break;
		case SDL_DROPFILE:
			equalityTable.Insert(e.drop.file);
			SDL_free(e.drop.file);
			break;
		case SDL_MOUSEWHEEL:
			scroll -= e.wheel.y * 8;
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (e.button.button == SDL_BUTTON_LEFT) {
				clicked = true;
			} else if (e.button.button == SDL_BUTTON_RIGHT) {
				rightClicked = true;
			}
			break;
		case SDL_MOUSEMOTION:
			tempCursorActive = false;
			break;
	}
}

void App::SetTemporaryCursor(SDL_Cursor* cursor) {
	SDL_SetCursor(pointerCursor);
	currentCursor = cursor;
	tempCursorActive = true;
}

void App::CopyToClipboard(const std::string & str) {
	if (SDL_SetClipboardText(str.c_str())) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Clipboard Error", SDL_GetError(), window);
	}
	SetTemporaryCursor(pointerCursor);
}

template <class T>
void App::TextHovering(FileResult<T>& fileResult, const std::string& text) {
	if (auto* err = std::get_if<ErrorMessage>(&fileResult)) {
		targetCursor = handCursor;
		if (clicked) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Fail Reason", err->c_str(), window);
		}
	} else if (auto* value = std::get_if<T>(&fileResult)) {
		targetCursor = handCursor;
		if (clicked) {
			if (ShiftIsDown()) {
				CopyToClipboard(std::to_string(*value));
			} else {
				CopyToClipboard(text);
			}
		}
	}
}

void App::UpdateTitle() {
	std::string title = "Equivalence | "
		+ std::to_string(equalityTable.size())
		+ " files | "
		+ std::to_string(equalityTable.DuplicateCount())
		+ " duplicates | "
		+ std::to_string(equalityTable.PendingCount())
		+ " pending tasks";
	SDL_SetWindowTitle(window, title.c_str());
}

void App::ClampScroll() {
	if ((float)equalityTable.size() * mainFontRenderer->GetHeight() - clientHeight > 0) {
		scroll = std::clamp(scroll, 0.0f, (float)equalityTable.size() * mainFontRenderer->GetHeight() - clientHeight);
	} else {
		scroll = 0;
	}
}

bool App::ShiftIsDown() {
	return keyboardState[SDL_SCANCODE_LSHIFT] || keyboardState[SDL_SCANCODE_RSHIFT];
}

void App::UpdateTable() {
	int i = scroll / mainFontRenderer->GetHeight();
	auto it = equalityTable.GetIndex((size_t)i);
	for (; it != equalityTable.end(); ++it, ++i) {
		auto& cur = **it;
		SDL_Color colour{};
		if (auto prev = it; prev != equalityTable.begin() && Entry::IsDefinitelyEqual(**--prev, cur)) {
			colour = (*it)->colour;
		} else if (auto next = it; ++next != equalityTable.end() && Entry::IsDefinitelyEqual(**next, cur)) {
			colour = (*it)->colour;
		}

		std::string filesizeString = cur.GetFileSizeString();
		std::string preCRC32String = cur.GetPreCRC32String();
		std::string crc32String = cur.GetCRC32String();
		int y = (i + 1) * mainFontRenderer->GetHeight() - scroll;
		int whichTextHovered;
		DrawEntry(
			cur.path,
			filesizeString,
			preCRC32String,
			crc32String,
			colour,
			{ 255, 255, 255, 255 },
			y,
			&whichTextHovered
		);

		switch (whichTextHovered) {
		case 1:
			targetCursor = handCursor;
			if (clicked) {
				std::string text = cur.path;
				if (ShiftIsDown()) {
					std::transform(text.begin(), text.end(), text.begin(), [](char c) { return c == '\\' ? '/' : c; });
				}
				CopyToClipboard(text);
			}
			break;
		case 2:
			TextHovering(cur.filesize, filesizeString);
			break;
		case 3:
			TextHovering(cur.precrc32, preCRC32String);
			break;
		case 4:
			TextHovering(cur.crc32, crc32String);
			break;
		}

		if (y > clientHeight) {
			break;
		}
	}

	DrawEntry(
		"Path",
		"File Size",
		"1KB CRC32",
		"CRC32",
		{ 240, 240, 240, 255 },
		{ 240, 240, 240, 255 },
		0
	);
}

void App::Loop() {
	targetCursor = pointerCursor;

	equalityTable.Tick();
	UpdateTitle();
	ClampScroll();
	UpdateTable();

	if (targetCursor != currentCursor && !tempCursorActive) {
		SDL_SetCursor(targetCursor);
		currentCursor = targetCursor;
	}
}

void App::DrawEntry(
	const std::string& path,
	const std::string& filesize,
	const std::string& precrc32,
	const std::string& crc32,
	const SDL_Color& colour,
	const SDL_Color& backgroundColour,
	int y,
	int* whichTextHovered)
{
	int height = mainFontRenderer->GetHeight();
	int bottomY = y + height;
	int colourWidth = height;

	SDL_Rect bgRc = { 0, y, clientWidth, bottomY };
	SDL_SetRenderDrawColor(renderer, backgroundColour.r, backgroundColour.g, backgroundColour.b, backgroundColour.a);
	SDL_RenderFillRect(renderer, &bgRc);

	if (colour.a == 0) {
		SDL_Rect rc = { 0, y, colourWidth / 2, height / 2 };
		SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
		SDL_RenderFillRect(renderer, &rc);

		rc.x += colourWidth / 2;
		rc.y += height / 2;
		SDL_RenderFillRect(renderer, &rc);
	} else {
		SDL_Rect colourRc = { 0, y, colourWidth, height };
		SDL_SetRenderDrawColor(renderer, colour.r, colour.g, colour.b, colour.a);
		SDL_RenderFillRect(renderer, &colourRc);
	}

	constexpr int padding = 5;
	SDL_Rect pathRect = mainFontRenderer->RenderText(path, colourWidth + padding, y, clientWidth / 2 - 2 * padding - colourWidth);

	int columnCount = 8;
	int columnWidth = clientWidth / 8;
	SDL_Rect filesizeRect = mainFontRenderer->RenderTextCentered(filesize, 4 * columnWidth, y, 2 * columnWidth, padding);
	SDL_Rect precrc32Rect = mainFontRenderer->RenderTextCentered(precrc32, 6 * columnWidth, y,     columnWidth, padding);
	SDL_Rect crc32Rect = mainFontRenderer->RenderTextCentered(crc32,    7 * columnWidth, y,     columnWidth, padding);

	SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
	SDL_RenderDrawLine(renderer, colourWidth, y, clientWidth, y);
	SDL_RenderDrawLine(renderer, colourWidth, bottomY, clientWidth, bottomY);
	SDL_RenderDrawLine(renderer, 4 * columnWidth, y, 4 * columnWidth, bottomY);
	SDL_RenderDrawLine(renderer, 6 * columnWidth, y, 6 * columnWidth, bottomY);
	SDL_RenderDrawLine(renderer, 7 * columnWidth, y, 7 * columnWidth, bottomY);

	if (whichTextHovered) {
		*whichTextHovered = -1;
		if (SDL_PointInRect(&mousePos, &pathRect)) {
			*whichTextHovered = 1;
		} else if (SDL_PointInRect(&mousePos, &filesizeRect)) {
			*whichTextHovered = 2;
		} else if (SDL_PointInRect(&mousePos, &precrc32Rect)) {
			*whichTextHovered = 3;
		} else if (SDL_PointInRect(&mousePos, &crc32Rect)) {
			*whichTextHovered = 4;
		} else if (SDL_PointInRect(&mousePos, &bgRc)) {
			*whichTextHovered = 0;
		}
	}
}
