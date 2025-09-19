#pragma once

#include <vector>
#include <iterator>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <format>

#include "btree.h"

#ifdef BTREE_ENABLE_JSON
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <functional>
#endif

template<typename Key, typename Value, typename Compare>
BTree<Key, Value, Compare>::Node::Node(bool leaf)
	: isLeaf(leaf),
	nextLeaf(nullptr),
	prevLeaf(nullptr)
{
	if (!isLeaf) {
		keys.reserve(2 * BTree::s_CAPACITY - 1);
		children.reserve(2 * BTree::s_CAPACITY);
	}

	entries.reserve(2 * BTree::s_CAPACITY - 1);
 }

template<typename Key, typename Value, typename Compare>
BTree<Key, Value, Compare>::BTree(Compare comp) : m_Comp(std::move(comp)), m_Size(0)
{
	m_Root = new Node(true);
}

template<typename Key, typename Value, typename Compare>
BTree<Key, Value, Compare>::~BTree()
{
	destroyNode(m_Root);
}

template <typename Key, typename Value, typename Compare>
Value& BTree<Key, Value, Compare>::operator[](const Key &key)
{
	if (Value* p = search(key)) {
		return	*p;
	}

	throw std::out_of_range(std::format("BTree[] lookup failed: key {} not found", key));
}

