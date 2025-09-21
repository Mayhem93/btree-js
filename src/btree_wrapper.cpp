#include <node.h>

#include "btree.h"
#include "btree_wrapper.h"

using namespace v8;

namespace BTreeAddon
{
	using BTreeJs = BTree<JsHandle, JsHandle, JsComparator>;

	JsHandle::JsHandle(Isolate *isolate, Local<Value> value) : h(isolate, value), i(isolate) {}
	JsHandle::JsHandle(JsHandle const &other) : i(other.i), h(other.i, Local<Value>::New(other.i, other.h)) {}
	JsHandle::JsHandle(JsHandle &&) noexcept = default;
	JsHandle &JsHandle::operator=(JsHandle &&) noexcept = default;

	JsHandle &JsHandle::operator=(JsHandle const &other) {
		if (this != &other)
		{
			i = other.i;
			h.Reset();
			h.Reset(i, Local<Value>::New(i, other.h));
		}

		return *this;
	}

	JsComparator::JsComparator() : i(Isolate::GetCurrent()) {}
	JsComparator::JsComparator(Isolate *isolate) : i(isolate) {}
	JsComparator::JsComparator(JsComparator const &o) = default;
	JsComparator& JsComparator::operator=(JsComparator const &o) = default;
	bool JsComparator::operator()(JsHandle const &a, JsHandle const &b) const {
		HandleScope hs(i);

		Local<Context> ctx = i->GetCurrentContext();

		Local<Value> la = v8::Local<v8::Value>::New(i, a.h);
		Local<Value> lb = v8::Local<v8::Value>::New(i, b.h);

		double da = la->NumberValue(ctx).ToChecked();
		double db = lb->NumberValue(ctx).ToChecked();

		return da < db;
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
		} else {
			Local<Value> outVal = Local<Value>::New(isolate, result->h);
			args.GetReturnValue().Set(outVal);
		}
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
			Local<Value> key = Local<Value>::New(isolate, entry.first->h);
			Local<Value> value = Local<Value>::New(isolate, entry.second->h);

			MaybeLocal<Map> mlResult = result->Set(ctx, key, value);

			if (mlResult.IsEmpty())
			{
				isolate->ThrowException(
					Exception::Error(String::NewFromUtf8Literal(isolate, "Map::Set failed"))
				);

				return;
			}
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
			Local<Value> key = Local<Value>::New(isolate, entry.first->h);
			Local<Value> value = Local<Value>::New(isolate, entry.second->h);
			MaybeLocal<Map> mlResult = result->Set(ctx, key, value);

			if (mlResult.IsEmpty())
			{
				isolate->ThrowException(
					Exception::Error(String::NewFromUtf8Literal(isolate, "Failed to set Map entry"))
				);

				return;
			}
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
