#include<node.h>
#include<node_object_wrap.h>

#include "btree.h"

using namespace v8;

namespace BTreeAddon
{

	/**
	 * @brief A lightweight wrapper for a V8 JavaScript value that can be
	 *        used as a `BTree<Key, Value, Compare>` key or value.
	 *
	 *        Stores either a raw double when the JS value is a Number,
	 *        or a Persistent<Value> handle for all other types.
	 *        This lets internal nodes manipulate numeric keys without
	 *        heap allocations, while still supporting arbitrary JS values.
	*/
	struct JsHandle {
		/**
		 * @brief The V8 isolate this handle is associated with.
		*/
		Isolate *i;

		/**
		 * @brief Persistent handle for non-numeric JS values.
		*/
		Persistent<Value, NonCopyablePersistentTraits<Value>> h;

		/**
		 * @brief True if the `v8::Value` is a Number, false otherwise.
		*/
		bool isNumber;

		/**
		 * @brief The numeric value if the `v8::Value` is a Number.
		*/
		double numberVal;

		/**
		 * @brief Constructs a new JsHandle from a V8 `Local<Value>`.
		 *
		 * @param isolate The V8 isolate to use for creating the Persistent handle.
		 * @param value   The JS value to wrap. If value->IsNumber(), the
		 *   numeric data is stored in numberVal; otherwise
		 *   a Persistent handle is created.
		*/
		BTreeAddon::JsHandle(Isolate *isolate, Local<Value> value);

		/**
		 * @brief Copy-constructs a JsHandle.
		 *
		 * @param other The JsHandle to duplicate. If other.isNumber is true,
		 *   copies the raw double; otherwise duplicates the Persistent handle.
		*/
		BTreeAddon::JsHandle(BTreeAddon::JsHandle const &other);

		/**
		 * @brief Copy-assigns from another JsHandle.
		 *
		 * @param other The source JsHandle.
		 * @return Reference to this JsHandle after assignment.
		*/
		BTreeAddon::JsHandle& operator=(BTreeAddon::JsHandle const &other);

		/**
		 * @brief Releases any Persistent handle held by this object.
		 *
		 *  If isNumber is false, resets the `Persistent<Value>` so V8
		 *  can reclaim the underlying JS object. Numeric values require no cleanup.
		*/
		~JsHandle();
	};

	/**
	 * @brief Functor that provides a strict weak ordering for JsHandle keys.
	 *
	 * Ensures a consistent, total ordering of JavaScript values in the B+Tree:
	 * - If both handles wrap numbers, compares their stored numberVal.
	 * - Otherwise falls back to V8’s comparison APIs on the underlying Value.
	*/
	struct JsComparator {
		/**
		 * @brief The V8 isolate this JsComparator is associated with.
		*/
		Isolate *i;

		/**
		 * @brief Default constructor that captures the current V8 isolate.
		 *
		 * If no isolate is explicitly provided, this pulls from the
		 * active V8 context.
		*/
		BTreeAddon::JsComparator();

		/**
		 * @brief Constructs a comparator using the given V8 isolate.
		 *
		 * @param isolate The V8 isolate to use for subsequent comparisons.
		*/
		BTreeAddon::JsComparator(Isolate *isolate);

		/**
		 * @brief Copy-constructs a JsComparator.
		 *
		 * @param o The comparator to duplicate.
		*/
		BTreeAddon::JsComparator(BTreeAddon::JsComparator const &o);

		/**
		 * @brief Copy-assigns from another JsComparator.
		 *
		 * @param o The source comparator.
		 * @return Reference to this comparator after assignment.
		*/
		BTreeAddon::JsComparator& operator=(BTreeAddon::JsComparator const &o);

		/**
		 * @brief Performs a less-than comparison between two JsHandle values.
		 *
		 * @param a The first JS value wrapper.
		 * @param b The second JS value wrapper.
		 * @return true if `a` is considered less than `b` under JS ordering rules.
		*/
		bool operator()(BTreeAddon::JsHandle const &a, BTreeAddon::JsHandle const &b) const;
	};

	using BTreeJs = BTree<JsHandle, JsHandle, JsComparator>;

