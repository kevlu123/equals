#pragma once
#include <string>
#include <optional>
#include <stdint.h>
#include <variant>
#include <future>
#include "SDL.h"

using ErrorMessage = std::string;
struct Pending {};
template <class T> using FileResult = std::variant<
	std::monostate,
	Pending,
	ErrorMessage,
	T>;

struct Entry {
	std::string path;
	FileResult<uint64_t> filesize;
	FileResult<uint32_t> precrc32;
	FileResult<uint32_t> crc32;
	SDL_Color colour = { 255, 255, 255, 255 };

	std::string GetFileSizeString() const;
	std::string GetPreCRC32String() const;
	std::string GetCRC32String() const;

	static bool IsDefinitelyEqual(const Entry& lhs, const Entry& rhs);
};
