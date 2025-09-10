#pragma once

#include <vector>
#include <iterator>
#include <algorithm>

#include "btree.h"

template<typename Key, typename Value, typename Compare>
BTree<Key, Value, Compare>::Node::Node(bool leaf)
	: isLeaf(leaf),
	nextLeaf(nullptr),
	prevLeaf(nullptr)

{
	if (!isLeaf) {
		keys.reserve(2 * CAPACITY - 1);
		children.reserve(2 * CAPACITY);
	}

	entries.reserve(2 * CAPACITY - 1);

	/* keys.reserve(2*capacity - 1);
	values.reserve(2*capacity - 1);
	children.reserve(2*capacity); */
 }

template<typename Key, typename Value, typename Compare>
BTree<Key, Value, Compare>::BTree(Compare comp) : _comp(comp), _size(0)
{
	_root = new Node(true);
}

template<typename Key, typename Value, typename Compare>
BTree<Key, Value, Compare>::~BTree()
{
	destroyNode(_root);
}

template <typename Key, typename Value, typename Compare>
std::size_t BTree<Key, Value, Compare>::size() const
{
	return _size;
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::destroyNode(Node *node)
{
	if (!node) {
		return;
	}

	for (Node* child : node->children) {
		destroyNode(child);
	}

	delete node;
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::splitChild(Node* parent, std::size_t index)
{
	Node* child = parent->children[index];
	Node* sibling = new Node(child->isLeaf);

	if (child->isLeaf) {
		/* auto midIt = child->entries.begin();

		std::advance(midIt, CAPACITY);

		sibling->entries.splice(
			sibling->entries.end(),
			child->entries,
			midIt,
			child->entries.end()
		); */
		auto midIt = child->entries.begin() + CAPACITY;

		sibling->entries.insert(
			sibling->entries.end(),
			std::make_move_iterator(midIt),
			std::make_move_iterator(child->entries.end())
		);

		child->entries.erase(midIt, child->entries.end());

		sibling->nextLeaf = child->nextLeaf;

		if (child->nextLeaf) {
			child->nextLeaf->prevLeaf = sibling;
		}

		child->nextLeaf = sibling;
		sibling->prevLeaf = child;

		Key promoteKey = sibling->entries.front().first;

		parent->keys.insert(parent->keys.begin() + index, promoteKey);
		parent->children.insert(parent->children.begin() + index + 1, sibling);
	} else {
		std::size_t mid = CAPACITY - 1;
		Key medianKey = std::move(child->keys[mid]);

		sibling->keys.insert(
			sibling->keys.end(),
			std::make_move_iterator(child->keys.begin() + mid + 1),
			std::make_move_iterator(child->keys.end())
		);

		sibling->children.insert(
			sibling->children.end(),
			std::make_move_iterator(child->children.begin() + mid + 1),
			std::make_move_iterator(child->children.end())
		);

		child->keys.erase(child->keys.begin() + mid, child->keys.end());
		child->children.erase(child->children.begin() + mid + 1, child->children.end());

		parent->keys.insert(parent->keys.begin() + index, std::move(medianKey));
		parent->children.insert(parent->children.begin() + index + 1, sibling);
	}
}

template <typename Key, typename Value, typename Compare>
bool BTree<Key, Value, Compare>::insertNonFull(Node* node, const Key& key, const Value& value)
{
	if (node->isLeaf) {
		auto it = node->entries.begin();

		while (it != node->entries.end() && less(it->first, key)) {
			++it;
		}

		if (it != node->entries.end() && !less(key, it->first) && !less(it->first, key)) {
			it->second = value;

			return false;
		}

		node->entries.insert(it, { key, value });

		return true;
	}

	std::size_t i = 0;

	while (i < node->keys.size() && less(node->keys[i], key)) {
		++i;
	}

	Node* child = node->children[i];

	bool childFull = child->isLeaf ? (child->entries.size() >= 2 * CAPACITY - 1) : (child->keys.size() >= 2 * CAPACITY - 1);

	if (childFull) {
		splitChild(node, i);

		if (less(node->keys[i], key)) {
			++i;
		}
	}

	return insertNonFull(node->children[i], key, value);
}

template <typename Key, typename Value, typename Compare>
bool BTree<Key, Value, Compare>::insert(const Key& key, const Value& value)
{
	bool rootFull = _root->isLeaf ? (_root->entries.size() >= 2 * CAPACITY - 1) : (_root->keys.size() >= 2 * CAPACITY - 1);

	if (rootFull) {
		Node* oldRoot = _root;
		Node* newRoot = new Node(false);

		newRoot->children.push_back(oldRoot);
		splitChild(newRoot, 0);
		_root = newRoot;
	}

	bool inserted = insertNonFull(_root, key, value);

	if (inserted) {
		++_size;
	}

	return inserted;
}

template <typename Key, typename Value, typename Compare>
Value* BTree<Key, Value, Compare>::search(const Key& key) const
{
	Node* node = _root;

	while (!node->isLeaf) {
		auto it = std::lower_bound(
			node->keys.begin(),
			node->keys.end(),
			key,
			_comp
		);

		std::size_t idx = std::distance(node->keys.begin(), it);
		node = node->children[idx];
	}

	for (auto& entry : node->entries) {
		if (!less(entry.first, key) && !less(key, entry.first)) {
			return &entry.second;
		}

		if (less(key, entry.first)) {
			break ;
		}
	}

	return nullptr;
}


template <typename Key, typename Value, typename Compare>
bool BTree<Key, Value, Compare>::remove(const Key& key)
{
	if (!_root) {
		return false;
	}

	std::vector<Node*> path;
	std::vector<std::size_t> indices;
	Node* node = _root;

	while (!node->isLeaf) {
		auto it = std::lower_bound(
			node->keys.begin(),
			node->keys.end(),
			key,
			_comp
		);

		std::size_t i = std::distance(node->keys.begin(), it);
		path.push_back(node);
		indices.push_back(i);
		node = node->children[i];
	}

	auto& entries = node->entries;
	auto eit = std::find_if(
		entries.begin(),
		entries.end(),
		[this, &key](auto const& p) {
			return !less(p.first, key) && !less(key, p.first);
		}
	);

	if (eit == entries.end()) {
		return false;
	}

	entries.erase(eit);
	--_size;

	const std::size_t minEntries = CAPACITY - 1;

	if (node != _root && entries.size() < minEntries) {
		Node* parent = path.back();
		std::size_t pos = indices.back();
		rebalanceLeaf(node, parent, pos);
	}

	for (int level = (int)path.size() - 1; level >= 0; --level) {
		Node* cur = path[level];
		Node* parent = level > 0 ? path[level - 1] : nullptr;
		std::size_t pos = indices[level];

		if (!cur->isLeaf && cur->keys.size() < CAPACITY - 1){
			rebalanceInternal(cur, parent, pos);

			continue;
		}

		break;
	}

	if (!_root->isLeaf && _root->children.size() == 1) {
		Node* old = _root;
		_root = _root->children.front();

		delete old;
	}

	return true;
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::rebalanceLeaf(Node* leaf, Node* parent, std::size_t index) {
	const std::size_t minEntries = CAPACITY - 1;
	Node* left = index > 0 ? parent->children[index - 1] : nullptr;
	Node* right = index + 1 < parent->children.size() ? parent->children[index + 1] : nullptr;

	if (left && left->entries.size() > minEntries) {
		auto kv = left->entries.back();

		left->entries.pop_back();
		// leaf->entries.push_front(kv);
		leaf->entries.insert(leaf->entries.begin(), std::move(kv));
		parent->keys[index - 1] = leaf->entries.front().first;

		return ;
	}

	if (right && right->entries.size() > minEntries) {
		auto kv = right->entries.front();
		// right->entries.pop_front();
		right->entries.erase(right->entries.begin());
		leaf->entries.push_back(kv);
		parent->keys[index] = right->entries.front().first;

		return;
	}

	if (left) {
		// left->entries.splice(left->entries.end(), leaf->entries);

		left->entries.insert(
			left->entries.end(),
			std::make_move_iterator(leaf->entries.begin()),
			std::make_move_iterator(leaf->entries.end())
		);

		leaf->entries.clear();

		left->nextLeaf = leaf->nextLeaf;

		if (leaf->nextLeaf) {
			leaf->nextLeaf->prevLeaf = left;
		}

		parent->children.erase(parent->children.begin() + index);
		parent->keys.erase(parent->keys.begin() + index - 1);

		delete leaf;
	} else {
		// leaf->entries.splice(leaf->entries.end(), right->entries);

		leaf->entries.insert(
			leaf->entries.end(),
			std::make_move_iterator(right->entries.begin()),
			std::make_move_iterator(right->entries.end())
		);

		right->entries.clear();

		leaf->nextLeaf = right->nextLeaf;

		if (right->nextLeaf) {
			right->nextLeaf->prevLeaf = leaf;
		}

		parent->children.erase(parent->children.begin() + index + 1);
		parent->keys.erase(parent->keys.begin() + index);

		delete right;
	}
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::rebalanceInternal(Node* node, Node* parent, std::size_t index) {
	const std::size_t minKeys = CAPACITY - 1;
	Node* left = index > 0 ? parent->children[index - 1] : nullptr;
	Node* right = index + 1 < parent->children.size() ? parent->children[index + 1] : nullptr;

	if (left && left->keys.size() > minKeys) {
		Key sep = parent->keys[index - 1];
		Node* c = left->children.back();

		left->children.pop_back();

		Key k2 = left->keys.back();

		left->keys.pop_back();

		node->children.insert(node->children.begin(), c);
		node->keys.insert(node->keys.begin(), sep);

		parent->keys[index - 1] = k2;

		return ;
	}

	if (right && right->keys.size() > minKeys) {
		Key sep = parent->keys[index];
		Node* c = right->children.front();

		right->children.erase(right->children.begin());

		Key k2 = right->keys.front();

		right->keys.erase(right->keys.begin());

		node->children.push_back(c);
		node->keys.push_back(sep);
		parent->keys[index] = k2;

		return ;
	}

	if (left) {
		Key sep = parent->keys[index - 1];

		left->keys.push_back(sep);
		left->keys.insert(left->keys.end(), node->keys.begin(), node->keys.end());
		left->children.insert(left->children.end(), node->children.begin(), node->children.end());

		parent->children.erase(parent->children.begin() + index);
		parent->keys.erase(parent->keys.begin() + index - 1);

		delete node;
	} else {
		Key sep = parent->keys[index];

		node->keys.push_back(sep);
		node->keys.insert(node->keys.end(), right->keys.begin(), right->keys.end());
		node->children.insert(node->children.end(), right->children.begin(), right->children.end());

		parent->children.erase(parent->children.begin() + index + 1);
		parent->keys.erase(parent->keys.begin() + index);

		delete right;
	}
}

/* template <typename Key, typename Value, typename Compare>
class BTree<Key, Value, Compare>::Iterator {
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = std::pair<const Key&, Value&>;
		using reference = value_type;
		using iterator_category = std::forward_iterator_tag;

		Iterator() noexcept : _currentNode(nullptr), _currentIndex(0) {}

		reference operator* () const {
			return { _currentNode->keys[_currentIndex], _currentNode->values[_currentIndex] };
		}

		Iterator& operator++ () {
			advance();

			return *this;
		}

		Iterator operator++ (int) {
			Iterator tmp = *this;
			advance();

			return tmp;
		}

		bool operator== (Iterator const& o) const {
			return _currentNode == o._currentNode && _currentIndex == o._currentIndex;
		}

		bool operator!= (Iterator const& o) const {
			return !(*this == o);
		}

	private:
		struct Frame {
			Node* node;
			std::size_t slot;
		};

		std::vector<Frame> _stack;
		Node* _currentNode;
		std::size_t _currentIndex;

		explicit Iterator(Node* root) {
			if (!root) { _currentNode = nullptr; }
			else {
				_stack.push_back({ root, 0 });
				_currentNode = nullptr;

				advance();
			}
		}

		void advance()
		{
			_currentNode = nullptr;

			while (!_stack.empty())
			{
				auto &frame = _stack.back();
				Node *node = frame.node;

				if (node->isLeaf)
				{
					if (frame.slot < node->keys.size())
					{
						_currentNode = node;
						_currentIndex = frame.slot++;
						return;
					}
					_stack.pop_back();
				}
				else
				{
					std::size_t nKeys = node->keys.size();
					std::size_t totalSlots = 2 * nKeys + 1; // child,key,child,key,...,child

					if (frame.slot < totalSlots)
					{
						// even slot → child
						if ((frame.slot & 1) == 0)
						{
							std::size_t cidx = frame.slot / 2;
							++frame.slot;
							_stack.push_back({node->children[cidx], 0});
						}
						// odd slot → key
						else
						{
							std::size_t kidx = (frame.slot - 1) / 2; // ← correct this!
							++frame.slot;
							// sanity check
							assert(kidx < nKeys);
							_currentNode = node;
							_currentIndex = kidx;
							return;
						}
					}
					else
					{
						_stack.pop_back();
					}
				}
			}

			_currentNode = nullptr; // end()
			_currentIndex = 0;
		}

		friend class BTree;
};

template <typename Key, typename Value, typename Compare>
typename BTree<Key, Value, Compare>::Iterator BTree<Key, Value, Compare>::begin()
{
	return Iterator(_root);
}

template <typename Key, typename Value, typename Compare>
typename BTree<Key, Value, Compare>::Iterator BTree<Key, Value, Compare>::end() noexcept
{
	return Iterator();
}
 */
