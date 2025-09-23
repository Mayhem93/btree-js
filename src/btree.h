#pragma once

#include <vector>
#include <cstddef>
#include <functional>
#include <string>
#include <cstring>
#include <type_traits>
#include "boost/container/small_vector.hpp"

template <typename Vec>
inline void trivial_insert(Vec &vec, size_t index, typename Vec::value_type const &value);

template <typename Vec>
inline void trivial_append_range(Vec &dst, size_t dstStart, typename Vec::value_type const *src, size_t count);

template <typename Vec>
inline void trivial_erase(Vec &vec, size_t index);

/**
 * @class BTree
 * @brief A templated B-Tree container for sorted key/value storage.
 *
 * Maintains balance by splitting and merging nodes as elements are inserted
 * or removed, allowing efficient logarithmic-time operations.
 *
 * @tparam Key     Type of the keys stored in the tree.
 * @tparam Value   Type of the values associated with each key.
 * @tparam Compare Functor used to order keys; defaults to `std::less<Key>`.
 */
template <typename Key, typename Value, typename Compare = std::less<Key>>
class BTree
{
	public:
		/**
		 * @brief The maximum number of entries per node.
		*/
		static constexpr size_t s_CAPACITY = 32;
		static constexpr size_t s_MAX_KEYS = 2 * s_CAPACITY - 1;
		static constexpr size_t s_MAX_CHILDREN = 2 * s_CAPACITY;

		/**
		 * @brief Constructs an empty B-Tree with a custom comparator.
		 *
		 * @param comp  A callable object that returns true if a < b.
		 * 		Defaults to `std::less<Key>`
		 */
		explicit BTree(const Compare& comp = Compare{});

		struct Node;

		struct InternalNode
		{
			boost::container::small_vector<Key, s_MAX_KEYS> keys;
			boost::container::small_vector<Node* , s_MAX_CHILDREN> children;

			~InternalNode() = default;
		};

		struct LeafNode
		{
			boost::container::small_vector<std::pair<Key, Value>, s_MAX_KEYS> entries;

			~LeafNode() = default;
		};

		/**
		 * @struct Node
		 * @brief Represents a single node in the B-Tree.
		 *
		 * Holds up to `BTree::s_MAX_KEYS` entries in a leaf.
		 */
		struct Node
		{
			union
			{
				InternalNode internal;
				LeafNode leaf;
			};

			bool isLeaf;
			Node* nextLeaf;
			Node* prevLeaf;

			/**
			 * @brief Constructs a node.
			 *
			 * @param leaf  If true, this node will act as a leaf; otherwise as an internal node.
			*/
			explicit Node(bool leaf);
			~Node();
		};

		/**
		 * @brief Accesses the value associated with a key. This does a search behind the scenes
		 * 	so don't use it for repeated access, instead loop over the iterator and do your custom
		 * 	logic there.
		 *
		 * If the key exists, returns a reference to its value. Otherwise,
		 * an out of range exception will be thrown
		 *
		 * @param key  The key to locate or insert.
		 * @return Reference to the mapped value.
		*/
		Value& operator[](const Key& key);

		/**
		 * @brief Returns the number of key/value pairs stored in the tree.
		 *
		 * This size of the B+Tree is updated during insertions and deletions and thus not computed.
		 *
		 * @return The number of entries in the tree.
		*/
		size_t size() const;

		/**
		 * @brief Inserts a key/value pair into the tree.
		 *
		 * If the key does not already exist, a new node is created.
		 * If the key exists, the insertion is skipped.
		 *
		 * @param key     The key to insert.
		 * @param value   The value to associate with the key.
		 * @return true if the pair was inserted; false if the key was already present.
		*/
		bool insert(const Key &key, const Value &value);

		/**
		 * @brief Searches for the value associated with a given key.
		 *
		 * Traverses the tree to locate the node matching key.
		 *
		 * @param key   The key to look up.
		 * @return Pointer to the stored value if found; nullptr if the key is not in the tree.
		*/
		Value* search(const Key &key) const;

		/**
		 * @brief Removes the entry with the specified key.
		 *
		 * Finds the node matching key, removes it, and rebalances the tree.
		 *
		 * @param key   The key of the entry to remove.
		 * @return true if an element was removed; false if the key was not found.
		*/
		bool remove(const Key &key);

