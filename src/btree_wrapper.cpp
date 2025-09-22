#include <node.h>

#include "btree.h"
#include "btree_wrapper.h"

using namespace v8;

namespace BTreeAddon
{
	using BTreeJs = BTree<JsHandle, JsHandle, JsComparator>;

	JsHandle::JsHandle(Isolate *isolate, Local<Value> value) : i(isolate)
	{
		if (value->IsNumber()) {
			isNumber = true;
			numberVal = value->NumberValue(isolate->GetCurrentContext()).ToChecked();
		} else if (value->IsString()) {
			isString = true;
			auto str = value->ToString(i->GetCurrentContext()).ToLocalChecked();
			stringVal.assign(*String::Utf8Value(isolate, str));
		} else {
			h.Reset(i, value);
		}
	}
	JsHandle::JsHandle(JsHandle const &other) :
		i(other.i),
		isNumber(other.isNumber),
		isString(other.isString),
		numberVal(other.numberVal),
		stringVal(other.stringVal)
	{
		if (!isNumber && !isString) {
			h.Reset(i, Local<Value>::New(i, other.h));
		}
	}
	// JsHandle::JsHandle(JsHandle &&) noexcept = default;
	// JsHandle &JsHandle::operator=(JsHandle &&) noexcept = default;

	JsHandle &JsHandle::operator=(JsHandle const &other)
	{
		if (this != &other)
		{
			i = other.i;
			isNumber = other.isNumber;
			isString = other.isString;
			numberVal = other.numberVal;
			stringVal = other.stringVal;
			h.Reset();

			if (!isNumber && !isString)
			{
				h.Reset(i, Local<Value>::New(i, other.h));
			}
		}

		return *this;
	}

	JsHandle::~JsHandle() {
		if (!isNumber && !isString) {
			h.Reset();
		}
	}

	JsComparator::JsComparator() : i(Isolate::GetCurrent()) {}
	JsComparator::JsComparator(Isolate *isolate) : i(isolate) {}
	JsComparator::JsComparator(JsComparator const &o) = default;
	JsComparator& JsComparator::operator=(JsComparator const &o) = default;
	bool JsComparator::operator()(JsHandle const &a, JsHandle const &b) const {
		if (a.isNumber &&b.isNumber) {
			return a.numberVal < b.numberVal;
		}

		if (a.isString && b.isString) {
			return a.stringVal < b.stringVal;
		}

		HandleScope hs(i);

		Local<Context> ctx = i->GetCurrentContext();

		Local<Value> la = v8::Local<v8::Value>::New(i, a.h);
		Local<Value> lb = v8::Local<v8::Value>::New(i, b.h);

		// If they’re strictly equal, neither is less
		if (la->StrictEquals(lb))
		{
			return false;
		}

		// Use V8’s generic Equals (returns Maybe<bool>)
		Maybe<bool> maybeEq = la->Equals(ctx, lb);
		bool isEq = maybeEq.FromMaybe(false);

		if (isEq)
		{
			return false;
		}

		// Last resort: compare type names or addresses for a stable ordering
		// e.g. numbers < strings < objects, or pointer‐based tie breaker
		// Here, we’ll just compare the pointer values of the persistent handles:
		auto pa = reinterpret_cast<uintptr_t>(*la);
		auto pb = reinterpret_cast<uintptr_t>(*lb);

		return pa < pb;
	}

	Persistent<Function, NonCopyablePersistentTraits<Function>> BTreeWrapper::s_Constructor;

	BTreeWrapper::BTreeWrapper() : m_Tree(std::make_unique<BTreeJs>()) {}

	BTreeWrapper::~BTreeWrapper() = default;

	void BTreeWrapper::New(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();

		if (!args.IsConstructCall())
		{
			isolate->ThrowException(
				Exception::TypeError(String::NewFromUtf8(
					isolate,
					"BTree must be constructed with new",
					NewStringType::kNormal)
					.ToLocalChecked())
			);

			return;
		}

		BTreeWrapper* self = new BTreeWrapper();

		self->Wrap(args.This());

		args.GetReturnValue().Set(args.This());
	}

