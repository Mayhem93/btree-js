#include "btree.h"
#include <iostream>
#include <random>
#include <chrono>
#include <string>

using namespace std;

const int nodeCapacity = 5;

int main() {
	auto tree = std::make_unique<BTree<int, std::string>>();
	const int insertions = 1000000;
	const int middle = insertions / 2;
	const int last = insertions - 1;

	unsigned seed = chrono::system_clock::now().time_since_epoch().count();
	mt19937 generate(seed);

	int firstKey, middleKey, lastKey;
	std::vector<int> keysToRemove;
	std::vector<int> keysToSearch;
	std::vector<int> bogusSearch;

	auto t0_insert = std::chrono::steady_clock::now();

	for (int i = 0; i < insertions; ++i) {
		int toInsert = generate();

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
	std::cout << "size: "<< tree->size() << std::endl;

	for (int i = 0; i < 2000; ++i) {
		bogusSearch.push_back(generate());
	}

	keysToSearch.insert(keysToSearch.end(), bogusSearch.begin(), bogusSearch.end());

	auto t0_search = std::chrono::steady_clock::now();

	for (int key : keysToSearch)
	{
		std::string* result = tree->search(key);
	}

	auto t1_search = std::chrono::steady_clock::now();

	auto duration_search = std::chrono::duration_cast<std::chrono::milliseconds>(t1_search - t0_search).count();

	std::cout << "search-time: " << duration_search << std::endl;
	std::size_t failedToRemove = 0;

	auto t0_remove = std::chrono::steady_clock::now();

	for (int keyToRemove : keysToRemove)
	{
		const bool result = tree->remove(keyToRemove);

		if (!result) {
			// std::cout << "remove failed: " << keyToRemove << std::endl;
			failedToRemove++;
		}
	}

	auto t1_remove = std::chrono::steady_clock::now();

	auto duration_remove = std::chrono::duration_cast<std::chrono::milliseconds>(t1_remove - t0_remove).count();

	std::cout << "remove-time: " << duration_remove << std::endl;
	std::cout << "final size: " << tree->size() << std::endl;
	std::cout << "failed removals: " << failedToRemove << std::endl;

	/* auto t0_walk = std::chrono::steady_clock::now();

	for (auto& kv : *tree) {
		const auto& key = kv.first;
		auto & value = kv.second;

		if (key % 23399 == 0)
		{
			std::cout << key << ": " << value << std::endl;
		}
	}

	auto t1_walk = std::chrono::steady_clock::now();

	auto duration_walk = std::chrono::duration_cast<std::chrono::milliseconds>(t1_walk - t0_walk).count();

	std::cout << "walk-time: " << duration_walk << std::endl; */

	/* 	result = tree->search(middleKey);

		if (result != nullptr)
		{
			std::cout << result->c_str() << std::endl;
		} else {
			std::cout << "not found" << std::endl;
		}
	 */
	return 0;
}
