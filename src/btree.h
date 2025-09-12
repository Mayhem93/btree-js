#pragma once

#include <vector>
#include <cstddef>
#include <functional>
#include "boost/container/small_vector.hpp"

template <typename Key, typename Value, typename Compare = std::less<Key>>
class BTree
{
	public:
		static constexpr std::size_t CAPACITY = 5;

		struct Node
		{
			bool isLeaf;
			std::vector<Key> keys;
			// std::vector<Value> values;
			std::vector<Node*> children;
			// std::list<std::pair<Key, Value>> entries;
			boost::container::small_vector<std::pair<Key, Value>, CAPACITY> entries;
			Node* nextLeaf;
			Node* prevLeaf;

			explicit Node(bool leaf);
		};
		class Iterator;
		Iterator begin();
		Iterator end() noexcept;

		explicit BTree(Compare comp = Compare());

		std::size_t size() const;

		bool insert(const Key &key, const Value &value);

		Value* search(const Key &key) const;

		bool remove(const Key &key);

		~BTree();

	private:
		Node* _root;
		std::size_t _capacity;
		Compare _comp;
		std::size_t _size{0};

		void splitChild(Node* parent, std::size_t index);

		bool insertNonFull(Node* node, const Key& key, const Value& value);

		void rebalanceLeaf(Node* leaf, Node* parent, std::size_t index);

		void rebalanceInternal(Node* node, Node* parent, std::size_t index);

		// --- Recursive remove helpers ---
		void removeFromNode(Node *node, const Key &key);
		Key getPredecessor(Node *node, std::size_t idx) const;
		Key getSuccessor(Node *node, std::size_t idx) const;
		void fill(Node *node, std::size_t idx);
		void borrowFromPrev(Node *node, std::size_t idx);
		void borrowFromNext(Node *node, std::size_t idx);
		void mergeNodes(Node *node, std::size_t idx);

		/**
		 * @brief Recursively destroys a node and its children in the B-tree.
		 *
		 * @param node The node to destroy.
		 */
		void destroyNode(Node *node);

		inline bool less(const Key& a, const Key& b) const
		{
			return _comp(a, b);
		}

	friend class Iterator;
};

#include "btree.cpp"