		/**
		 * @brief Collects all entries whose keys are within [low, high], inclusive.
		 *
		 * Performs an in-order traversal to gather matching entries in ascending key order.
		 *
		 * @param low   The lower bound key (inclusive).
		 * @param high  The upper bound key (inclusive).
		 * @return A vector of (key pointer, value pointer) pairs for matching entries.
		*/
		std::vector<std::pair<const Key*, Value*>> range(const Key &low, const Key &high);

		/**
		 * @brief Collects up to `count` entries starting at key ≥ low.
		 *
		 * Traverses the tree in order beginning at the first entry ≥ low,
		 * gathering at most `count` results.
		 *
		 * @param low    The starting key (inclusive).
		 * @param count  Maximum number of entries to return.
		 * @return A vector of (key pointer, value pointer) pairs for the first `count` entries ≥ low.
		*/
		std::vector<std::pair<const Key*, Value*>> range(const Key &low, size_t count);

#ifdef BTREE_ENABLE_JSON
		std::string serializeToJson() const;
#else
		std::string serializeToJson() const { return std::string{}; }
#endif
		class Iterator
		{
			public:
				using difference_type = std::ptrdiff_t;
				using iterator_category = std::bidirectional_iterator_tag;

				Iterator() noexcept;
				std::pair<const Key&, Value&> operator*() const;
				Iterator& operator++ ();
				Iterator operator++ (int);
				Iterator& operator-- ();
				Iterator operator-- (int);
				bool operator== (Iterator const &o) const;
				bool operator!= (Iterator const &o) const;

			private:
				BTree* m_Tree;
				Node* m_CurrentNode;
				size_t m_CurrentIndex;

				Iterator(BTree *tree, Node *node, size_t index) noexcept;

			friend class BTree;
		};

		/**
		 * @brief Returns an iterator to the first (smallest) element.
		 *
		 * Starts a forward in-order traversal of the B+Tree,
		 * yielding entries from low keys to high.
		 *
		 * @return Iterator pointing at the minimum key.
		 */
		Iterator begin();

		/**
		 * @brief Returns an iterator one past the last element.
		 *
		 * @return End iterator.
		*/
		Iterator end() noexcept;

		/**
		 * @brief Returns a reverse iterator to the last (largest) element.
		 *
		 * Starts a reverse in-order traversal of the B+Tree,
		 * yielding entries from high keys down to low.
		 *
		 * @return A std::reverse_iterator over Iterator, initially pointing at the maximum key.
		*/
		std::reverse_iterator<Iterator> rbegin();

		/**
		 * @brief Returns a reverse iterator one past the first (smallest) element.
		 *
		 * This marks the end of a reverse in-order traversal.
		 *
		 * @return A std::reverse_iterator over Iterator at the rend position.
		 */
		std::reverse_iterator<Iterator> rend() noexcept;

		/**
		 * @brief Destroys the B-Tree, freeing all internal nodes.
		 *
		*/
		~BTree();

	private:
		Node* m_Root;
		Compare m_Comp;
		size_t m_Size{0};

		/**
		 * Divides a full child node into two siblings by moving the upper half of its
		 * entries into a new node, promotes the median key into the parent, and links
		 * the new sibling so the tree remains balanced.
		 *
		 * @param parent The parent of the node to split.
		 * @param index The index in the node where to to split.
		*/
		void splitChild(Node* parent, size_t index);

		/**
		 * Inserts a key–value pair into a node that is guaranteed not to be full.
		 *
		 * This routine handles both leaf and internal nodes:
		 *  - If `node` is a leaf, it finds the correct position, updates an existing
		 *    entry if the key already exists, or inserts a new one.
		 *  - If `node` is internal, it locates the child slot, pre-splits the child
		 *    if it’s full, and then recurses into that child.
		 *
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
		 * @param leaf      Pointer to the leaf node that needs rebalancing.
		 * @param parent    Pointer to the parent node containing the separator key
		 *                  and references to its children.
		 * @param index     The index in `parent->children` where `leaf` is located.
		*/
		void rebalanceLeaf(Node* leaf, Node* parent, size_t index);

