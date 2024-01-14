#pragma once
#include <stdint.h>
#include <vector>
#include <memory>
#include <deque>
#include <set>
#include <mutex>
#include "Entry.h"

struct EqualityTable {
	struct EntryComparer {
		using is_transparent = void;
		EntryComparer(EqualityTable* equalityTable);
		bool operator()(const std::unique_ptr<Entry>& lhs, const std::unique_ptr<Entry>& rhs) const;
		bool operator()(const std::unique_ptr<Entry>& lhs, Entry* rhs) const;
		bool operator()(Entry* lhs, const std::unique_ptr<Entry>& rhs) const;
	private:
		bool operator()(Entry* lhs, Entry* rhs) const;
		EqualityTable* equalityTable;
	};
	struct Task {
		std::future<std::function<void()>> future;
		Entry* entry;
	};
	using Container = std::set<std::unique_ptr<Entry>, EntryComparer>;

	EqualityTable();
	~EqualityTable();
	Container::const_iterator begin() const;
	Container::const_iterator end() const;
	size_t size() const;
	void Insert(std::string path);
	void Remove(const std::string& path);
	void Tick();
	Container::const_iterator GetIndex(size_t index) const;
	size_t DuplicateCount() const;
	size_t PendingCount() const;
private:
	void InsertNewFile(std::string path);
	void InsertEntry(std::unique_ptr<Entry> entry);
	std::unique_ptr<Entry> Remove(Entry* entry);
	std::unique_ptr<Entry> Remove(Container::iterator it);

	std::set<std::string> paths;
	std::set<std::unique_ptr<Entry>, EntryComparer> entries;
	std::deque<Task> tasks;
	bool inserting = false;
	std::mutex mutex;
	size_t duplicateCount = 0;
	bool closing = false;

	mutable size_t cachedIteratorIndex = 0;
	mutable Container::const_iterator cachedIterator;
	mutable bool iteratorInvalidated = true;
};
