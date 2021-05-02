#include <stdio.h>
#include "v8.h"

int main(int argc, char* argv[]) {
  printf("hello v8\n");
  bool init_result = v8::V8::Initialize();
  if (!init_result) {
    printf("v8 initialize failed\n");
    return -1;
  }
  auto ctx = v8::Context::New();
  v8::HandleScope hs;
  v8::Context::Scope context_scope(ctx);
  v8::Local<v8::String> source = v8::String::New("1+1");
  v8::Local<v8::Value> result = v8::Script::Compile(source)->Run();
  v8::String::AsciiValue str(result);
  printf("1+1=%s\n", *str);
  return 0;
}