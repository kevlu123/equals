#include "EqualityTable.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <array>
#include <thread>
#include "Crc32.h"

static FileResult<uint64_t> ComputeFileSize(Entry* entry) {
	std::error_code ec;
	if (!std::filesystem::is_regular_file(entry->path, ec)) {
		return "Not a regular file";
	} else if (ec) {
		return ec.message();
	}

	auto result = (uint64_t)std::filesystem::file_size(entry->path, ec);
	if (ec) {
		return ec.message();
	}
	return result;
}

static FileResult<uint32_t> ComputePreCRC32(Entry* entry) {
	if (std::get<uint64_t>(entry->filesize) == 0) {
		return (uint32_t)0;
	}

	std::ifstream file(entry->path, std::ios::binary);
	if (!file) {
		return "Failed to open file";
	}

	std::array<char, 1024> buffer;
	file.read(buffer.data(), buffer.size());
	if (!file.eof() && !file) {
		return "Failed to read file";
	}
	auto bufferLen = file.gcount();
	return crc32_fast(buffer.data(), bufferLen);
}

static FileResult<uint32_t> ComputeCRC32(Entry* entry) {
	if (std::get<uint64_t>(entry->filesize) <= 1024) {
		return entry->precrc32;
	}

	std::ifstream file(entry->path, std::ios::binary);
	if (!file) {
		return "Failed to open file";
	}

	std::vector<char> buffer(1024 * 1024 * 1);
	uint32_t crc32 = 0;
	while (!file.eof()) {
		file.read(buffer.data(), buffer.size());
		if (!file.eof() && !file) {
			return "Failed to read file";
		}
		size_t bufferLen = (size_t)file.gcount();
		crc32 = crc32_fast(buffer.data(), bufferLen, crc32);
	}
	return crc32;
}

EqualityTable::EqualityTable()
	: entries(EntryComparer(this)) { }

EqualityTable::~EqualityTable() {
	{
		std::unique_lock lock(mutex);
		closing = true;
	}
}

EqualityTable::Container::const_iterator EqualityTable::begin() const {
	return entries.begin();
}

EqualityTable::Container::const_iterator EqualityTable::end() const {
	return entries.end();
}

size_t EqualityTable::size() const {
	return entries.size();
}

void EqualityTable::Insert(std::string path) {
	auto future = std::async(std::launch::async, [this, path]() ->std::function<void()> {
		if (std::filesystem::is_directory(path)) {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
				if (entry.is_regular_file()) {
					std::string canonicalPath = std::filesystem::canonical(entry.path()).string();
					std::unique_lock lock(mutex);
					if (closing) break;
					InsertNewFile(std::move(canonicalPath));
				}
			}
			return [] {};
		} else {
			std::string canonicalPath = std::filesystem::canonical(path).string();
			std::unique_lock lock(mutex);
			InsertNewFile(std::move(canonicalPath));
			return [] {};
		}
		});
	tasks.push_back(Task{ std::move(future), nullptr });
}

void EqualityTable::InsertNewFile(std::string path) {
	auto entry = std::make_unique<Entry>();
	entry->path = std::move(path);
	entry->colour.r = rand() % 255;
	entry->colour.g = rand() % 255;
	entry->colour.b = rand() % 255;
	InsertEntry(std::move(entry));
}

void EqualityTable::InsertEntry(std::unique_ptr<Entry> entry) {
	if (paths.contains(entry->path)) return;
	iteratorInvalidated = true;
	paths.insert(entry->path);
	inserting = true;
	auto [it, _] = entries.insert(std::move(entry));
	inserting = false;

	if (auto prev = it; prev != entries.begin() && Entry::IsDefinitelyEqual(**--prev, **it)) {
		duplicateCount++;
		(*it)->colour = (*prev)->colour;
	}
	if (auto next = it; ++next != entries.end() && Entry::IsDefinitelyEqual(**next, **it)) {
		duplicateCount++;
	}

	auto next = it;
	++next;
	for (; next != entries.end() && Entry::IsDefinitelyEqual(**next, **it); ++next) {
		(*next)->colour = (*it)->colour;
	}
}

void EqualityTable::Remove(const std::string& path) {
	auto it = std::find_if(
		entries.begin(), entries.end(),
		[&](const std::unique_ptr<Entry>& entry) {
			return entry->path == path;
		}
	);
	Remove(it);
}

std::unique_ptr<Entry> EqualityTable::Remove(Entry* entry) {
	return Remove(entries.find(entry));
}

std::unique_ptr<Entry> EqualityTable::Remove(Container::iterator it) {
	iteratorInvalidated = true;

	if (auto prev = it; prev != entries.begin() && Entry::IsDefinitelyEqual(**--prev, **it)) {
		duplicateCount--;
	} else if (auto next = it; ++next != entries.end() && Entry::IsDefinitelyEqual(**next, **it)) {
		duplicateCount--;
	}

	auto entryNh = entries.extract(it);
	paths.erase(entryNh.value()->path);
	return std::move(entryNh.value());
}

void EqualityTable::Tick() {
	std::unique_lock lock(mutex);
	std::vector<std::unique_ptr<Entry>> updated;
	for (auto it = tasks.begin(); it != tasks.end();) {
		if (it->future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
			++it;
			continue;
		}

		if (it->entry) {
			updated.push_back(Remove(it->entry));
		}
		it->future.get()();
		it = tasks.erase(it);
	}

	for (auto& entry : updated) {
		InsertEntry(std::move(entry));
	}
}

