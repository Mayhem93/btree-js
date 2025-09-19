#include <node.h>
#include <stdlib.h>

#include "btree.h"

using namespace v8;

namespace BTree {

  void BTreeWrapper (const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if (!args.IsConstructCall()) {
      isolate->ThrowException(
        Exception::TypeError(String::NewFromUtf8(
          isolate,
          "Cannot call constructor as function",
          NewStringType::kNormal
        ).ToLocalChecked())
      );

      return ;
    }

    // auto* tree = new BTree
  }

  void Initialize(Local<Object> exports) {
    Isolate *isolate = exports->GetIsolate();
    Local<Context> ctx = isolate->GetCurrentContext();

    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, BTreeWrapper);

    tpl->SetClassName(String::NewFromUtf8(isolate, "BTree", NewStringType::kNormal).ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    Local<Function> classFunc = tpl->GetFunction(ctx).ToLocalChecked();

    exports->Set(
      ctx,
      String::NewFromUtf8(isolate, "BTree", NewStringType::kNormal).ToLocalChecked(),
      classFunc
    ).Check();
  }

  NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
}