template <typename Key, typename Value, typename Compare>
size_t BTree<Key, Value, Compare>::size() const
{
	return m_Size;
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
void BTree<Key, Value, Compare>::splitChild(Node* parent, size_t index)
{
	Node* child = parent->children[index];
	Node* sibling = new Node(child->isLeaf);

	if (child->isLeaf) {
		auto midIt = child->entries.begin() + BTree::s_CAPACITY;

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
		size_t mid = BTree::s_CAPACITY - 1;
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

	size_t i = 0;

	while (i < node->keys.size() && less(node->keys[i], key)) {
		++i;
	}

	Node* child = node->children[i];

	bool childFull = child->isLeaf ? (child->entries.size() >= 2 * BTree::s_CAPACITY - 1) : (child->keys.size() >= 2 * BTree::s_CAPACITY - 1);

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
	bool rootFull = m_Root->isLeaf ? (m_Root->entries.size() >= 2 * BTree::s_CAPACITY - 1) : (m_Root->keys.size() >= 2 * BTree::s_CAPACITY - 1);

	if (rootFull) {
		Node* oldRoot = m_Root;
		Node* newRoot = new Node(false);

		newRoot->children.push_back(oldRoot);
		splitChild(newRoot, 0);
		m_Root = newRoot;
	}

	bool inserted = insertNonFull(m_Root, key, value);

	if (inserted) {
		++m_Size;
	}

	return inserted;
}

template <typename Key, typename Value, typename Compare>
Value* BTree<Key, Value, Compare>::search(const Key& key) const
{
	Node* node = m_Root;

	while (!node->isLeaf) {
		auto it = std::lower_bound(
			node->keys.begin(),
			node->keys.end(),
			key,
			m_Comp
		);

		size_t idx = std::distance(node->keys.begin(), it);
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
	if (!m_Root)
		return false;

	removeFromNode(m_Root, key);

	--m_Size;

	// if root is internal and now empty, delete it
	if (!m_Root->isLeaf && m_Root->keys.empty()) {
		Node *old = m_Root;

		m_Root = m_Root->children.front();

		delete old;
	}

	return true;
}

template <typename Key, typename Value, typename Compare>
auto BTree<Key, Value, Compare>::getPredecessor(Node *node, size_t idx) const -> Key
{
	Node *cur = node->children[idx];

	while (!cur->isLeaf) {
		cur = cur->children.back();
	}

	return cur->entries.back().first;
}

template <typename Key, typename Value, typename Compare>
auto BTree<Key, Value, Compare>::getSuccessor(Node *node, size_t idx) const -> Key
{
	Node *cur = node->children[idx + 1];

	while (!cur->isLeaf) {
		cur = cur->children.front();
	}

	return cur->entries.front().first;
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::fill(Node *node, size_t idx)
{
	Node *child = node->children[idx];

	// try borrow from left sibling
	if (idx > 0 && ((node->children[idx - 1]->isLeaf
		? node->children[idx - 1]->entries.size()
		: node->children[idx - 1]->keys.size()) >= BTree::s_CAPACITY))
	{
		borrowFromPrev(node, idx);
	}
	// else try borrow from right sibling
	else if (idx < node->keys.size() && ((node->children[idx + 1]->isLeaf
		? node->children[idx + 1]->entries.size()
		: node->children[idx + 1]->keys.size()) >= BTree::s_CAPACITY))
	{
		borrowFromNext(node, idx);
	}
	// else merge with a sibling
	else
	{
		if (idx < node->keys.size())
			mergeNodes(node, idx);
		else
			mergeNodes(node, idx - 1);
	}
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::borrowFromPrev(Node *node, size_t idx)
{
	Node *child = node->children[idx];
	Node *left = node->children[idx - 1];

	if (child->isLeaf)
	{
		// steal one entry from left leaf
		child->entries.insert(
			child->entries.begin(),
			std::move(left->entries.back())
		);

		left->entries.pop_back();
		// update parent key
		node->keys[idx - 1] = child->entries.front().first;
	}
	else
	{
		// steal one key+child from left internal
		child->keys.insert(child->keys.begin(), node->keys[idx - 1]);
		node->keys[idx - 1] = left->keys.back();
		left->keys.pop_back();

		child->children.insert(
			child->children.begin(),
			left->children.back()
		);

		left->children.pop_back();
	}
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::borrowFromNext(Node *node, size_t idx)
{
	Node *child = node->children[idx];
	Node *right = node->children[idx + 1];

	if (child->isLeaf)
	{
		// steal one entry from right leaf
		child->entries.push_back(std::move(right->entries.front()));
		right->entries.erase(right->entries.begin());
		// update parent key
		node->keys[idx] = right->entries.front().first;
	}
	else
	{
		// steal one key+child from right internal
		child->keys.push_back(node->keys[idx]);
		node->keys[idx] = right->keys.front();
		right->keys.erase(right->keys.begin());

		child->children.push_back(right->children.front());
		right->children.erase(right->children.begin());
	}
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::mergeNodes(Node *node, size_t idx)
{
	Node *left = node->children[idx];
	Node *right = node->children[idx + 1];

	if (left->isLeaf)
	{
		// merge leaf entries
		left->entries.insert(
			left->entries.end(),
			std::make_move_iterator(right->entries.begin()),
			std::make_move_iterator(right->entries.end())
		);

		// stitch leaf list
		left->nextLeaf = right->nextLeaf;

		if (right->nextLeaf)
			right->nextLeaf->prevLeaf = left;
	}
	else
	{
		// pull down the separating key
		left->keys.push_back(node->keys[idx]);
		// append right’s keys and children
		left->keys.insert(
			left->keys.end(),
			right->keys.begin(),
			right->keys.end()
		);

		left->children.insert(
			left->children.end(),
			right->children.begin(),
			right->children.end()
		);
	}

	// remove right sibling
	node->children.erase(node->children.begin() + idx + 1);
	node->keys.erase(node->keys.begin() + idx);

	delete right;
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::removeFromNode(Node *node, const Key &key)
{
	// 1) Find the first index ≥ key
	size_t idx = 0;
	while (idx < node->keys.size() && less(node->keys[idx], key))
		++idx;

	// 2) Case A: key is in this node (and node is leaf or internal)
	if (idx < node->keys.size() && !less(key, node->keys[idx]) && !less(node->keys[idx], key))
	{
		if (node->isLeaf)
		{
			// A1) Leaf: erase the entry
			auto eit = std::find_if(
				node->entries.begin(),
				node->entries.end(),
				[&](auto const &p)
				{ return !less(p.first, key) && !less(key, p.first); });

			if (eit != node->entries.end())
			{
				node->entries.erase(eit);
			}
		}
		else
		{
			// A2) Internal: three subcases
			Node *leftChild = node->children[idx];
			Node *rightChild = node->children[idx + 1];
			auto leftCount = leftChild->isLeaf ? leftChild->entries.size()
											   : leftChild->keys.size();
			auto rightCount = rightChild->isLeaf ? rightChild->entries.size()
												 : rightChild->keys.size();

			if (leftCount >= BTree::s_CAPACITY)
			{
				Key pred = getPredecessor(node, idx);

				node->keys[idx] = pred;
				removeFromNode(leftChild, pred);
			}
			else if (rightCount >= BTree::s_CAPACITY)
			{
				Key succ = getSuccessor(node, idx);

				node->keys[idx] = succ;
				removeFromNode(rightChild, succ);
			}
			else
			{
				// both children have only CAPACITY-1 keys → merge
				mergeNodes(node, idx);
				removeFromNode(leftChild, key);
			}
		}
	}
	// 3) Case B: key not in this node
	else
	{
		if (node->isLeaf)
		{
			return;
		}

		bool lastChild = (idx == node->keys.size());
		Node *child = node->children[idx];
		auto childCount = child->isLeaf ? child->entries.size()
										: child->keys.size();

		// ensure child has at least CAPACITY keys
		if (childCount < BTree::s_CAPACITY)
		{
			fill(node, idx);
		}

		// after fill(), decide correct subtree
		if (lastChild && idx > node->keys.size())
			removeFromNode(node->children[idx - 1], key);
		else
			removeFromNode(node->children[idx], key);
	}
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::rebalanceLeaf(Node* leaf, Node* parent, size_t index) {
	const size_t minEntries = BTree::s_CAPACITY - 1;
	Node* left = index > 0 ? parent->children[index - 1] : nullptr;
	Node* right = index + 1 < parent->children.size() ? parent->children[index + 1] : nullptr;

	if (left && left->entries.size() > minEntries) {
		auto kv = left->entries.back();

		left->entries.pop_back();
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
void BTree<Key, Value, Compare>::rebalanceInternal(Node* node, Node* parent, size_t index) {
	const size_t minKeys = BTree::s_CAPACITY - 1;
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

template <typename Key, typename Value, typename Compare>
std::vector<std::pair<const Key *, Value *>> BTree<Key, Value, Compare>::range(const Key &low, const Key &high)
{
	std::vector<std::pair<const Key *, Value *>> out;

	if (!m_Root)
		return out;

	// 1) Search down to the leaf that could contain `low`
	Node *n = m_Root;

	while (!n->isLeaf) {
		auto it = std::upper_bound(
			n->keys.begin(), n->keys.end(), low,
			[this](auto const &a, auto const &b) { return m_Comp(a, b); }
		);
		size_t childIdx = std::distance(n->keys.begin(), it);

		n = n->children[childIdx];
	}

	// 2) In that leaf, find the first entry >= low
	auto entryIt = std::lower_bound(
		n->entries.begin(), n->entries.end(), low,
		[this](auto const &entry, auto const &val) { return m_Comp(entry.first, val); }
	);

	size_t idx = std::distance(n->entries.begin(), entryIt);

	// 3) Collect until > high, hopping leaves as needed
	while (n) {
		while (idx < n->entries.size()) {
			auto &kv = n->entries[idx];

			if (m_Comp(high, kv.first)) // kv.first > high ?
				return out;

			out.emplace_back(&kv.first, &kv.second);
			++idx;
		}

		n = n->nextLeaf;
		idx = 0;
	}

	return out;
}

template <typename Key, typename Value, typename Compare>
std::vector<std::pair<const Key *, Value *>> BTree<Key, Value, Compare>::range(const Key &low, size_t count)
{
	std::vector<std::pair<const Key *, Value *>> out;

	if (!m_Root || count == 0)
		return out;

	// 1) Descend to leaf
	Node *n = m_Root;

	while (!n->isLeaf) {
		auto it = std::upper_bound(
			n->keys.begin(), n->keys.end(), low,
			[this](auto const &a, auto const &b) { return m_Comp(a, b); }
		);

		n = n->children[std::distance(n->keys.begin(), it)];
	}

	// 2) Find first >= low
	auto entryIt = std::lower_bound(
		n->entries.begin(), n->entries.end(), low,
		[this](auto const &entry, auto const &val) { return m_Comp(entry.first, val); }
	);

	size_t idx = std::distance(n->entries.begin(), entryIt);
	size_t taken = 0;

	// 3) Collect up to count
	while (n && taken < count) {
		if (idx < n->entries.size()) {
			auto &kv = n->entries[idx++];
			out.push_back({&kv.first, &kv.second});

			++taken;
		} else {
			n = n->nextLeaf;
			idx = 0;
		}
	}

	return out;
}

#ifdef BTREE_ENABLE_JSON

template <typename Key, typename Value, typename Compare>
std::string BTree<Key, Value, Compare>::serializeToJson() const
{
	using json = nlohmann::ordered_json;

	auto ptrToHex = [](auto *ptr)
	{
		std::ostringstream ss;
		ss << "0x"
		   << std::hex << std::uppercase
		   << reinterpret_cast<uintptr_t>(ptr);

		return ss.str();
	};

	// Helper to turn a Node* into a JSON object
	std::function<json(Node *)> dumpNode = [&](Node *node) -> json
	{
		json j;

		// id as hex pointer
		j["id"] = ptrToHex(node);

		j["isLeaf"] = node->isLeaf;

		// entries: [ [key,value], [key,value], … ]
		j["entries"] = json::array();
		for (auto const &[k, v] : node->entries)
		{
			j["entries"].push_back(json::array({k, v}));
		}

		// children: recurse or empty array for leaves
		j["children"] = json::array();

		if (!node->isLeaf)
		{
			for (Node *child : node->children)
			{
				j["children"].push_back(dumpNode(child));
			}
		} else {
			json prev = node->prevLeaf ? json(ptrToHex(node->prevLeaf)) : json(nullptr);
			json next = node->nextLeaf ? json(ptrToHex(node->nextLeaf)) : json(nullptr);

			j["prev"] = std::move(prev);
			j["next"] = std::move(next);
		}

		return j;
	};

	// top‐level wrapper
	json out;
	out["node"] = dumpNode(m_Root);

	return out.dump(2); // 2-space indent
}

#endif

template <typename Key, typename Value, typename Compare>
class BTree<Key, Value, Compare>::Iterator {
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = std::pair<const Key&, Value&>;
		using reference = value_type;
		using iterator_category = std::forward_iterator_tag;

		Iterator() noexcept : m_CurrentNode(nullptr), m_CurrentIndex(0) {}

		reference operator* () const {
			auto &kv = m_CurrentNode->entries[m_CurrentIndex];

			return { kv.first, kv.second };
		}

		Iterator& operator++ () {
			if (!m_CurrentNode) {
				return *this;
			}

			if (++m_CurrentIndex >= m_CurrentNode->entries.size()) {
				m_CurrentNode = m_CurrentNode->nextLeaf;
				m_CurrentIndex = 0;
			}

			return *this;
		}

		Iterator operator++ (int) {
			Iterator tmp = *this;

			++*this;

			return tmp;
		}

		bool operator== (Iterator const& o) const {
			return m_CurrentNode == o.m_CurrentNode && m_CurrentIndex == o.m_CurrentIndex;
		}

		bool operator!= (Iterator const& o) const {
			return !(*this == o);
		}

	private:
		Node* m_CurrentNode;
		size_t m_CurrentIndex;

		friend class BTree;
};

template <typename Key, typename Value, typename Compare>
typename BTree<Key, Value, Compare>::Iterator BTree<Key, Value, Compare>::begin()
{
	Iterator it;
	Node* n = m_Root;

	while (n && !n->isLeaf) {
		n = n->children.front();
	}

	it.m_CurrentNode = n;
	it.m_CurrentIndex = 0;

	return it;
}

template <typename Key, typename Value, typename Compare>
typename BTree<Key, Value, Compare>::Iterator BTree<Key, Value, Compare>::end() noexcept
{
	return Iterator();
}

