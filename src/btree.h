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
		static constexpr std::size_t s_CAPACITY = 32;

		explicit BTree(Compare comp = Compare{});

		struct Node
		{
			bool isLeaf;
			std::vector<Key> keys;
			std::vector<Node*> children;
			boost::container::small_vector<std::pair<Key, Value>, s_CAPACITY> entries;
			Node* nextLeaf;
			Node* prevLeaf;

			explicit Node(bool leaf);
		};

		Value& operator[](const Key& key);

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
		class Iterator;
		Iterator begin();
		Iterator end() noexcept;

		~BTree();

	private:
		Node* m_Root;
		Compare m_Comp;
		std::size_t m_Size{0};

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

		/**
		 * Removes a key from the subtree rooted at `node`, preserving B-Tree invariants.
		 *
		 * This routine handles three major scenarios:
		 * 1. If `node` is a leaf and contains `key`, it erases the entry directly.
		 * 2. If `node` is internal and contains `key`, it replaces `key` with its
		 *    predecessor or successor—or merges children when neither side can lend—
		 *    then continues removal in the affected subtree.
		 * 3. If `key` is not present in `node`, it locates the correct child slot,
		 *    ensures that child has enough entries (borrowing or merging if underflow),
		 *    and recurses into that child.
		 *
		 * @tparam Key      The type of keys stored in the tree.
		 * @tparam Value    The type of values associated with each key.
		 * @tparam Compare  Functor type used to order keys.
		 * @param  node     Pointer to the node where removal begins.
		 * @param  key      The key to remove from the tree.
		*/
		void removeFromNode(Node *node, const Key &key);

		/**
		 * Finds the in-order predecessor of the key at the given child index.
		 *
		 * Starting from the child pointer at `node->children[idx]`, this method
		 * descends down to the rightmost leaf in that subtree and returns its
		 * last entry’s key. This key serves as the predecessor for
		 * `node->keys[idx]` when replacing or deleting internal-node keys.
		 *
		 * @tparam Key      The key type stored in the B-Tree.
		 * @tparam Value    The value type stored alongside each key.
		 * @tparam Compare  Functor used to order keys in the tree.
		 * @param  node     Pointer to the internal node whose predecessor is sought.
		 * @param  idx      Child index in `node->children` from which to start.
		 * @return          The largest key in the subtree rooted at `node->children[idx]`.
		*/
		Key getPredecessor(Node *node, std::size_t idx) const;

		/**
		 * Finds the in-order successor of the key at the given child index.
		 *
		 * Starting from the child pointer at `node->children[idx + 1]`, this method
		 * descends down to the leftmost leaf in that subtree and returns its
		 * first entry’s key. This key serves as the successor for
		 * `node->keys[idx]` when replacing or deleting internal-node keys.
		 *
		 * @tparam Key      The key type stored in the B-Tree.
		 * @tparam Value    The value type stored alongside each key.
		 * @tparam Compare  Functor used to order keys in the tree.
		 * @param  node     Pointer to the internal node whose successor is sought.
		 * @param  idx      Child index in `node->children` preceding the target subtree.
		 * @return          The smallest key in the subtree rooted at `node->children[idx + 1]`.
		*/
		Key getSuccessor(Node *node, std::size_t idx) const;

		/**
		 * Ensures that the child at index `idx` has at least the minimum number of
		 * keys or entries by borrowing from a sibling or merging when underflow occurs.
		 *
		 * If the immediate left or right sibling of the underflowing child has enough
		 * entries (or keys for internal nodes), this method borrows one and updates
		 * the parent’s separator key. If neither sibling can lend, it merges the child
		 * with an adjacent sibling and adjusts the parent’s keys and child pointers.
		 *
		 * @tparam Key      The key type stored in the B-Tree.
		 * @tparam Value    The value type stored alongside each key.
		 * @tparam Compare  Comparison functor used to order keys in the tree.
		 * @param  node     Pointer to the parent node whose child may underflow.
		 * @param  idx      The index in `node->children` of the child to fill.
		*/
		void fill(Node *node, std::size_t idx);

		/**
		 * Borrows one key–child pair (internal nodes) or one entry (leaf nodes)
		 * from the immediate left sibling of the child at index `idx`, updating
		 * the parent’s separator key to restore B-Tree invariants.
		 *
		 * @tparam Key      The key type stored in the B-Tree.
		 * @tparam Value    The value type stored alongside each key.
		 * @tparam Compare  Functor used to order keys in the tree.
		 * @param  node     Pointer to the parent node whose child at `idx` is underfull.
		 * @param  idx      Index in `node->children` of the underfull child to which the left sibling will lend.
		*/
		void borrowFromPrev(Node *node, std::size_t idx);

		/**
		 * Borrows one key–child pair (internal nodes) or one entry (leaf nodes)
		 * from the immediate right sibling of the child at index `idx`, updating
		 * the parent’s separator key to restore B-Tree invariants.
		 *
		 * @tparam Key      The key type stored in the B-Tree.
		 * @tparam Value    The value type stored alongside each key.
		 * @tparam Compare  Functor used to order keys in the tree.
		 * @param  node     Pointer to the parent node whose child at `idx` is underfull.
		 * @param  idx      Index in `node->children` of the underfull child to which the right sibling will lend.
		*/
		void borrowFromNext(Node *node, std::size_t idx);

		/**
		 * Merges the child at index `idx` with its adjacent sibling and removes the sibling.
		 *
		 * For leaf nodes, this concatenates both nodes’ entries and stitches the leaf‐level
		 * "linked list". For internal nodes, it pulls down the parent’s separator key,
		 * appends the right sibling’s keys and children to the left child, and then
		 * removes the right sibling pointer and its key from the parent.
		 *
		 * @tparam Key      The key type stored in the B-Tree nodes.
		 * @tparam Value    The value type stored alongside each key.
		 * @tparam Compare  Functor used to order keys in the tree.
		 * @param  node     Pointer to the parent node containing the two siblings.
		 * @param  idx      Index in `node->children` of the left child to merge; its
		 *                  right sibling lives at `idx + 1`.
		*/
		void mergeNodes(Node *node, std::size_t idx);

		/**
		 * Recursively deletes the subtree rooted at `node`, freeing all allocated nodes.
		 *
		 * This routine traverses each child pointer, invokes itself on that child,
		 * then deletes `node` itself. It is called by the destructor to teardown the
		 * entire tree.
		 *
		 * @tparam Key      The key type stored in the B-Tree.
		 * @tparam Value    The value type stored alongside each key.
		 * @tparam Compare  Functor used to order keys in the tree.
		 * @param  node     Pointer to the root of the subtree to destroy.
		*/
		void destroyNode(Node *node);

		/**
		 * Compares two keys, wrapper function for `Compare m_Comp`
		 * @param a The first key to compare.
		 * @param b The second key to compare.
		 * @return True if a is "less" than b, false otherwise.
		 */
		inline bool less(const Key& a, const Key& b) const
		{
			return m_Comp(a, b);
		}

	friend class Iterator;
};

#include "btree.cpp"
