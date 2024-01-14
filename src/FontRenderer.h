#pragma once
#include "SDL_ttf.h"
#include <unordered_map>
#include <string>
#include <limits>

class FontRenderer {
public:
	FontRenderer(SDL_Renderer* renderer, TTF_Font* font);
	~FontRenderer();
	FontRenderer(const FontRenderer& other) = delete;
	FontRenderer& operator=(const FontRenderer& other) = delete;
	SDL_Rect RenderText(std::string text, int x, int y, int maxWidth = std::numeric_limits<int>::max());
	SDL_Rect RenderTextCentered(std::string text, int x, int y, int width, int padding = 0);
	int MeasureText(const std::string& text, int maxWidth = std::numeric_limits<int>::max());
	void Tick();
	int GetHeight() const;
private:
	SDL_Texture* GetTexture(std::string text, int x, int y, int maxWidth);

	struct TextureInfo {
		SDL_Texture* texture;
		bool used;
	};

	SDL_Renderer* renderer;
	TTF_Font* font;
	std::unordered_map<std::string, TextureInfo> cache;
	int height = 0;
};