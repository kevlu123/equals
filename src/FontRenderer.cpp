#include "FontRenderer.h"
#include <stdexcept>

FontRenderer::FontRenderer(SDL_Renderer* renderer, TTF_Font* font)
	: renderer(renderer), font(font)
{
	height = TTF_FontHeight(font);
}

FontRenderer::~FontRenderer() {
	for (auto& pair : cache) {
		SDL_DestroyTexture(pair.second.texture);
	}
}

SDL_Rect FontRenderer::RenderText(std::string text, int x, int y, int maxWidth) {
	SDL_Rect dest{};
	SDL_Texture* texture = GetTexture(std::move(text), x, y, maxWidth);

	int texWidth, texHeight;
	if (SDL_QueryTexture(texture, nullptr, nullptr, &texWidth, &texHeight))
		goto error;

	dest = SDL_Rect{ x, y, texWidth, texHeight };
	if (SDL_RenderCopy(renderer, texture, nullptr, &dest))
		goto error;

	return dest;

error:
	throw std::runtime_error(SDL_GetError());
}

SDL_Rect FontRenderer::RenderTextCentered(std::string text, int x, int y, int width, int padding) {
	SDL_Rect dest{};
	int freeSpace = 0;

	width -= 2 * padding;
	SDL_Texture* texture = GetTexture(std::move(text), x, y, width);

	int texWidth, texHeight;
	if (SDL_QueryTexture(texture, nullptr, nullptr, &texWidth, &texHeight))
		goto error;

	freeSpace = width - texWidth;
	dest = SDL_Rect{ x + freeSpace / 2 + padding, y, texWidth, texHeight };
	if (SDL_RenderCopy(renderer, texture, nullptr, &dest))
		goto error;

	return dest;

error:
	throw std::runtime_error(SDL_GetError());
}

int FontRenderer::MeasureText(const std::string& text, int maxWidth) {
	// Check if the texture is cached
	if (auto it = cache.find(text); it != cache.end()) {
		SDL_Texture* texture = it->second.texture;
		int width;
		if (SDL_QueryTexture(texture, nullptr, nullptr, &width, nullptr))
			goto error;
		return width <= maxWidth ? width : maxWidth;
	}

	// Calculate extent
	{
		int extent, count;
		if (TTF_MeasureUTF8(font, text.data(), maxWidth, &extent, &count))
			goto error;
		return extent;
	}

error:
	throw std::runtime_error(SDL_GetError());
}

void FontRenderer::Tick() {
	// Destroy unused textures
	for (auto it = cache.begin(); it != cache.end();) {
		if (!it->second.used) {
			SDL_DestroyTexture(it->second.texture);
			it = cache.erase(it);
		} else {
			++it;
		}
	}

	// Reset the used flag
	for (auto& pair : cache) {
		pair.second.used = false;
	}
}

int FontRenderer::GetHeight() const {
	return height;
}

SDL_Texture* FontRenderer::GetTexture(std::string text, int x, int y, int maxWidth) {
	SDL_Texture* texture = nullptr;

	// Check if the texture is cached
	if (auto it = cache.find(text); it != cache.end()) {
		texture = it->second.texture;
		// Check if the existing texture is within the maxWidth
		int width;
		if (SDL_QueryTexture(texture, nullptr, nullptr, &width, nullptr))
			goto error;
		if (width <= maxWidth) {
			it->second.used = true;
			return texture;
		}
	}

	// Trim the text to fit the width, then check again if the texture is cached
	{
		int extent, count;
		if (TTF_MeasureUTF8(font, text.data(), maxWidth, &extent, &count))
			goto error;
		if (count < text.size())
			text.resize(count);

		if (auto it = cache.find(text); it != cache.end()) {
			texture = it->second.texture;
			it->second.used = true;
			return texture;
		}
	}

	// Texture is not cached so create a new texture
	{
		if (text.empty()) {
			text = " ";
		}

		constexpr SDL_Color colour = { 0, 0, 0 };
		SDL_Surface* surface = TTF_RenderText_Blended(font, text.data(), colour);
		if (surface == nullptr)
			goto error;

		texture = SDL_CreateTextureFromSurface(renderer, surface);
		SDL_FreeSurface(surface);
		if (texture == nullptr)
			goto error;

		cache.emplace(std::move(text), TextureInfo{
			.texture = texture,
			.used = true,
			});
		return texture;
	}

error:
	throw std::runtime_error(SDL_GetError());
}
