#pragma once

#include <vector>
#include <iterator>

#include "btree.h"

template<typename Key, typename Value, typename Compare>
BTree<Key, Value, Compare>::Node::Node(bool leaf, std::size_t capacity) : isLeaf(leaf)
{
	keys.reserve(2*capacity - 1);
	values.reserve(2*capacity - 1);
	children.reserve(2*capacity);
}

template<typename Key, typename Value, typename Compare>
BTree<Key, Value, Compare>::BTree(std::size_t capacity, Compare comp) : _capacity(capacity), _comp(comp), _size(0)
{
	_root = new Node(true, capacity);
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
	Node* fullChild = parent->children[index];
	Node* newChild = new Node(fullChild->isLeaf, _capacity);

	std::size_t median = _capacity - 1;

	Key medianKey = fullChild->keys[median];
	Value medianValue = fullChild->values[median];

	for (std::size_t i = median + 1; i < fullChild->keys.size(); ++i) {
		newChild->keys.push_back(std::move(fullChild->keys[i]));
		newChild->values.push_back(std::move(fullChild->values[i]));
	}

	if (!fullChild->isLeaf) {
		for (std::size_t i = _capacity; i < fullChild->children.size(); ++i) {
			newChild->children.push_back(fullChild->children[i]);
		}
	}

	fullChild->keys.resize(median);
	fullChild->values.resize(median);

	if (!fullChild->isLeaf) {
		fullChild->children.resize(_capacity);
	}

	parent->children.insert(parent->children.begin() + index + 1, newChild);
	parent->keys.insert(parent->keys.begin() + index, std::move(medianKey));
	parent->values.insert(parent->values.begin() + index, std::move(medianValue));
}

template <typename Key, typename Value, typename Compare>
bool BTree<Key, Value, Compare>::insertNonFull(Node* node, const Key& key, const Value& value)
{
	int i = static_cast<int>(node->keys.size()) - 1;
	while (i >= 0 && less(key, node->keys[i])) {
		--i;
	}

	std::size_t ci = static_cast<std::size_t>(i + 1);

	if (node->isLeaf)
	{
		if (ci > 0 &&
			!less(key, node->keys[ci - 1]) &&
			!less(node->keys[ci - 1], key))
		{
			node->values[ci - 1] = value;

			return false;
		}

		if (ci < node->keys.size() &&
			!less(key, node->keys[ci]) &&
			!less(node->keys[ci], key))
		{
			node->values[ci] = value;

			return false;
		}

		node->keys.insert(node->keys.begin() + ci, key);
		node->values.insert(node->values.begin() + ci, value);

		return true;
	}

	if (node->children[ci]->keys.size() == 2 * _capacity - 1) {
		splitChild(node, ci);

		if (less(node->keys[ci], key)) {
			++ci;
		}
	}

	return insertNonFull(node->children[ci], key, value);
}

template <typename Key, typename Value, typename Compare>
bool BTree<Key, Value, Compare>::insert(const Key& key, const Value& value)
{
	if (_root->keys.size() == 2 * _capacity - 1) {
		Node* oldRoot = _root;
		Node* newRoot = new Node(false, _capacity);

		newRoot->children.push_back(oldRoot);
		splitChild(newRoot, 0);
		_root = newRoot;
	}

	bool isNew = insertNonFull(_root, key, value);

	if (isNew) {
		++_size;
	}

	return isNew;
}

template <typename Key, typename Value, typename Compare>
Value* BTree<Key, Value, Compare>::search(const Key& key) const
{
	return searchNode(_root, key);
}

template <typename Key, typename Value, typename Compare>
Value* BTree<Key, Value, Compare>::searchNode(Node* node, const Key& key) const
{
	std::size_t i = 0;

	while (i < node->keys.size() && less(node->keys[i], key)) {
		++i;
	}

	if (i < node->keys.size()
		&& !less(key, node->keys[i])
		&& !less(node->keys[i], key)) {

		return &node->values[i];
	}

	if (node->isLeaf) {
		return nullptr;
	}

	return searchNode(node->children[i], key);
}

template <typename Key, typename Value, typename Compare>
bool BTree<Key, Value, Compare>::remove(const Key& key)
{
	if (!_root) {
		return false;
	}

	bool removed = removeNode(_root, key);

	if (_root->keys.empty() && !_root->isLeaf) {
		Node* oldRoot = _root;
		_root = oldRoot->children[0];

		delete oldRoot;
	}

	if (removed) {
		--_size;
	}

	return removed;
}

