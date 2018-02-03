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
#include "commands.h"
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

std::shared_ptr<JsBuiltinModule> JsBuiltinModule::FromName(std::string name) {
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


static inline Local<String> v8_str(const char* str, size_t len) {
    return String::NewFromUtf8(isolate, str, NewStringType::kNormal, len).ToLocalChecked();
}

static inline Local<String> v8_str(const char* str) {
    return v8_str(str, strlen(str));
}

static inline const char* c_str(String::Utf8Value& str) {
    return *str != NULL ? *str : "<string conversion failed>";
}

// __hedit.log("file", line, severity, "contents");
static void Log(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();

    assert(args.Length() == 4);

    String::Utf8Value file(isolate, args[0]);
    int line = args[1]->Int32Value(ctx).FromJust();
    int severity = args[2]->Int32Value(ctx).FromJust();
    String::Utf8Value contents(isolate, args[3]);

    __hedit_log(*file, line, (log_severity) severity, "%s", *contents);
}

// __hedit.emit("keys");
static void EmitKeys(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 1);

    String::Utf8Value keys(isolate, args[0]);

    hedit_emit_keys(hedit, *keys);
}

// __hedit.command("command");
static void Command(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 1);

    String::Utf8Value cmd(isolate, args[0]);
    char* cmddup = strdup(*cmd);
    if (cmddup == NULL) {
        log_fatal("Out of memory.");
        return;
    }

    args.GetReturnValue().Set(hedit_command_exec(hedit, cmddup));
    free(cmddup);
}

static bool JsCommandHandler(HEdit* hedit, bool force, ArgIterator* arg, void* user) {
    Persistent<Function>* handler = static_cast<Persistent<Function>*>(user);
    
    // Enter the js user context
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(user_context.Get(isolate));

    // Collect the arguments and convert them to js strings
    std::vector<Local<v8::Value>> jsargs;
    for (const char* a = it_next(arg); a != NULL; a = it_next(arg)) {
        jsargs.push_back(v8_str(a));
    }

    // Invoke the js callback
    TryCatch tt;
    if (handler->Get(isolate)->Call(user_context.Get(isolate), Null(isolate), jsargs.size(), jsargs.data()).IsEmpty()) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        log_error("Exception during JS command callback: %s", c_str(str));
        return false;
    }

    return true;
}

// __hedit.registerCommand("name", function(args) {});
static void RegisterCommand(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 2);

    if (!args[1]->IsFunction()) {
        isolate->ThrowException(v8_str("Expected function."));
        return;
    }

    String::Utf8Value command_name(isolate, args[0]);
    Persistent<Function>* handler = new Persistent<Function>(isolate, Local<Function>::Cast(args[1]));

    bool res = hedit_command_register(hedit, *command_name, JsCommandHandler, handler);
    args.GetReturnValue().Set(res);

    if (!res) {
        handler->Reset();
        delete handler;
    }

}

// __hedit.map("mode", "from", "to", force);
static void MapKeys(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 4);

    String::Utf8Value mode_name(isolate, args[0]);
    String::Utf8Value from(isolate, args[1]);
    String::Utf8Value to(isolate, args[2]);
    bool force = args[3]->BooleanValue(ctx).FromJust();

    Mode* mode = hedit_mode_from_name(*mode_name);
    if (mode == NULL) {
        isolate->ThrowException(
            String::Concat(v8_str("Invalid mode: "), args[0]->ToString(ctx).ToLocalChecked())
        );
        return;
    }

    args.GetReturnValue().Set(hedit_map_keys(hedit, mode->id, *from, *to, force));
}

// __hedit.set("name", "value");
static void SetOption(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 2);

    String::Utf8Value name(isolate, args[0]);
    String::Utf8Value value(isolate, args[1]);

    args.GetReturnValue().Set(hedit_option_set(hedit, *name, *value));
}

// __hedit.switchMode("mode");
static void SwitchMode(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 1);
    
    String::Utf8Value mode_name(isolate, args[0]);
    
    Mode* mode = hedit_mode_from_name(*mode_name);
    if (mode == NULL) {
        isolate->ThrowException(
            String::Concat(v8_str("Invalid mode: "), args[0]->ToString(ctx).ToLocalChecked())
        );
        return;
    }

    hedit_switch_mode(hedit, mode->id);
}


// ----------------------------------------------------------------------------------------------------------


static MaybeLocal<Module> EvalModule(const char* origin_str, const Persistent<Context>& ctx, const char* contents, size_t len);
static MaybeLocal<Module> EvalModule(Local<String> origin_str, const Persistent<Context>& ctx, const char* contents, size_t len);

static MaybeLocal<Module> ModuleResolveCallback(Local<Context> ctx, Local<String> specifier, Local<Module> referrer) {
    EscapableHandleScope handle_scope(isolate);

    // A module can import only builtins
    String::Utf8Value s(isolate, specifier);
    std::shared_ptr<JsBuiltinModule> builtin = JsBuiltinModule::FromName(std::string(c_str(s)));
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


static void LoadUserConfig() {
    
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
                goto next;
            }

            // Stat to get the size
            struct stat s;
            if (fstat(fd, &s) < 0) {
                log_warn("Cannot stat %s: %s.", expanded_path, strerror(errno));
                close(fd);
                goto next;
            }

            // Mmap the file to memory
            const char* contents = (const char*) mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
            if (contents == MAP_FAILED) {
                log_warn("Cannot mmap %s: %s.", expanded_path, strerror(errno));
                close(fd);
                goto next;
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

next:
        wordfree(&p);

    }

}

bool hedit_js_init(HEdit* hedit) {

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
        
        Local<External> he = External::New(isolate, hedit);

#define SET(name, f) \
    obj->Set(v8_str(name), FunctionTemplate::New(isolate, f, he));

        // Build the global `__hedit` object that will be exposed to builtin scripts
        Local<ObjectTemplate> obj = ObjectTemplate::New(isolate);
        SET("log", Log);
        SET("emit", EmitKeys);
        SET("command", Command);
        SET("registerCommand", RegisterCommand);
        SET("map", MapKeys);
        SET("set", SetOption);
        SET("switchMode", SwitchMode);
        Local<ObjectTemplate> builtin_global = ObjectTemplate::New(isolate);
        builtin_global->Set(v8_str("__hedit"), obj);

        // Create the contexts
        Local<Context> user_ctx = Context::New(isolate);
        Local<Context> builtin_ctx = Context::New(isolate, NULL, builtin_global);

        // Set the same access token to allow access between the two contexts
        Local<Symbol> token = Symbol::New(isolate, v8_str("security-token"));
        user_ctx->SetSecurityToken(token);
        builtin_ctx->SetSecurityToken(token);

        // Prepare the contexts to be reused later
        user_context.Reset(isolate, user_ctx);
        builtin_context.Reset(isolate, builtin_ctx);

        // Load the user config
        LoadUserConfig();
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