	void BTreeWrapper::Insert(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		Local<Context> ctx = isolate->GetCurrentContext();

		if (args.Length() < 2) {
			isolate->ThrowException(Exception::TypeError(
				String::NewFromUtf8Literal(isolate, "insert requires 2 arguments: key (number), value (string)"))
			);

			return;
		}

		if (!args[0]->IsNumber() || !args[1]->IsString())
		{
			isolate->ThrowException(Exception::TypeError(
				String::NewFromUtf8Literal(isolate, "insert arguments must be (number, string)"))
			);

			return;
		}

		BTreeWrapper* wrap = ObjectWrap::Unwrap<BTreeWrapper>(args.Holder());

		JsHandle key(isolate, args[0]);
		JsHandle value(isolate, args[1]);

		bool ok = wrap->m_Tree->insert(key, value);

		args.GetReturnValue().Set(Boolean::New(isolate, ok));
	}

	void BTreeWrapper::Search(const FunctionCallbackInfo<Value>& args)
	{
		Isolate* isolate = args.GetIsolate();
		Local<Context> ctx = isolate->GetCurrentContext();

		if (args.Length() < 1)
		{
			isolate->ThrowException(Exception::TypeError(
				String::NewFromUtf8Literal(isolate, "search requires 1 argument: key (number)"))
			);

			return;
		}

		if (!args[0]->IsNumber())
		{
			isolate->ThrowException(Exception::TypeError(
				String::NewFromUtf8Literal(isolate, "search argument must be a number"))
			);

			return;
		}

		BTreeWrapper* wrap = ObjectWrap::Unwrap<BTreeWrapper>(args.Holder());
		JsHandle key(isolate, args[0]);
		JsHandle* result = wrap->m_Tree->search(key);

		if (!result) {
			args.GetReturnValue().Set(Null(isolate));

			return;
		}

		Local<Value> outVal;
		if (result->isNumber)
		{
			outVal = Number::New(isolate, result->numberVal);
		}
		else if (result->isString)
		{
			outVal = String::NewFromUtf8(
				isolate,
				result->stringVal.c_str(),
				NewStringType::kNormal
			).ToLocalChecked();
		}
		else
		{
			outVal = Local<Value>::New(isolate, result->h);
		}

		args.GetReturnValue().Set(outVal);
	}

	void BTreeWrapper::Remove(const FunctionCallbackInfo<Value>& args)
	{
		Isolate* isolate = args.GetIsolate();
		Local<Context> ctx = isolate->GetCurrentContext();

		if (args.Length() < 1)
		{
			isolate->ThrowException(Exception::TypeError(
				String::NewFromUtf8Literal(isolate, "remove requires 1 argument: key (number)"))
			);

			return;
		}
		if (!args[0]->IsNumber())
		{
			isolate->ThrowException(Exception::TypeError(
				String::NewFromUtf8Literal(isolate, "remove argument must be a number"))
			);

			return;
		}

		BTreeWrapper* wrap = ObjectWrap::Unwrap<BTreeWrapper>(args.Holder());
		JsHandle key(isolate, args[0]);

		bool ok = wrap->m_Tree->remove(key);

		args.GetReturnValue().Set(Boolean::New(isolate, ok));
	}

	void BTreeWrapper::Size(const FunctionCallbackInfo<Value>& args)
	{
		Isolate* isolate = args.GetIsolate();
		BTreeWrapper* wrap = ObjectWrap::Unwrap<BTreeWrapper>(args.Holder());
		size_t size = wrap->m_Tree->size();

		args.GetReturnValue().Set(Number::New(isolate, static_cast<double>(size)));
	}