template <typename Key, typename Value, typename Compare>
bool BTree<Key, Value, Compare>::removeNode(Node* node, const Key& key)
{
	std::size_t index = findKey(node, key);

	if (index < node->keys.size()
		&& !less(key, node->keys[index])
		&& !less(node->keys[index], key)) {

		if (node->isLeaf) {
			node->keys.erase(node->keys.begin() + index);
			node->values.erase(node->values.begin() + index);
		} else {
			removeFromNonLeaf(node, index);
		}

		return true;
	}

	if (node->isLeaf) {
		return false;
	}

	Node* child = node->children[index];

	if (child->keys.size() < _capacity) {
		fill(node, index);

		if (index > node->keys.size()) {
			--index;
		}
	}

	return removeNode(node->children[index], key);
}

template <typename Key, typename Value, typename Compare>
std::size_t BTree<Key, Value, Compare>::findKey(Node* node, const Key& key) const
{
	std::size_t index = 0;

	while (index < node->keys.size() && less(node->keys[index], key)) {
		++index;
	}

	return index;
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::removeFromNonLeaf(Node* node, std::size_t index)
{
	Key k = node->keys[index];
	Node* prevChild = node->children[index];
	Node* nextChild = node->children[index + 1];

	if (prevChild->keys.size() >= _capacity) {
		Node* current = prevChild;

		while (!current->isLeaf) {
			current = current->children.back();
		}

		Key prevKey = current->keys.back();
		Value prevValue = current->values.back();

		node->keys[index] = prevKey;
		node->values[index] = prevValue;
		removeNode(prevChild, prevKey);
	} else if (nextChild->keys.size() >= _capacity) {
		Node* current = nextChild;

		while (!current->isLeaf) {
			current = current->children.front();
		}

		Key nextKey = current->keys.front();
		Value nextValue = current->values.front();

		node->keys[index] = nextKey;
		node->values[index] = nextValue;
		removeNode(nextChild, nextKey);
	} else {
		merge(node, index);
		removeNode(prevChild, k);
	}
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::fill(Node* node, std::size_t index)
{
	if (index != 0 && node->children[index - 1]->keys.size() >= _capacity) {
		borrowFromPrev(node, index);
	} else if (index != node->keys.size() && node->children[index + 1]->keys.size() >= _capacity) {
		borrowFromNext(node, index);
	} else {
		if (index != node->keys.size()) {
			merge(node, index);
		} else {
			merge(node, index - 1);
		}
	}
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::borrowFromPrev(Node* node, std::size_t index)
{
	Node* child = node->children[index];
	Node* sibling = node->children[index - 1];

	child->keys.insert(child->keys.begin(), node->keys[index - 1]);
	child->values.insert(child->values.begin(), node->values[index - 1]);

	if (!sibling->isLeaf) {
		child->children.insert(child->children.begin(), sibling->children.back());
		sibling->children.pop_back();
	}

	node->keys[index - 1] = sibling->keys.back();
	node->values[index - 1] = sibling->values.back();

	sibling->keys.pop_back();
	sibling->values.pop_back();
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::borrowFromNext(Node* node, std::size_t index)
{
	Node* child = node->children[index];
	Node* sibling = node->children[index + 1];

	child->keys.push_back(node->keys[index]);
	child->values.push_back(node->values[index]);

	if (!sibling->isLeaf) {
		child->children.push_back(sibling->children.front());
		sibling->children.erase(sibling->children.begin());
	}

	node->keys[index] = sibling->keys.front();
	node->values[index] = sibling->values.front();

	sibling->keys.erase(sibling->keys.begin());
	sibling->values.erase(sibling->values.begin());
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::merge(Node* node, std::size_t index)
{
	Node* leftChild = node->children[index];
	Node* rightChild = node->children[index + 1];

	leftChild->keys.push_back(node->keys[index]);
	leftChild->values.push_back(node->values[index]);

	for (std::size_t i = 0; i < rightChild->keys.size(); ++i) {
		leftChild->keys.push_back(rightChild->keys[i]);
		leftChild->values.push_back(rightChild->values[i]);
	}

	if (!leftChild->isLeaf) {
		for (auto c : rightChild->children) {
			leftChild->children.push_back(c);
		}
	}

	node->keys.erase(node->keys.begin() + index);
	node->values.erase(node->values.begin() + index);
	node->children.erase(node->children.begin() + index + 1);

	delete rightChild;
}

template <typename Key, typename Value, typename Compare>
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
