#pragma once

#include <vector>
#include <cstddef>
#include <functional>
#include "boost/container/small_vector.hpp"

template <typename Key, typename Value, typename Compare = std::less<Key>>
class BTree
{
	public:
		static constexpr std::size_t CAPACITY = 32;

		struct Node
		{
			bool isLeaf;
			std::vector<Key> keys;
			std::vector<Node*> children;
			boost::container::small_vector<std::pair<Key, Value>, CAPACITY> entries;
			Node* nextLeaf;
			Node* prevLeaf;

			explicit Node(bool leaf);
		};

		Value& operator[](const Key& key);

		class Iterator;
		Iterator begin();
		Iterator end() noexcept;

		explicit BTree(Compare comp = Compare{});

		std::size_t size() const;

		/**
		 * Insert a key-value pair into the BTree.
		 * @param key The key to insert.
		 * @param value The value to insert.
		 * @return True if the insertion was successful, false otherwise.
		 */
		bool insert(const Key &key, const Value &value);

		Value* search(const Key &key) const;

		bool remove(const Key &key);

		std::vector<std::pair<const Key*, Value*>> range(const Key &low, const Key &high);
		std::vector<std::pair<const Key*, Value*>> range(const Key &low, std::size_t count);

		~BTree();

	private:
		Node* _root;
		std::size_t _capacity;
		Compare _comp;
		std::size_t _size{0};

		/**
		 * Divides a full child node into two siblings by moving the upper half of its
		 * entries into a new node, promotes the median key into the parent, and links
		 * the new sibling so the tree remains balanced.
		 * @param parent The parent of the node to split.
		 * @param index The index in the node where to to split.
		 * @return void
		 */
		void splitChild(Node* parent, std::size_t index);

		bool insertNonFull(Node* node, const Key& key, const Value& value);

		void rebalanceLeaf(Node* leaf, Node* parent, std::size_t index);

		void rebalanceInternal(Node* node, Node* parent, std::size_t index);

		void removeFromNode(Node *node, const Key &key);
		Key getPredecessor(Node *node, std::size_t idx) const;
		Key getSuccessor(Node *node, std::size_t idx) const;
		void fill(Node *node, std::size_t idx);
		void borrowFromPrev(Node *node, std::size_t idx);
		void borrowFromNext(Node *node, std::size_t idx);
		void mergeNodes(Node *node, std::size_t idx);

		/**
		 * Recursively destroys nodes and their children
		 * @param node The starting node to delete.
		 * @return void
		 */
		void destroyNode(Node *node);

		/**
		 * Compares two keys, wrapper function for `Compare _comp`
		 * @param a The first key to compare.
		 * @param b The second key to compare.
		 * @return True if a is "less" than b, false otherwise.
		 */
		inline bool less(const Key& a, const Key& b) const
		{
			return _comp(a, b);
		}

	friend class Iterator;
};

#include "btree.cpp"
