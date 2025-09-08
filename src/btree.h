#pragma once

#include <vector>
#include <cstddef>
#include <functional>
#include "libusb-1.0/libusb.h"
#include "boost/container/small_vector.hpp"

template <typename Key, typename Value, typename Compare = std::less<Key>>
class BTree
{
	public:
		struct Node
		{
			bool isLeaf;
			std::vector<Key> keys;
			std::vector<Value> values;
			std::vector<Node*> children;
			explicit Node(bool leaf, std::size_t capacity) : isLeaf(leaf);
		};
		class Iterator;
		Iterator begin();
		Iterator end() noexcept;

		explicit BTree(std::size_t capacity, Compare comp = Compare());

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

		Value* searchNode(Node* node, const Key& key) const;

		bool removeNode(Node* node, const Key& key);

		std::size_t findKey(Node* node, const Key& key) const;

		void removeFromNonLeaf(Node* node, std::size_t index);

		void fill(Node* node, std::size_t index);

		void borrowFromPrev(Node* node, std::size_t index);

		void borrowFromNext(Node* node, std::size_t index);

		void merge(Node* node, std::size_t index);

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
