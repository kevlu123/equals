#include "Entry.h"
#include <sstream>
#include <iomanip>

static std::string HexString(uint32_t n) {
	std::stringstream ss;
	ss << std::hex << std::setfill('0') << std::setw(8) << (n & 0xFFFFFFFF);
	return ss.str();
}

std::string Entry::GetCRC32String() const {
	return std::visit([](auto&& arg) -> std::string {
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, std::monostate>) {
			return "-";
		} else if constexpr (std::is_same_v<T, Pending>) {
			return "...";
		} else if constexpr (std::is_same_v<T, std::string>) {
			return "!";
		} else {
			return HexString(arg);
		}
	}, crc32);
};

std::string Entry::GetPreCRC32String() const {
	return std::visit([](auto&& arg) -> std::string {
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, std::monostate>) {
			return "-";
		} else if constexpr (std::is_same_v<T, Pending>) {
			return "...";
		} else if constexpr (std::is_same_v<T, std::string>) {
			return "!";
		} else {
			return HexString(arg);
		}
	}, precrc32);
};

std::string Entry::GetFileSizeString() const {
	return std::visit([](auto&& arg) -> std::string {
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, std::monostate>) {
			return "-";
		} else if constexpr (std::is_same_v<T, Pending>) {
			return "...";
		} else if constexpr (std::is_same_v<T, std::string>) {
			return "!";
		} else {
			std::stringstream ss;
			ss << std::fixed << std::setprecision(2);

			constexpr uint64_t KB = 1024;
			constexpr uint64_t MB = 1024 * KB;
			constexpr uint64_t GB = 1024 * MB;
			uint64_t n = arg;
			if (n < KB) {
				ss << n << "B";
			} else if (n < MB) {
				ss << (n / (float)KB) << "KB";
			} else if (n < GB) {
				ss << (n / (float)MB) << "MB";
			} else {
				ss << (n / (float)GB) << "GB";
			}

			ss << " (" << n << ')';
			return ss.str();
		}
	}, filesize);
};

bool Entry::IsDefinitelyEqual(const Entry& lhs, const Entry& rhs) {
	return std::holds_alternative<uint32_t>(lhs.crc32) && std::holds_alternative<uint32_t>(rhs.crc32)
		&& std::get<uint32_t>(lhs.crc32) == std::get<uint32_t>(rhs.crc32);
}
