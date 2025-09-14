#pragma once

#include <vector>
#include <cstddef>
#include <functional>
#include "boost/container/small_vector.hpp"

#ifdef BTREE_ENABLE_JSON
#include <string>
#endif

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

#ifdef BTREE_ENABLE_JSON
		std::string serializeToJson() const;
#else
		std::string serializeToJson() const { return std::string{}; }
#endif
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
		 * @tparam Key      The key type stored in the B-Tree.
		 * @tparam Value    The value type stored alongside each key.
		 * @tparam Compare  Comparison functor used to order keys.
		 * @param parent The parent of the node to split.
		 * @param index The index in the node where to to split.
		 */
		void splitChild(Node* parent, std::size_t index);

		/**
		 * Inserts a key–value pair into a node that is guaranteed not to be full.
		 *
		 * This routine handles both leaf and internal nodes:
		 *  - If `node` is a leaf, it finds the correct position, updates an existing
		 *    entry if the key already exists, or inserts a new one.
		 *  - If `node` is internal, it locates the child slot, pre-splits the child
		 *    if it’s full, and then recurses into that child.
		 *
		 * @tparam Key      The key type stored in the B-Tree.
		 * @tparam Value    The value type stored alongside each key.
		 * @tparam Compare  Comparison functor used to order keys.
		 * @param  node     Pointer to a B-Tree node that has space for at least one more entry.
		 * @param  key      The key to insert or update.
		 * @param  value    The value to associate with `key`.
		 * @return `true` if a new entry was inserted (tree size should be incremented),
		 *         `false` if an existing entry was overwritten.
		 */
		bool insertNonFull(Node* node, const Key& key, const Value& value);

		/**
		 * Rebalances a leaf node that has fallen below the minimum entry threshold.
		 *
		 * This method restores a leaf’s capacity by first attempting to borrow an entry
		 * from its immediate left or right sibling. If neither sibling has spare entries,
		 * it merges the underfull leaf with one sibling and updates the parent’s keys
		 * and child pointers accordingly.
		 *
		 * @tparam Key      The key type stored in the B-Tree leaves.
		 * @tparam Value    The value type stored alongside each key.
		 * @tparam Compare  Comparison functor used to order keys in the tree.
		 * @param leaf      Pointer to the leaf node that needs rebalancing.
		 * @param parent    Pointer to the parent node containing the separator key
		 *                  and references to its children.
		 * @param index     The index in `parent->children` where `leaf` is located.
		 */
		void rebalanceLeaf(Node* leaf, Node* parent, std::size_t index);

		/**
		 * Rebalances an internal node that has fallen below the minimum key threshold.
		 *
		 * This method restores an internal node’s capacity by first attempting to
		 * borrow a key–child pair from its immediate left or right sibling. If neither
		 * sibling has extra keys, it merges the underfull node with one sibling,
		 * pulls down the separator key from the parent, and updates the parent’s keys
		 * and child pointers accordingly.
		 *
		 * @tparam Key      The key type stored in the B-Tree internal nodes.
		 * @tparam Value    The value type stored alongside each key in the B-Tree.
		 * @tparam Compare  The comparison functor used to order keys in the tree.
		 * @param node      Pointer to the internal node that needs rebalancing.
		 * @param parent    Pointer to the parent node containing separator keys
		 *                  and references to its children.
		 * @param index     The index in `parent->children` where `node` is located.
		 */
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
