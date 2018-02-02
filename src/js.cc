#include <map>
#include <stdint.h>
#include <wordexp.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <libplatform/libplatform.h>
#include <v8.h>

#include "js.h"
#include "util/log.h"

using namespace v8;


// Global V8 structures
static std::unique_ptr<Platform> platform;
static Isolate::CreateParams create_params;
static Isolate* isolate;

// Contexts for evaluating the modules
static Persistent<Context> builtin_context;
static Persistent<Context> user_context;



JsBuiltinModule::JsBuiltinModule(std::string name, const unsigned char* source, unsigned int source_len) :
    _name(name),
    _source((const char*) source),
    _source_len((size_t) source_len)
{
}

JsBuiltinModule* JsBuiltinModule::FromName(std::string name) {
    auto it = _all_modules.find(name);
    if (it == _all_modules.end()) {
        return nullptr;
    } else {
        return it->second;
    }
}

const std::string& JsBuiltinModule::GetName() {
    return _name;
}

const char* JsBuiltinModule::GetSource() {
    return _source;
}

size_t JsBuiltinModule::GetSourceLen() {
    return _source_len;
}

Persistent<Module>& JsBuiltinModule::GetModule() {
    return _module;
}


// ----------------------------------------------------------------------------------------------------------


static void Test(const v8::FunctionCallbackInfo<v8::Value>& args) {
    HandleScope handle_scope(isolate);
    for (int i = 0; i < args.Length(); i++) {
        String::Utf8Value str(isolate, args[i]);
        log_info("%s", *str);
    }
}


// ----------------------------------------------------------------------------------------------------------


static MaybeLocal<Module> EvalModule(const char* origin_str, const Persistent<Context>& ctx, const char* contents, size_t len);
static MaybeLocal<Module> EvalModule(Local<String> origin_str, const Persistent<Context>& ctx, const char* contents, size_t len);

static inline Local<String> v8_str(const char* str, size_t len) {
    return String::NewFromUtf8(isolate, str, NewStringType::kNormal, len).ToLocalChecked();
}

static inline Local<String> v8_str(const char* str) {
    return v8_str(str, strlen(str));
}

static inline const char* c_str(String::Utf8Value& str) {
    return *str != NULL ? *str : "<string conversion failed>";
}

static MaybeLocal<Module> ModuleResolveCallback(Local<Context> ctx, Local<String> specifier, Local<Module> referrer) {
    EscapableHandleScope handle_scope(isolate);

    // A module can import only builtins
    String::Utf8Value s(isolate, specifier);
    JsBuiltinModule* builtin = JsBuiltinModule::FromName(std::string(c_str(s)));
    if (builtin == nullptr) {
        isolate->ThrowException(String::Concat(v8_str("Cannot resolve module "), specifier));
        return MaybeLocal<Module>();
    }

    // Return a cached version of the module, or compile it on the fly
    if (!builtin->GetModule().IsEmpty()) {
        return MaybeLocal<Module>(handle_scope.Escape(builtin->GetModule().Get(isolate)));
    } else {
        MaybeLocal<Module> m = EvalModule(
            String::Concat(v8_str("builtin:"), v8_str(builtin->GetName().c_str())),
            builtin_context,
            builtin->GetSource(),
            builtin->GetSourceLen()
        );
        if (!m.IsEmpty()) {
            builtin->GetModule().Reset(isolate, m.ToLocalChecked());
            return MaybeLocal<Module>(handle_scope.Escape(m.ToLocalChecked()));
        } else {
            return MaybeLocal<Module>();
        }
    }

}

static MaybeLocal<Module> EvalModule(const char* origin_str, const Persistent<Context>& ctx, const char* contents, size_t len)  {
    HandleScope handle_scope(isolate);
    return EvalModule(v8_str(origin_str), ctx, contents, len);
}

