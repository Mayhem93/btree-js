#pragma once

#include <vector>
#include <iterator>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <format>

#include <boost/container/small_vector.hpp>

#include "btree.h"

#ifdef BTREE_ENABLE_JSON
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <functional>
#endif

template <typename T, size_t N, typename ...Options>
inline void trivial_insert(boost::container::small_vector<T, N, Options...> &vec, size_t index, T const &value)
{
	if constexpr (std::is_trivially_copyable_v<T>) {
		vec.push_back(value);

		T *data = vec.data();
		std::memmove(
			data + index + 1,
			data + index,
			(vec.size() - 1 - index) * sizeof(T));

		data[index] = value;
	} else {
		vec.insert(std::next(vec.begin(), index), value);
	}
}

template <typename T, size_t N, typename... Options>
inline void trivial_append_range(boost::container::small_vector<T, N, Options...> &dst, size_t dstStart, T const *src, size_t count)
{
	if constexpr (std::is_trivially_copyable_v<T>)
	{
		// Fast path: one resize + one memmove
		if (count == 0)
			return;

		size_t oldSize = dst.size();
		dst.resize(oldSize + count); // OK for trivially-copyable T
		std::memmove(				 // copy all elements in one go
			dst.data() + dstStart,
			src,
			count * sizeof(T)
		);
	}
	else
	{
		// Fallback path for any non-trivial T:
		// This version of insert(range) will invoke your copy/move ctors,
		// never needing a default ctor.
		dst.insert(
			dst.begin() + dstStart,
			src,
			src + count
		);
	}
}

template <typename T, size_t N, typename... Options>
inline void trivial_erase(boost::container::small_vector<T, N, Options...> &vec, size_t index)
{
	if constexpr (!std::is_trivially_copyable_v<T>) {
		T* data = vec.data();

		std::memmove(
			data + index,
			data + index + 1,
			(vec.size() - index - 1) * sizeof(T)
		);

		vec.pop_back();
	} else {
		vec.erase(std::next(vec.begin(), index));
	}
}

template<typename Key, typename Value, typename Compare>
BTree<Key, Value, Compare>::Node::Node(bool leaf)
	: isLeaf(leaf),
	nextLeaf(nullptr),
	prevLeaf(nullptr)
{
	if (leaf)
	{
		new (&this->leaf) LeafNode();
	}
	else
	{
		new (&this->internal) InternalNode();
	}
}

template <typename Key, typename Value, typename Compare>
BTree<Key, Value, Compare>::Node::~Node()
{
	if (isLeaf)
	{
		this->leaf.~LeafNode();
	}
	else
	{
		this->internal.~InternalNode();
	}
}

template <typename Key, typename Value, typename Compare>
BTree<Key, Value, Compare>::BTree(const Compare &comp) : m_Comp(std::move(comp)), m_Size(0)
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
		return *p;
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
	if (!node)
		return;

	if (!node->isLeaf)
	{
		for (Node *child : node->internal.children)
		{
			destroyNode(child);
		}
	}

	delete node;
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::splitChild(Node* parent, size_t index)
{
	Node* child = parent->internal.children[index];
	Node* sibling = new Node(child->isLeaf);

	if (child->isLeaf) {
		auto midIt = child->leaf.entries.begin() + BTree::s_CAPACITY;

		sibling->leaf.entries.insert(
			sibling->leaf.entries.end(),
			std::make_move_iterator(midIt),
			std::make_move_iterator(child->leaf.entries.end())
		);

		child->leaf.entries.erase(midIt, child->leaf.entries.end());

		sibling->nextLeaf = child->nextLeaf;

		if (child->nextLeaf) {
			child->nextLeaf->prevLeaf = sibling;
		}

		child->nextLeaf = sibling;
		sibling->prevLeaf = child;

		Key promoteKey = sibling->leaf.entries.front().first;

		// parent->internal.keys.insert(parent->internal.keys.begin() + index, promoteKey);
		// parent->internal.children.insert(parent->internal.children.begin() + index + 1, sibling);

		trivial_insert(parent->internal.keys, index, promoteKey);
		trivial_insert(parent->internal.children, index + 1, sibling);
	} else {
		size_t mid = BTree::s_CAPACITY - 1;
		Key medianKey = std::move(child->internal.keys[mid]);

		/* sibling->internal.keys.insert(
			sibling->internal.keys.end(),
			std::make_move_iterator(child->internal.keys.begin() + mid + 1),
			std::make_move_iterator(child->internal.keys.end())
		); */

		if (sibling->internal.keys.size() - mid - 1 > 0)
		{
			trivial_append_range(sibling->internal.keys,
				sibling->internal.keys.size(),
				child->internal.keys.data() + mid + 1,
				child->internal.keys.size() - mid - 1
			);
		}

		/* sibling->internal.children.insert(
			sibling->internal.children.end(),
			std::make_move_iterator(child->internal.children.begin() + mid + 1),
			std::make_move_iterator(child->internal.children.end())
		); */

		if (sibling->internal.children.size() - mid - 1 > 0)
		{
			trivial_append_range(sibling->internal.children,
				sibling->internal.children.size(),
				child->internal.children.data() + mid + 1,
				child->internal.children.size() - mid - 1
			);
		}

		child->internal.keys.erase(child->internal.keys.begin() + mid, child->internal.keys.end());
		child->internal.children.erase(child->internal.children.begin() + mid + 1, child->internal.children.end());

		/* parent->internal.keys.insert(parent->internal.keys.begin() + index, std::move(medianKey));
		parent->internal.children.insert(parent->internal.children.begin() + index + 1, sibling); */

		trivial_insert(parent->internal.keys, index, std::move(medianKey));
		trivial_insert(parent->internal.children, index + 1, sibling);
	}
}

