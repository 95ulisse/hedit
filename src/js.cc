#include <stdint.h>
#include <libplatform/libplatform.h>
#include <v8.h>

#include "js.h"
#include "util/log.h"

std::unique_ptr<v8::Platform> platform;
v8::Isolate::CreateParams create_params;
v8::Isolate* isolate;

void Log(const v8::FunctionCallbackInfo<v8::Value>& args) {
    for (int i = 0; i < args.Length(); i++) {
        v8::HandleScope handle_scope(args.GetIsolate());
        v8::String::Utf8Value str(args.GetIsolate(), args[i]);
        const char* cstr = *str;
        log_info("Message from JS: %s", cstr);
    }
}

bool hedit_js_init(int argc, char** argv) {

    log_debug("Initializing V8.");

    // Initialize V8
    v8::V8::InitializeICU();
    platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();

    // Create a new Isolate and make it the current one.
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate = v8::Isolate::New(create_params);

    {
        v8::Isolate::Scope isolate_scope(isolate);

        // Create a stack-allocated handle scope.
        v8::HandleScope handle_scope(isolate);

        // Create a template for the global object.
        v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

        // Bind the global 'log' function to the C++ Log callback.
        global->Set(
            v8::String::NewFromUtf8(isolate, "log", v8::NewStringType::kNormal)
                .ToLocalChecked(),
            v8::FunctionTemplate::New(isolate, Log));

        // Create a new context.
        v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global);

        // Enter the context for compiling and running the hello world script.
        v8::Context::Scope context_scope(context);

        // Create a string containing the JavaScript source code.
        v8::Local<v8::String> source =
            v8::String::NewFromUtf8(isolate, "log('Hello from JS!')",
                                    v8::NewStringType::kNormal)
                .ToLocalChecked();

        // Compile the source code.
        v8::Local<v8::Script> script =
            v8::Script::Compile(context, source).ToLocalChecked();

        // Run the script to get the result.
        v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

        if (result.IsEmpty()) {
            abort();
        }

    }

    return true;
}

void hedit_js_teardown() {
    log_debug("V8 teardown.");
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    delete create_params.array_buffer_allocator;
}
