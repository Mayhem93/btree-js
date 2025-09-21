#include <iostream>

#include <node.h>
#include <stdlib.h>

#include "btree.h"
#include "btree_wrapper.h"

using namespace v8;

namespace BTreeAddon {

  inline Local<Value> toV8Number(v8::Isolate *isolate, int32_t value)
  {
    return Number::New(isolate, value);
  }

  inline Local<Value> toV8String(Isolate *isolate, const std::string &str)
  {
    return String::NewFromUtf8(
               isolate,
               str.c_str(),
               NewStringType::kNormal)
        .ToLocalChecked();
  }

  void Initialize(Local<Object> exports) {
    BTreeWrapper::Init(exports);
  }

  NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
}
