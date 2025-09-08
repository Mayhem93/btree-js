#include <node.h>
#include <stdlib.h>
#include <random>
#include <chrono>

#include "btree.h"

using namespace std;

namespace demo {
  using v8::FunctionCallbackInfo;
  using v8::Isolate;
  using v8::Local;
  using v8::Object;
  using v8::String;
  using v8::Value;
  using v8::Null;

  void Hello(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    auto tree = std::make_unique<BTree<int, std::string>>(50);
    unsigned seed = chrono::system_clock::now().time_since_epoch().count();
    mt19937 generate (seed);

    tree->insert(generate(), "hello xxx");
    tree->insert(generate(), "hello yyy");
    tree->insert(generate(), "hello zzz");

    auto result = tree->search(5);

    if (result) {
      args.GetReturnValue().Set(
        String::NewFromUtf8(isolate, result->c_str(), v8::NewStringType::kNormal).ToLocalChecked()
      );
    } else {
      args.GetReturnValue().Set(v8::Null(isolate));
    }

    // args.GetReturnValue().Set(String::NewFromUtf8(isolate, "{put the result here please}").ToLocalChecked());
  }

  void Initialize(Local<Object> exports) {
    NODE_SET_METHOD(exports, "hello", Hello);
  }

  NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
}
