#include <string>
#include <algorithm>
#include <regex>
#include <string.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include <libplatform/libplatform.h>
#include <v8.h>

#include "js.h"

using namespace std;
using namespace std::chrono;
using namespace v8;

static inline void fatal(string str) {
    cerr << str;
    abort();
}

static inline string readwhole(string path) {
    ifstream file(path);
    stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static inline Local<String> v8_str(const char* str, size_t len) {
    return String::NewFromUtf8(Isolate::GetCurrent(), str, NewStringType::kNormal, len).ToLocalChecked();
}

static inline Local<String> v8_str(const char* str) {
    return v8_str(str, strlen(str));
}

static inline const char* c_str(String::Utf8Value& str) {
    return *str != NULL ? *str : "<string conversion failed>";
}

static MaybeLocal<Module> EvalModule(const char* origin_str, const char* contents, size_t len);

static MaybeLocal<Module> ModuleResolveCallback(Local<Context> ctx, Local<String> specifier, Local<Module> referrer) {
    Isolate* isolate = Isolate::GetCurrent();
    EscapableHandleScope handle_scope(isolate);

    // A module can import only builtins
    String::Utf8Value s(isolate, specifier);
    shared_ptr<JsBuiltinModule> builtin = JsBuiltinModule::FromName(string(c_str(s)));
    if (builtin == nullptr) {
        isolate->ThrowException(String::Concat(v8_str("Cannot resolve module "), specifier));
        return MaybeLocal<Module>();
    }

    // Evaluate and return the module
    if (!builtin->GetHandle().IsEmpty()) {
        return MaybeLocal<Module>(handle_scope.Escape(builtin->GetHandle().Get(isolate)));
    } else {
        MaybeLocal<Module> m = EvalModule(
            (string("builtin:") + c_str(s)).c_str(),
            builtin->GetSource(),
            builtin->GetSourceLen()
        );
        if (!m.IsEmpty()) {
            builtin->GetHandle().Reset(isolate, m.ToLocalChecked());
            return MaybeLocal<Module>(handle_scope.Escape(builtin->GetHandle().Get(isolate)));
        } else {
            return MaybeLocal<Module>();
        }
    }
}

static MaybeLocal<Module> EvalModule(const char* origin_str, const char* contents, size_t len)  {
    Isolate* isolate = Isolate::GetCurrent();
    EscapableHandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();

    // Surround the evaluation with a try
    TryCatch tt(isolate);

    // Compile the module
    ScriptOrigin origin = ScriptOrigin(v8_str(origin_str), Local<v8::Integer>(), Local<v8::Integer>(),
                                       Local<v8::Boolean>(), Local<v8::Integer>(),
                                       Local<v8::Value>(), Local<v8::Boolean>(),
                                       Local<v8::Boolean>(), True(isolate));
    ScriptCompiler::Source source(v8_str(contents, len), origin);
    Local<Module> m;
    if (!ScriptCompiler::CompileModule(isolate, &source).ToLocal(&m)) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        fatal(string("Exception while evaluating module ") + origin_str + ": " + c_str(str));
        return MaybeLocal<Module>();
    }

    // Instantiate the module
    if (m->InstantiateModule(ctx, ModuleResolveCallback).IsNothing()) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        fatal(string("Exception while evaluating module ") + origin_str + ": " + c_str(str));
        return MaybeLocal<Module>();
    }

    // Evaluate the module
    if (m->Evaluate(ctx).IsEmpty()) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        fatal(string("Exception while evaluating module ") + origin_str + ": " + c_str(str));
        return MaybeLocal<Module>();
    }

    return MaybeLocal<Module>(handle_scope.Escape(m));
}



// ------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------



enum Color {
    Reset = 0,
    Bold = 1,
    Red = 31,
    Green = 32,
    Gray = 90,
    Orange = 93
};

class TColor {
public:
    TColor(Color c) : _color(c) {}

    friend ostream& operator<<(ostream& os, const TColor& c) {
        if (&os == &cout && !_isatty) {
            return os;
        } else {
            return os << "\033[" << c._color << "m";
        }
    }

private:
    Color _color;
    static bool _isatty;
};

bool TColor::_isatty = isatty(1);



static unsigned int _total = 0;
static unsigned int _failure = 0;

static void ReportSuiteBegin(string suite) {
    cout << suite << endl;
}

static void ReportSuiteEnd(string suite) {
}

static string PrettifyName(string& name) {
    string str = regex_replace(name, regex("[A-Z]"), " $&").substr(1);
    transform(str.begin() + 1, str.end(), str.begin() + 1, ::tolower);
    return str;
}

static void ReportSuccess(string test_name, high_resolution_clock::time_point start_time, high_resolution_clock::time_point end_time) {
    auto ms = (duration_cast<milliseconds>(end_time - start_time)).count();
    cout << "    " << PrettifyName(test_name) << " " << TColor(Bold) << TColor(Green) << "[OK] " << TColor(Reset)
         << TColor(ms < 100 ? Gray : Orange) << "[" << ms << "ms]" << TColor(Reset) << endl;
    _total++;
}