template <typename Key, typename Value, typename Compare>
bool BTree<Key, Value, Compare>::insertNonFull(Node* node, const Key& key, const Value& value)
{
	if (node->isLeaf) {
		auto it = node->leaf.entries.begin();

		while (it != node->leaf.entries.end() && less(it->first, key))
		{
			++it;
		}

		if (it != node->leaf.entries.end() && !less(key, it->first) && !less(it->first, key))
		{
			it->second = value;

			return false;
		}

		node->leaf.entries.insert(it, {key, value});

		return true;
	}

	size_t i = 0;

	while (i < node->internal.keys.size() && less(node->internal.keys[i], key))
	{
		++i;
	}

	Node *child = node->internal.children[i];

	bool childFull = child->isLeaf ? (child->leaf.entries.size() >= BTree::s_MAX_KEYS) : (child->internal.keys.size() >= BTree::s_MAX_KEYS);

	if (childFull) {
		splitChild(node, i);

		if (less(node->internal.keys[i], key))
		{
			++i;
		}
	}

	return insertNonFull(node->internal.children[i], key, value);
}

template <typename Key, typename Value, typename Compare>
bool BTree<Key, Value, Compare>::insert(const Key& key, const Value& value)
{
	bool rootFull = m_Root->isLeaf ? (m_Root->leaf.entries.size() >= BTree::s_MAX_KEYS) : (m_Root->internal.keys.size() >= BTree::s_MAX_KEYS);

	if (rootFull) {
		Node* oldRoot = m_Root;
		Node* newRoot = new Node(false);

		newRoot->internal.children.push_back(oldRoot);
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
			node->internal.keys.begin(),
			node->internal.keys.end(),
			key,
			m_Comp
		);

		size_t idx = std::distance(node->internal.keys.begin(), it);
		node = node->internal.children[idx];
	}

	for (auto& entry : node->leaf.entries) {
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
	if (!m_Root->isLeaf && m_Root->internal.keys.empty()) {
		Node *old = m_Root;

		m_Root = m_Root->internal.children.front();

		delete old;
	}

	return true;
}

template <typename Key, typename Value, typename Compare>
Key BTree<Key, Value, Compare>::getPredecessor(Node *node, size_t idx) const
{
	Node *cur = node->internal.children[idx];

	while (!cur->isLeaf) {
		cur = cur->internal.children.back();
	}

	return cur->leaf.entries.back().first;
}