EqualityTable::Container::const_iterator EqualityTable::GetIndex(size_t index) const {
	if (iteratorInvalidated || cachedIteratorIndex != index) {
		auto it = entries.begin();
		std::advance(it, index);
		cachedIterator = it;
		cachedIteratorIndex = index;
		return it;
	} else {
		return cachedIterator;
	}
}

size_t EqualityTable::DuplicateCount() const {
	return duplicateCount;
}

size_t EqualityTable::PendingCount() const {
	return tasks.size();
}

EqualityTable::EntryComparer::EntryComparer(EqualityTable* equalityTable)
	: equalityTable(equalityTable) { }

bool EqualityTable::EntryComparer::operator()(const std::unique_ptr<Entry>& lhs, const std::unique_ptr<Entry>& rhs) const {
	return this->operator()(lhs.get(), rhs.get());
}

bool EqualityTable::EntryComparer::operator()(const std::unique_ptr<Entry>& lhs, Entry* rhs) const {
	return this->operator()(lhs.get(), rhs);
}

bool EqualityTable::EntryComparer::operator()(Entry* lhs, const std::unique_ptr<Entry>& rhs) const {
	return this->operator()(lhs, rhs.get());
}

template <class FileResultType>
bool ResultNotComputed(const FileResult<FileResultType>& fileResult) {
	return std::holds_alternative<std::monostate>(fileResult)
		|| std::holds_alternative<Pending>(fileResult);
}

template <class FileResultType>
static int CompareEntryMember(Entry* lhsEntry, Entry* rhsEntry, FileResult<FileResultType> Entry::* member, bool& laddTask, bool& raddTask) {
	auto& lhs = lhsEntry->*member;
	auto& rhs = rhsEntry->*member;

	if (std::holds_alternative<ErrorMessage>(lhs) && std::holds_alternative<ErrorMessage>(rhs)) {
		return lhsEntry->path.compare(rhsEntry->path);
	} else if (std::holds_alternative<ErrorMessage>(lhs)) {
		return 1;
	} else if (std::holds_alternative<ErrorMessage>(rhs)) {
		return -1;
	} else if (ResultNotComputed(lhs) && ResultNotComputed(rhs)) {
		laddTask = true;
		raddTask = true;
		return lhsEntry->path.compare(rhsEntry->path);
	} else if (ResultNotComputed(lhs)) {
		laddTask = true;
		return 1;
	} else if (ResultNotComputed(rhs)) {
		raddTask = true;
		return -1;
	} else if (std::get<FileResultType>(lhs) != std::get<FileResultType>(rhs)) {
		return std::get<FileResultType>(lhs) > std::get<FileResultType>(rhs) ? -1 : 1;
	} else {
		return 0;
	}
}

template <class FileResultType>
static std::optional<EqualityTable::Task> CreateTask(Entry* entry, FileResult<FileResultType> Entry::* member, FileResult<FileResultType>(work)(Entry* entry)) {
	if (std::holds_alternative<Pending>(entry->*member)) {
		return std::nullopt;
	}

	entry->*member = Pending();
	auto future = std::async(std::launch::async, [entry, member, work]() -> std::function<void()> {
		auto result = work(entry);
		return [entry, member, result]() { entry->*member = result; };
		});
	return EqualityTable::Task{ std::move(future), entry };
}

bool EqualityTable::EntryComparer::operator()(Entry* lhs, Entry* rhs) const {
	enum class TaskType {
		FileSize,
		PreCRC32,
		CRC32,
	};

	struct TaskInfo {
		TaskType type;
		Entry* entry;
	};

	bool less = false;
	bool laddTask = false;
	bool raddTask = false;
	std::array<std::optional<TaskInfo>, 2> newTaskInfos;
	if (int x = CompareEntryMember(lhs, rhs, &Entry::filesize, laddTask, raddTask)) {
		less = x < 0;
		if (laddTask) newTaskInfos[0] = TaskInfo{ TaskType::FileSize, lhs };
		if (raddTask) newTaskInfos[1] = TaskInfo{ TaskType::FileSize, rhs };
	} else if (int x = CompareEntryMember(lhs, rhs, &Entry::precrc32, laddTask, raddTask)) {
		less = x < 0;
		if (laddTask) newTaskInfos[0] = TaskInfo{ TaskType::PreCRC32, lhs };
		if (raddTask) newTaskInfos[1] = TaskInfo{ TaskType::PreCRC32, rhs };
	} else if (int x = CompareEntryMember(lhs, rhs, &Entry::crc32, laddTask, raddTask)) {
		less = x < 0;
		if (laddTask) newTaskInfos[0] = TaskInfo{ TaskType::CRC32, lhs };
		if (raddTask) newTaskInfos[1] = TaskInfo{ TaskType::CRC32, rhs };
	} else {
		less = lhs->path < rhs->path;
	}

	if (!equalityTable->inserting) {
		return less;
	}

	for (auto& newTaskInfo : newTaskInfos) {
		if (!newTaskInfo) continue;

		switch (newTaskInfo->type) {
		case TaskType::FileSize:
			if (auto task = CreateTask(newTaskInfo->entry, &Entry::filesize, ComputeFileSize)) {
				equalityTable->tasks.push_back(std::move(*task));
			}
			break;
		case TaskType::PreCRC32:
			if (auto task = CreateTask(newTaskInfo->entry, &Entry::precrc32, ComputePreCRC32)) {
				equalityTable->tasks.push_back(std::move(*task));
			}
			break;
		case TaskType::CRC32:
			if (auto task = CreateTask(newTaskInfo->entry, &Entry::crc32, ComputeCRC32)) {
				equalityTable->tasks.push_back(std::move(*task));
			}
			break;
		}
	}

	return less;
}