		/**
		 * Rebalances an internal node that has fallen below the minimum key threshold.
		 *
		 * This method restores an internal node’s capacity by first attempting to
		 * borrow a key–child pair from its immediate left or right sibling. If neither
		 * sibling has extra keys, it merges the underfull node with one sibling,
		 * pulls down the separator key from the parent, and updates the parent’s keys
		 * and child pointers accordingly.
		 *
		 * @param node      Pointer to the internal node that needs rebalancing.
		 * @param parent    Pointer to the parent node containing separator keys
		 *                  and references to its children.
		 * @param index     The index in `parent->children` where `node` is located.
		*/
		void rebalanceInternal(Node* node, Node* parent, size_t index);

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
		 * @param  node     Pointer to the internal node whose predecessor is sought.
		 * @param  idx      Child index in `node->children` from which to start.
		 * @return          The largest key in the subtree rooted at `node->children[idx]`.
		*/
		Key getPredecessor(Node *node, size_t idx) const;

		/**
		 * Finds the in-order successor of the key at the given child index.
		 *
		 * Starting from the child pointer at `node->children[idx + 1]`, this method
		 * descends down to the leftmost leaf in that subtree and returns its
		 * first entry’s key. This key serves as the successor for
		 * `node->keys[idx]` when replacing or deleting internal-node keys.
		 *
		 * @param  node     Pointer to the internal node whose successor is sought.
		 * @param  idx      Child index in `node->children` preceding the target subtree.
		 * @return          The smallest key in the subtree rooted at `node->children[idx + 1]`.
		*/
		Key getSuccessor(Node *node, size_t idx) const;

		/**
		 * Ensures that the child at index `idx` has at least the minimum number of
		 * keys or entries by borrowing from a sibling or merging when underflow occurs.
		 *
		 * If the immediate left or right sibling of the underflowing child has enough
		 * entries (or keys for internal nodes), this method borrows one and updates
		 * the parent’s separator key. If neither sibling can lend, it merges the child
		 * with an adjacent sibling and adjusts the parent’s keys and child pointers.
		 *
		 * @param  node     Pointer to the parent node whose child may underflow.
		 * @param  idx      The index in `node->children` of the child to fill.
		*/
		void fill(Node *node, size_t idx);

		/**
		 * Borrows one key–child pair (internal nodes) or one entry (leaf nodes)
		 * from the immediate left sibling of the child at index `idx`, updating
		 * the parent’s separator key to restore B-Tree invariants.
		 *
		 * @param  node     Pointer to the parent node whose child at `idx` is underfull.
		 * @param  idx      Index in `node->children` of the underfull child to which the left sibling will lend.
		*/
		void borrowFromPrev(Node *node, size_t idx);

		/**
		 * Borrows one key–child pair (internal nodes) or one entry (leaf nodes)
		 * from the immediate right sibling of the child at index `idx`, updating
		 * the parent’s separator key to restore B-Tree invariants.
		 *
		 * @param  node     Pointer to the parent node whose child at `idx` is underfull.
		 * @param  idx      Index in `node->children` of the underfull child to which the right sibling will lend.
		*/
		void borrowFromNext(Node *node, size_t idx);

		/**
		 * Merges the child at index `idx` with its adjacent sibling and removes the sibling.
		 *
		 * For leaf nodes, this concatenates both nodes’ entries and stitches the leaf‐level
		 * "linked list". For internal nodes, it pulls down the parent’s separator key,
		 * appends the right sibling’s keys and children to the left child, and then
		 * removes the right sibling pointer and its key from the parent.
		 *
		 * @param  node     Pointer to the parent node containing the two siblings.
		 * @param  idx      Index in `node->children` of the left child to merge; its
		 *                  right sibling lives at `idx + 1`.
		*/
		void mergeNodes(Node *node, size_t idx);

		/**
		 * Recursively deletes the subtree rooted at `node`, freeing all allocated nodes.
		 *
		 * This routine traverses each child pointer, invokes itself on that child,
		 * then deletes `node` itself. It is called by the destructor to teardown the
		 * entire tree.
		 *
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