template <typename Key, typename Value, typename Compare>
Key BTree<Key, Value, Compare>::getSuccessor(Node *node, size_t idx) const
{
	Node *cur = node->internal.children[idx + 1];

	while (!cur->isLeaf) {
		cur = cur->internal.children.front();
	}

	return cur->leaf.entries.front().first;
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::fill(Node *node, size_t idx)
{
	Node *child = node->internal.children[idx];

	// try borrow from left sibling
	if (idx > 0 && ((node->internal.children[idx - 1]->isLeaf
		? node->internal.children[idx - 1]->leaf.entries.size()
		: node->internal.children[idx - 1]->internal.keys.size()) >= BTree::s_CAPACITY))
	{
		borrowFromPrev(node, idx);
	}
	// else try borrow from right sibling
	else if (idx < node->internal.keys.size() && ((node->internal.children[idx + 1]->isLeaf
		? node->internal.children[idx + 1]->leaf.entries.size()
		: node->internal.children[idx + 1]->internal.keys.size()) >= BTree::s_CAPACITY))
	{
		borrowFromNext(node, idx);
	}
	// else merge with a sibling
	else
	{
		if (idx < node->internal.keys.size())
			mergeNodes(node, idx);
		else
			mergeNodes(node, idx - 1);
	}
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::borrowFromPrev(Node *node, size_t idx)
{
	Node *child = node->internal.children[idx];
	Node *left = node->internal.children[idx - 1];

	if (child->isLeaf)
	{
		// steal one entry from left leaf
		child->leaf.entries.insert(
			child->leaf.entries.begin(),
			std::move(left->leaf.entries.back())
		);

		left->leaf.entries.pop_back();
		// update parent key
		node->internal.keys[idx - 1] = child->leaf.entries.front().first;
	}
	else
	{
		// steal one key+child from left internal
		child->internal.keys.insert(child->internal.keys.begin(), node->internal.keys[idx - 1]);
		node->internal.keys[idx - 1] = left->internal.keys.back();
		left->internal.keys.pop_back();

		child->internal.children.insert(
			child->internal.children.begin(),
			left->internal.children.back());

		left->internal.children.pop_back();
	}
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::borrowFromNext(Node *node, size_t idx)
{
	Node *child = node->internal.children[idx];
	Node *right = node->internal.children[idx + 1];

	if (child->isLeaf)
	{
		// steal one entry from right leaf
		child->leaf.entries.push_back(std::move(right->leaf.entries.front()));
		right->leaf.entries.erase(right->leaf.entries.begin());
		// update parent key
		node->internal.keys[idx] = right->leaf.entries.front().first;
	}
	else
	{
		// steal one key+child from right internal
		child->internal.keys.push_back(node->internal.keys[idx]);
		node->internal.keys[idx] = right->internal.keys.front();
		right->internal.keys.erase(right->internal.keys.begin());

		child->internal.children.push_back(right->internal.children.front());
		right->internal.children.erase(right->internal.children.begin());
	}
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::mergeNodes(Node *node, size_t idx)
{
	Node *left = node->internal.children[idx];
	Node *right = node->internal.children[idx + 1];

	if (left->isLeaf)
	{
		// merge leaf entries
		left->leaf.entries.insert(
			left->leaf.entries.end(),
			std::make_move_iterator(right->leaf.entries.begin()),
			std::make_move_iterator(right->leaf.entries.end()));

		// stitch leaf list
		left->nextLeaf = right->nextLeaf;

		if (right->nextLeaf)
			right->nextLeaf->prevLeaf = left;
	}
	else
	{
		// pull down the separating key
		left->internal.keys.push_back(node->internal.keys[idx]);
		// append right’s keys and children
		left->internal.keys.insert(
			left->internal.keys.end(),
			right->internal.keys.begin(),
			right->internal.keys.end()
		);

		left->internal.children.insert(
			left->internal.children.end(),
			right->internal.children.begin(),
			right->internal.children.end()
		);
	}

	// remove right sibling
	node->internal.children.erase(node->internal.children.begin() + idx + 1);
	node->internal.keys.erase(node->internal.keys.begin() + idx);

	delete right;
}

template <typename Key, typename Value, typename Compare>
void BTree<Key, Value, Compare>::removeFromNode(Node *node, const Key &key)
{
	// 1) Find the first index ≥ key
	size_t idx = 0;
	while (idx < node->internal.keys.size() && less(node->internal.keys[idx], key))
		++idx;

	// 2) Case A: key is in this node (and node is leaf or internal)
	if (idx < node->internal.keys.size() && !less(key, node->internal.keys[idx]) && !less(node->internal.keys[idx], key))
	{
		if (node->isLeaf)
		{
			// A1) Leaf: erase the entry
			auto eit = std::find_if(
				node->leaf.entries.begin(),
				node->leaf.entries.end(),
				[&](auto const &p)
				{ return !less(p.first, key) && !less(key, p.first); });

			if (eit != node->leaf.entries.end())
			{
				node->leaf.entries.erase(eit);
			}
		}
		else
		{
			// A2) Internal: three subcases
			Node *leftChild = node->internal.children[idx];
			Node *rightChild = node->internal.children[idx + 1];
			size_t leftCount = leftChild->isLeaf ? leftChild->leaf.entries.size()
							: leftChild->internal.keys.size();
			size_t rightCount = rightChild->isLeaf ? rightChild->leaf.entries.size()
							: rightChild->internal.keys.size();

			if (leftCount >= BTree::s_CAPACITY)
			{
				Key pred = getPredecessor(node, idx);

				node->internal.keys[idx] = pred;
				removeFromNode(leftChild, pred);
			}
			else if (rightCount >= BTree::s_CAPACITY)
			{
				Key succ = getSuccessor(node, idx);

				node->internal.keys[idx] = succ;
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

		bool lastChild = (idx == node->internal.keys.size());
		Node *child = node->internal.children[idx];
		size_t childCount = child->isLeaf ? child->leaf.entries.size()
						: child->internal.keys.size();

		// ensure child has at least CAPACITY keys
		if (childCount < BTree::s_CAPACITY)
		{
			fill(node, idx);
		}

		// after fill(), decide correct subtree
		if (lastChild && idx > node->internal.keys.size())
			removeFromNode(node->internal.children[idx - 1], key);
		else
			removeFromNode(node->internal.children[idx], key);
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

		// node->children.insert(node->children.begin(), c);
		trivial_insert(node->children, 0, c);
		// node->keys.insert(node->keys.begin(), sep);
		trivial_insert(node->keys, 0, sep);

		parent->keys[index - 1] = k2;

		return ;
	}

	if (right && right->keys.size() > minKeys) {
		Key sep = parent->keys[index];
		Node* c = right->children.front();

		// right->children.erase(right->children.begin());
		trivial_erase(right->children, 0);

		Key k2 = right->keys.front();

		// right->keys.erase(right->keys.begin());
		trivial_erase(right->keys, 0);

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

		// parent->children.erase(parent->children.begin() + index);
		trivial_erase(parent->children, index);
		// parent->keys.erase(parent->keys.begin() + index - 1);
		trivial_erase(parent->keys, index - 1);

		delete node;
	} else {
		Key sep = parent->keys[index];

		node->keys.push_back(sep);
		node->keys.insert(node->keys.end(), right->keys.begin(), right->keys.end());
		node->children.insert(node->children.end(), right->children.begin(), right->children.end());

		// parent->children.erase(parent->children.begin() + index + 1);
		trivial_erase(parent->children, index + 1);
		// parent->keys.erase(parent->keys.begin() + index);
		trivial_erase(parent->keys, index);

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
			n->internal.keys.begin(), n->internal.keys.end(), low,
			[this](auto const &a, auto const &b) { return m_Comp(a, b); }
		);
		size_t childIdx = std::distance(n->internal.keys.begin(), it);

		n = n->internal.children[childIdx];
	}

	// 2) In that leaf, find the first entry >= low
	auto entryIt = std::lower_bound(
		n->leaf.entries.begin(), n->leaf.entries.end(), low,
		[this](std::pair<Key, Value> const &entry, auto const &val)
		{ return m_Comp(entry.first, val); });

	size_t idx = std::distance(n->leaf.entries.begin(), entryIt);

	// 3) Collect until > high, hopping leaves as needed
	while (n) {
		while (idx < n->leaf.entries.size()) {
			auto &kv = n->leaf.entries[idx];

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
			n->internal.keys.begin(), n->internal.keys.end(), low,
			[this](auto const &a, auto const &b) { return m_Comp(a, b); }
		);

		n = n->internal.children[std::distance(n->internal.keys.begin(), it)];
	}

	// 2) Find first >= low
	auto entryIt = std::lower_bound(
		n->leaf.entries.begin(), n->leaf.entries.end(), low,
		[this](auto const &entry, auto const &val) { return m_Comp(entry.first, val); }
	);

	size_t idx = std::distance(n->leaf.entries.begin(), entryIt);
	size_t taken = 0;

	// 3) Collect up to count
	while (n && taken < count) {
		if (idx < n->leaf.entries.size()) {
			auto &kv = n->leaf.entries[idx++];
			out.push_back({&kv.first, &kv.second});

			++taken;
		} else {
			n = n->nextLeaf;
			idx = 0;
		}
	}

	return out;
}

template <typename Key, typename Value, typename Compare>
bool BTree<Key, Value, Compare>::move(const Key &from, const Key &to)
{
	const Value* value = this->search(from);

	if (!value)
	{
		return false;
	}

	this->remove(from);
	this->insert(to, *value);

	return true;
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
		for (auto const &[k, v] : node->leaf.entries)
		{
			j["entries"].push_back(json::array({k, v}));
		}

		// children: recurse or empty array for leaves
		j["children"] = json::array();

		if (!node->isLeaf)
		{
			for (Node *child : node->internal.children)
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
BTree<Key, Value, Compare>::Iterator::Iterator() noexcept :
	m_Tree(nullptr),
	m_CurrentNode(nullptr),
	m_CurrentIndex(0) {}

template <typename Key, typename Value, typename Compare>
BTree<Key, Value, Compare>::Iterator::Iterator(BTree *tree, Node *node, size_t index) noexcept :
	m_Tree(tree),
	m_CurrentNode(node),
	m_CurrentIndex(index) {}

template <typename Key, typename Value, typename Compare>
std::pair<const Key &, Value &> BTree<Key, Value, Compare>::Iterator::operator*() const
{
	auto &kv = m_CurrentNode->leaf.entries[m_CurrentIndex];

	return { kv.first, kv.second };
}

template <typename Key, typename Value, typename Compare>
typename BTree<Key, Value, Compare>::Iterator& BTree<Key, Value, Compare>::Iterator::operator++()
{
	if (!m_CurrentNode)
	{
		return *this;
	}

	if (++m_CurrentIndex >= m_CurrentNode->leaf.entries.size())
	{
		m_CurrentNode = m_CurrentNode->nextLeaf;
		m_CurrentIndex = 0;
	}

	return *this;
}

template <typename Key, typename Value, typename Compare>
typename BTree<Key, Value, Compare>::Iterator BTree<Key, Value, Compare>::Iterator::operator++(int)
{
	Iterator tmp = *this;

	++*this;

	return tmp;
}

template <typename Key, typename Value, typename Compare>
typename BTree<Key, Value, Compare>::Iterator &BTree<Key, Value, Compare>::Iterator::operator--()
{
	if (!m_CurrentNode)
	{
		Node *n = m_Tree->m_Root;

		while (n && !n->isLeaf)
		{
			n = n->children.back();
		}

		if (n && !n->entries.empty())
		{
			m_CurrentNode = n;
			m_CurrentIndex = n->entries.size() - 1;
		}

		return *this;
	}

	// still in a leaf: back up index
	if (m_CurrentIndex > 0)
	{
		--m_CurrentIndex;

		return *this;
	}

	// at index 0, hop to previous leaf
	Node *prev = m_CurrentNode->prevLeaf;

	m_CurrentNode = prev;
	m_CurrentIndex = prev ? prev->entries.size() - 1 : 0;

	return *this;
}

template <typename Key, typename Value, typename Compare>
typename BTree<Key, Value, Compare>::Iterator BTree<Key, Value, Compare>::Iterator::operator--(int)
{
	Iterator tmp = *this;

	--*this;

	return tmp;
}

template <typename Key, typename Value, typename Compare>
bool BTree<Key, Value, Compare>::Iterator::operator==(Iterator const &o) const
{
	return m_CurrentNode == o.m_CurrentNode && m_CurrentIndex == o.m_CurrentIndex;
}

template <typename Key, typename Value, typename Compare>
bool BTree<Key, Value, Compare>::Iterator::operator!=(Iterator const &o) const
{
	return !(*this == o);
}

template <typename Key, typename Value, typename Compare>
typename BTree<Key, Value, Compare>::Iterator BTree<Key, Value, Compare>::begin()
{
	Node* n = m_Root;

	while (n && !n->isLeaf) {
		n = n->internal.children.front();
	}

	return Iterator(this, n, 0);
}

template <typename Key, typename Value, typename Compare>
typename BTree<Key, Value, Compare>::Iterator BTree<Key, Value, Compare>::end() noexcept
{
	return Iterator(this, nullptr, 0);
}

template <typename Key, typename Value, typename Compare>
auto BTree<Key, Value, Compare>::rbegin() -> std::reverse_iterator<BTree<Key, Value, Compare>::Iterator>
{
	return std::reverse_iterator<BTree<Key, Value, Compare>::Iterator>(end());
}

template <typename Key, typename Value, typename Compare>
auto BTree<Key, Value, Compare>::rend() noexcept -> std::reverse_iterator<BTree<Key, Value, Compare>::Iterator>
{
	return std::reverse_iterator<BTree<Key, Value, Compare>::Iterator>(begin());
}