static void ReportFailure(string test_name, string msg) {
    msg = regex_replace(msg, regex("\\n"), "\n        ");
    cout << "    " << PrettifyName(test_name) << " " << TColor(Bold) << TColor(Green) << "[FAIL]" << TColor(Reset) << endl
         << "        " << TColor(Red) << msg << TColor(Reset) << endl;

    _total++;
    _failure++;
}

static void ReportFinalResults(high_resolution_clock::time_point start_time, high_resolution_clock::time_point end_time) {
    auto secs = (duration_cast<duration<double>>(end_time - start_time)).count();
    cout << TColor(Bold) << TColor(_failure == 0 ? Green : Red);
    cout << "Results: " << _total << " tests (" << (_total - _failure) << " passed, " << _failure << " failed) in " << secs << " seconds" << endl;
    cout << TColor(Reset);
}

static void RunTests(string suite, Local<Module> m) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();

    ReportSuiteBegin(suite);
    auto start_time = high_resolution_clock::now();

    // Each exported value is a test to run
    Local<Object> obj = Local<Object>::Cast(m->GetModuleNamespace());
    Local<Array> names = obj->GetOwnPropertyNames(ctx).ToLocalChecked();
    for (unsigned int i = 0; i < names->Length(); i++) {
        Local<v8::Value> key = names->Get(ctx, i).ToLocalChecked();
        String::Utf8Value key_str(isolate, key);
        Local<Function> test = Local<Function>::Cast(obj->Get(ctx, key).ToLocalChecked());

        // Invoke the function
        TryCatch tt;
        if (test->Call(ctx, Null(isolate), 0, {}).IsEmpty()) {
            Local<v8::Value> ex = tt.Exception();
            String::Utf8Value str(isolate, ex);
            // If we have a proper error object with a stack, report that
            if (ex->IsObject()) {
                Local<Object> ex_obj = Local<Object>::Cast(ex);
                MaybeLocal<v8::Value> stack = ex_obj->Get(ctx, v8_str("stack"));
                if (!stack.IsEmpty()) {
                    String::Utf8Value stack_str(isolate, stack.ToLocalChecked());
                    ReportFailure(c_str(key_str), c_str(stack_str));
                } else {
                    ReportFailure(c_str(key_str), c_str(str));
                }
            } else {
                ReportFailure(c_str(key_str), c_str(str));
            }
        } else {
            ReportSuccess(c_str(key_str), start_time, high_resolution_clock::now());
        }
    }

    ReportSuiteEnd(suite);
}

static void EmptyCallback(const FunctionCallbackInfo<v8::Value>& args) {
}

int main(int argc, const char* argv[]) {

    // Grab the paths to should.js and the test folder from the arguments
    string should_js_path = argv[1];
    string test_dir = argv[2];

    // Initialize V8
    V8::InitializeICU();
    Platform* platform = platform::CreateDefaultPlatform();
    V8::InitializePlatform(platform);
    V8::Initialize();

    // Create a new Isolate and make it the current one
    Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = ArrayBuffer::Allocator::NewDefaultAllocator();
    Isolate* isolate = Isolate::New(create_params);
    {
        Isolate::Scope isolate_scope(isolate);

        // Create a stack-allocated handle scope
        HandleScope handle_scope(isolate);

        // Create a new context to parse should.js
        Local<Context> context = Context::New(isolate);
        Context::Scope context_scope(context);

        // Create a fake `__hedit` global object with fake functions
        Local<Object> hedit = Object::New(isolate);
        hedit->Set(v8_str("registerEventBroker"), Function::New(context, EmptyCallback).ToLocalChecked());
        context->Global()->Set(v8_str("__hedit"), hedit);

        // Parse should.js
        string should_js_contents = readwhole(should_js_path);
        EvalModule(should_js_path.c_str(), should_js_contents.c_str(), should_js_contents.size()).ToLocalChecked();

        auto start_time = high_resolution_clock::now();

        // Cycle all the test files
        DIR* dir = opendir(test_dir.c_str());
        struct dirent* de;
        while ((de = readdir(dir)) != NULL) {
            if (de->d_type == DT_REG) {
                string path = test_dir + "/" + de->d_name;
                string contents = readwhole(path);
                Local<Module> m = EvalModule(path.c_str(), contents.c_str(), contents.size()).ToLocalChecked();
                RunTests(de->d_name, m);
            }
        }
        closedir(dir);

        ReportFinalResults(start_time, high_resolution_clock::now());
    }
 
    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    V8::Dispose();
    V8::ShutdownPlatform();
    delete platform;
    delete create_params.array_buffer_allocator;

    return _failure == 0 ? 0 : 1;
}