	void BTreeWrapper::Range(const FunctionCallbackInfo<Value>& args)
	{
		Isolate* isolate = args.GetIsolate();
		Local<Context> ctx = isolate->GetCurrentContext();

		if (args.Length() < 2) {
			isolate->ThrowException(Exception::TypeError(
				String::NewFromUtf8Literal(isolate, "range requires 2 arguments: low (number), high (number)")
			));

			return ;
		}

		BTreeWrapper* wrap = ObjectWrap::Unwrap<BTreeWrapper>(args.Holder());
		JsHandle low(isolate, args[0]);
		JsHandle high(isolate, args[1]);
		auto entries = wrap->m_Tree->range(low, high);
		Local<Map> result = Map::New(isolate);

		for (std::pair<const JsHandle*, JsHandle*> entry : entries)
		{
			Local<Value> keyOut;
			if (entry.first->isNumber)
			{
				keyOut = Number::New(isolate, entry.first->numberVal);
			}
			else if (entry.first->isString)
			{
				keyOut = String::NewFromUtf8(
					isolate,
					entry.first->stringVal.c_str(),
					NewStringType::kNormal
				).ToLocalChecked();
			}
			else
			{
				keyOut = Local<Value>::New(isolate, entry.first->h);
			}

			Local<Value> valOut;
			if (entry.second->isNumber)
			{
				valOut = Number::New(isolate, entry.second->numberVal);
			}
			else if (entry.second->isString)
			{
				valOut = String::NewFromUtf8(
					isolate,
					entry.second->stringVal.c_str(),
					NewStringType::kNormal
				).ToLocalChecked();
			}
			else
			{
				valOut = Local<Value>::New(isolate, entry.second->h);
			}

			result->Set(ctx, keyOut, valOut).ToLocalChecked();
		}

		args.GetReturnValue().Set(result);
	}

	void BTreeWrapper::RangeCount(const FunctionCallbackInfo<Value> &args)
	{
		Isolate* isolate = args.GetIsolate();
		Local<Context> ctx = isolate->GetCurrentContext();

		if (args.Length() < 2)
		{
			isolate->ThrowException(Exception::TypeError(
				String::NewFromUtf8Literal(isolate, "range requires 2 arguments: low (number), count (number)")));

			return;
		}

		BTreeWrapper* wrap = ObjectWrap::Unwrap<BTreeWrapper>(args.Holder());
		JsHandle low(isolate, args[0]);
		size_t count = static_cast<size_t>(args[1].As<Number>()->Value());
		auto entries = wrap->m_Tree->range(low, count);
		Local<Map> result = Map::New(isolate);

		for (std::pair<const JsHandle*, JsHandle*> entry : entries)
		{
			Local<Value> keyOut;
			if (entry.first->isNumber)
			{
				keyOut = Number::New(isolate, entry.first->numberVal);
			}
			else if (entry.first->isString)
			{
				keyOut = String::NewFromUtf8(
					isolate,
					entry.first->stringVal.c_str(),
					NewStringType::kNormal)
					.ToLocalChecked();
			}
			else
			{
				keyOut = Local<Value>::New(isolate, entry.first->h);
			}

			Local<Value> valOut;
			if (entry.second->isNumber)
			{
				valOut = Number::New(isolate, entry.second->numberVal);
			}
			else if (entry.second->isString)
			{
				valOut = String::NewFromUtf8(
					isolate,
					entry.second->stringVal.c_str(),
					NewStringType::kNormal)
					.ToLocalChecked();
			}
			else
			{
				valOut = Local<Value>::New(isolate, entry.second->h);
			}

			result->Set(ctx, keyOut, valOut).ToLocalChecked();
		}

		args.GetReturnValue().Set(result);
	}

	void BTreeWrapper::Init(Local<Object> exports)
	{
		Isolate *isolate = exports->GetIsolate();
		Local<Context> ctx = isolate->GetCurrentContext();

		Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);

		tpl->SetClassName(String::NewFromUtf8(isolate, "BTreeJS", NewStringType::kNormal).ToLocalChecked());
		tpl->InstanceTemplate()->SetInternalFieldCount(1);

		NODE_SET_PROTOTYPE_METHOD(tpl, "insert", Insert);
		NODE_SET_PROTOTYPE_METHOD(tpl, "search", Search);
		NODE_SET_PROTOTYPE_METHOD(tpl, "remove", Remove);
		NODE_SET_PROTOTYPE_METHOD(tpl, "size", Size);
		NODE_SET_PROTOTYPE_METHOD(tpl, "range", Range);
		NODE_SET_PROTOTYPE_METHOD(tpl, "rangeCount", RangeCount);

		s_Constructor.Reset(isolate, tpl->GetFunction(ctx).ToLocalChecked());

		exports->Set(
			ctx,
			String::NewFromUtf8(isolate, "BTreeJs", NewStringType::kNormal).ToLocalChecked(),
			tpl->GetFunction(ctx).ToLocalChecked());
	}
}