	/**
	 * @brief Node.js wrapper around a native B+Tree of JsHandle keys and values.
	 *
	 *        Exposes a JavaScript class with instance methods that forward to the
	 *        underlying BTree<JsHandle,JsHandle,JsComparator> API.
	*/
	class BTreeWrapper : public node::ObjectWrap {
		public:
			/**
			 * @brief Initializes the BTree constructor and prototype methods on the
			 *        `exports` object.
			 *
			 *        Binds:
			 *          - New()          → JS `new BTree()`
			 *          - Insert()       → JS `BTree::insert()`
			 *          - Search()       → JS `BTree::search()`
			 *          - Remove()       → JS `BTree::remove()`
			 *          - Size()         → JS `BTree::size()`
			 *          - Range()        → JS `BTree::range()`
			 *          - RangeCount()   → JS `BTree::range()`
			 *
			 * @param exports The module exports to which the constructor is attached.
			 */
			static void Init(Local<Object> exports);

		private:
			/**
			 * @brief Constructs an empty native B+Tree instance.
			 *
			 *        Internally does:
			 *          m_Tree = std::make_unique<BTreeJs>();
			 */
			explicit BTreeWrapper();

			/**
			 * @brief Destroys the native B+Tree instance. Called when the variable is out of JS scope
			*/
			~BTreeWrapper() override;

			/**
			 * @brief JavaScript constructor for `new BTree()`.
			 *
			 *        Creates a new BTreeWrapper C++ object and wraps it in the JS `this`.
			 *        Throws a TypeError if not called as a constructor.
			 *
			 * @param args V8 callback info, containing the `this` object.
			 * @throws Throws `v8::Exception::TypeError` if not called as a constructor
			*/
			static void New(const FunctionCallbackInfo<Value>& args);

			/**
			 * @brief JS `BTree::insert()` method.
			 *
			 * Converts two JS values to JsHandle and invokes: `m_Tree->insert(keyHandle, valueHandle);`
			 *
			 * @param args args[0]=key, args[1]=value
			*/
			static void Insert(const FunctionCallbackInfo<Value>& args);

			/**
			 * @brief JS `BTree::search()` method.
			 *
			 * Converts args[0] to JsHandle and invokes: `m_Tree->search(keyHandle);`
			 *        Returns a JS array of matching values.
			 *
			 * @param args args[0]=key
			*/
			static void Search(const FunctionCallbackInfo<Value>& args);

			/**
			 * @brief JS `BTree::remove()` method.
			 *
			 *        Converts args[0] to JsHandle and invokes:
			 *          `size_t removed = m_Tree->remove(keyHandle);`
			 *        Returns a boolean whether the key was removed or not.
			 *
			 * @param args args[0]=key
			*/
			static void Remove(const FunctionCallbackInfo<Value>& args);

			/**
			 * @brief JS `BTree::size()` method.
			 *
			 *        Invokes:
			 *          `size_t count = m_Tree->size();`
			 *        Returns the total number of entries in the tree.
			 *
			 * @param args No arguments expected.
			*/
			static void Size(const FunctionCallbackInfo<Value>& args);

			/**
			 * @brief JS `BTree::range(const Key &low, const Key &high)` method.
			 *
			 *        Converts args[0], args[1] to JsHandle and invokes:
			 *          `auto results = m_Tree->range(startHandle, endHandle);`
			 *        Returns a JS array of values in [start, end].
			 *
			 * @param args args[0]=startKey, args[1]=endKey
			 */
			static void Range(const FunctionCallbackInfo<Value>& args);

			/**
			 * @brief JS `BTree::range(const Key &low, size_t count)` method.
			 *
			 *        Converts args[0], args[1] to JsHandle and invokes:
			 *          `size_t count = m_Tree->rangeCount(startHandle, size_t(args[1]));`
			 *        Returns the count of values in [start, end].
			 *
			 * @param args args[0]=startKey, args[1]=count
			 */
			static void RangeCount(const FunctionCallbackInfo<Value>& args);

			/**
			 * @brief The underlying BTree<JsHandle,JsHandle,JsComparator> instance
			 */
			std::unique_ptr<BTreeJs> m_Tree;

			/**
			 * @brief Persistent JS constructor for the BTree class.
			 */
			static Persistent<Function, NonCopyablePersistentTraits<Function>> s_Constructor;
	};
}
