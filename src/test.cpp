#include "btree.h"
#include <iostream>
#include <random>
#include <chrono>
#include <string>

void jsonSerializationTests(BTree<int, std::string>* tree) {
	const int insertions = 11;

	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::mt19937 generate(seed);

	for (int i = 0; i < insertions; ++i)
	{
		int toInsert = generate();

		tree->insert(toInsert, "1");
	}

	std::cout << tree->serializeToJson() << std::endl;
}

void standardTests(BTree<int, std::string>* tree) {
	const int insertions = 1e6;
	const int middle = insertions / 2;

	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::mt19937 generate(seed);

	int firstKey, middleKey, lastKey;
	std::vector<int> keysToRemove;
	std::vector<int> keysToSearch;
	std::vector<int> bogusSearch;

	auto t0_insert = std::chrono::steady_clock::now();

	for (int i = 0; i < insertions; ++i)
	{
		int toInsert = generate();

		if (i == middle)
		{
			middleKey = toInsert;
		}

		if (i > middle - 5 && i <= middle + 5001)
		{
			keysToRemove.push_back(toInsert);
		}
		else if (i > middle + 7001 && i <= middle + 15000)
		{
			keysToSearch.push_back(toInsert);
		}

		tree->insert(toInsert, "1");
	}

	auto t1_insert = std::chrono::steady_clock::now();

	auto duration_insert = std::chrono::duration_cast<std::chrono::milliseconds>(t1_insert - t0_insert).count();

	std::cout << "insert-time: " << duration_insert << std::endl;
	std::cout << "size: " << tree->size() << std::endl;

	for (int i = 0; i < 2000; ++i)
	{
		bogusSearch.push_back(generate());
	}

	keysToSearch.insert(keysToSearch.end(), bogusSearch.begin(), bogusSearch.end());

	auto t0_search = std::chrono::steady_clock::now();

	for (int key : keysToSearch)
	{
		std::string *result = tree->search(key);
	}

	auto t1_search = std::chrono::steady_clock::now();

	auto duration_search = std::chrono::duration_cast<std::chrono::milliseconds>(t1_search - t0_search).count();

	std::cout << "search-time: " << duration_search << std::endl;
	std::size_t failedToRemove = 0;

	auto t0_remove = std::chrono::steady_clock::now();

	for (int keyToRemove : keysToRemove)
	{
		const bool result = tree->remove(keyToRemove);

		if (!result)
		{
			// std::cout << "remove failed: " << keyToRemove << std::endl;
			failedToRemove++;
		}
	}

	auto t1_remove = std::chrono::steady_clock::now();

	auto duration_remove = std::chrono::duration_cast<std::chrono::milliseconds>(t1_remove - t0_remove).count();

	std::cout << "remove-time: " << duration_remove << std::endl;
	std::cout << "final size: " << tree->size() << std::endl;
	std::cout << "failed removals: " << failedToRemove << std::endl;

	auto t0_walk = std::chrono::steady_clock::now();

	for (auto &&[key, value] : *tree)
	{
		if (key % 23399 == 0)
		{
			// std::cout << "walk\t" << key << ": " << value << std::endl;
		}
	}

	auto t1_walk = std::chrono::steady_clock::now();

	auto duration_walk = std::chrono::duration_cast<std::chrono::milliseconds>(t1_walk - t0_walk).count();

	std::cout << "walk-time: " << duration_walk << std::endl;

	auto t0_range = std::chrono::steady_clock::now();

	auto range = tree->range(middleKey, std::size_t{10});
	auto [firstResultKey, firstResultValue] = range[0];
	auto [lastResultKey, lastResultValue] = range[range.size() - 1];

	for (auto [key, value] : range)
	{
		// std::cout << "range: " << *key << ": " << *value << std::endl;

		*value = std::string("hello");

		// std::cout << "modified: " << *key << ": " << *value << std::endl;
	}

	std::cout << "Range: change-value: " << *firstResultKey << ": " << *firstResultValue << std::endl;

	auto t1_range = std::chrono::steady_clock::now();

	auto duration_range = std::chrono::duration_cast<std::chrono::milliseconds>(t1_range - t0_range).count();

	std::cout << "range-time: " << duration_range << std::endl;

	auto range2 = tree->range(*firstResultKey, *lastResultKey);

	std::cout << "range2-size: " << range2.size() << std::endl;

	std::cout << "Range2: get the 5th " << *range2[5].second << std::endl;

	(*tree)[*firstResultKey] = std::string("AALLOOOOO");

	std::cout << "X2 sa moar copii mei valoarea lu cristos " << *firstResultValue << std::endl;

	try
	{
		(*tree)[123] = std::string("AALLOOOOO");
	}
	catch (std::exception &e)
	{
		std::cout << "exception: " << e.what() << std::endl;
	}
}

int main() {
	auto tree = std::make_unique<BTree<int, std::string>>();

	standardTests(tree.get());

	// jsonSerializationTests(tree.get());

	return 0;
}