static MaybeLocal<Module> EvalModule(Local<String> origin_str, const Persistent<Context>& ctx, const char* contents, size_t len)  {
    Isolate::Scope isolate_scope(isolate);
    EscapableHandleScope handle_scope(isolate);

    // Enter the required context
    Context::Scope context_scope(ctx.Get(isolate));

    // Surround the evaluation with a try
    TryCatch tt(isolate);

    String::Utf8Value str(isolate, origin_str);
    const char* origin_cstr = c_str(str);
    log_debug("Evaluating module %s", origin_cstr);

    // Compile the module
    ScriptOrigin origin = ScriptOrigin(origin_str, Local<v8::Integer>(), Local<v8::Integer>(),
                                       Local<v8::Boolean>(), Local<v8::Integer>(),
                                       Local<v8::Value>(), Local<v8::Boolean>(),
                                       Local<v8::Boolean>(), True(isolate));
    ScriptCompiler::Source source(v8_str(contents, len), origin);
    Local<Module> m;
    if (!ScriptCompiler::CompileModule(isolate, &source).ToLocal(&m)) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        log_error("Exception while evaluating module %s: %s", origin_cstr, c_str(str));
        return MaybeLocal<Module>();
    }

    // Instantiate the module
    if (m->InstantiateModule(ctx.Get(isolate), ModuleResolveCallback).IsNothing()) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        log_error("Exception while evaluating module %s: %s", origin_cstr, c_str(str));
        return MaybeLocal<Module>();
    }

    // Evaluate the module
    if (m->Evaluate(ctx.Get(isolate)).IsEmpty()) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        log_error("Exception while evaluating module %s: %s", origin_cstr, c_str(str));
        return MaybeLocal<Module>();
    }

    return MaybeLocal<Module>(handle_scope.Escape(m));
}

bool hedit_js_init(int argc, char** argv) {

    log_debug("Initializing V8.");

    // Initialize V8
    V8::InitializeICU();
    ::platform = platform::NewDefaultPlatform();
    V8::InitializePlatform(::platform.get());
    V8::Initialize();

    // Create a new Isolate
    create_params.array_buffer_allocator = ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate = Isolate::New(create_params);

    {
        Isolate::Scope isolate_scope(isolate);
        HandleScope handle_scope(isolate);
        
        // Build the global `__hedit` object that will be exposed to builtin scripts
        Local<ObjectTemplate> obj = ObjectTemplate::New(isolate);
        obj->Set(String::NewFromUtf8(isolate, "test", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, Test));
        Local<ObjectTemplate> builtin_global = ObjectTemplate::New(isolate);
        builtin_global->Set(String::NewFromUtf8(isolate, "__hedit", NewStringType::kNormal).ToLocalChecked(), obj);

        // Prepare the contexts to be reused later
        user_context.Reset(isolate, Context::New(isolate));
        builtin_context.Reset(isolate, Context::New(isolate, NULL, builtin_global));
    }    

    return true;
}

void hedit_js_teardown() {
    log_debug("V8 teardown.");
    builtin_context.Reset();
    user_context.Reset();
    isolate->Dispose();
    V8::Dispose();
    V8::ShutdownPlatform();
    delete create_params.array_buffer_allocator;
}

void hedit_js_load_user_config(HEdit* hedit) {
    
    // Static search paths
    static const char* startup_script_paths[] = {
        "/etc/heditrc.js",
        "~/.heditrc",
        nullptr
    };

    for (const char** path = &startup_script_paths[0]; *path != nullptr; path++) {
        
        // Perform bash-like expansion of the path
        wordexp_t p;
        if (wordexp(*path, &p, WRDE_NOCMD) == 0 && p.we_wordc > 0) {
            char* expanded_path = p.we_wordv[0];

            // Open the file
            int fd;
            while ((fd = open(expanded_path, O_RDONLY)) == -1 && errno == EINTR);
            if (fd < 0) {
                if (errno != ENOENT) {
                    log_warn("Error loading %s: %s.", expanded_path, strerror(errno));
                }
                continue;
            }

            // Stat to get the size
            struct stat s;
            if (fstat(fd, &s) < 0) {
                log_warn("Cannot stat %s: %s.", expanded_path, strerror(errno));
                close(fd);
                continue;
            }

            // Mmap the file to memory
            const char* contents = (const char*) mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
            if (contents == MAP_FAILED) {
                log_warn("Cannot mmap %s: %s.", expanded_path, strerror(errno));
                close(fd);
                continue;
            }

            // Close the file
            close(fd);

            log_info("Loading %s.", expanded_path);

            // Evaluate the module
            if (EvalModule(expanded_path, user_context, contents, s.st_size).IsEmpty()) {
                log_warn("Loading of %s failed.", expanded_path);
            }

            // Unmap the file from the memory
            munmap((void*) contents, s.st_size);

        }
        wordfree(&p);

    }

}
