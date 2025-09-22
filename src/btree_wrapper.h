#include<node.h>
#include<node_object_wrap.h>

#include "btree.h"

using namespace v8;

namespace BTreeAddon
{
	struct JsHandle {
		Isolate *i;
		Persistent<Value, NonCopyablePersistentTraits<Value>> h;
		bool isNumber;
		double numberVal;

		BTreeAddon::JsHandle(Isolate *isolate, Local<Value> value);
		BTreeAddon::JsHandle(BTreeAddon::JsHandle const &other);
		BTreeAddon::JsHandle &operator=(BTreeAddon::JsHandle const &other);

		~JsHandle();
	};

	struct JsComparator {
		Isolate *i;

		BTreeAddon::JsComparator();
		BTreeAddon::JsComparator(Isolate *isolate);
		BTreeAddon::JsComparator(BTreeAddon::JsComparator const &o);
		BTreeAddon::JsComparator& operator=(BTreeAddon::JsComparator const &o);
		bool operator()(BTreeAddon::JsHandle const &a, BTreeAddon::JsHandle const &b) const;
	};

	using BTreeJs = BTree<JsHandle, JsHandle, JsComparator>;

	class BTreeWrapper : public node::ObjectWrap {
		public:
			static void Init(Local<Object> exports);

		private:
			explicit BTreeWrapper();
			~BTreeWrapper() override;

			static void New(const FunctionCallbackInfo<Value>& args);
			static void Insert(const FunctionCallbackInfo<Value>& args);
			static void Search(const FunctionCallbackInfo<Value>& args);
			static void Remove(const FunctionCallbackInfo<Value>& args);
			static void Size(const FunctionCallbackInfo<Value>& args);
			static void Range(const FunctionCallbackInfo<Value>& args);
			static void RangeCount(const FunctionCallbackInfo<Value>& args);

			std::unique_ptr<BTreeJs> m_Tree;

			static Persistent<Function, NonCopyablePersistentTraits<Function>> s_Constructor;
	};
}
