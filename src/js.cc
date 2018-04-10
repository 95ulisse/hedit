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
#include "util/pubsub.h"

using namespace v8;


// Global V8 structures
static std::unique_ptr<Platform> platform;
static Isolate::CreateParams create_params;
static Isolate* isolate;

// Contexts for evaluating the modules
static Persistent<Context> builtin_context;
static Persistent<Context> user_context;

// Event registrations.
// We register a single global event handler to forward all the events to the js side.
static Subscription* native_pubsub_subscription;
static Global<Function> js_event_broker;

// JS function to guess the format of a file.
// All the file format implementation is pushed to the JS side.
static Global<Function> format_guess_function;



// Forward declarations
static inline Local<String> v8_str(const char* str);
static inline Local<String> v8_str(const char* str, size_t len);
static MaybeLocal<Module> EvalModule(const char* origin_str, const Persistent<Context>& ctx, const char* contents, size_t len);
static MaybeLocal<Module> EvalModule(Local<String> origin_str, const Persistent<Context>& ctx, const char* contents, size_t len);



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

Persistent<Module>& JsBuiltinModule::GetHandle() {
    return _module;
}

bool JsBuiltinModule::Eval(Isolate* isolate) {
    HandleScope handle_scope(isolate);

    if (!this->GetHandle().IsEmpty()) {
        return true;
    } else {
        MaybeLocal<Module> m = EvalModule(
            String::Concat(v8_str("builtin:"), v8_str(this->GetName().c_str())),
            builtin_context,
            this->GetSource(),
            this->GetSourceLen()
        );
        if (!m.IsEmpty()) {
            this->GetHandle().Reset(isolate, m.ToLocalChecked());
            return true;
        } else {
            return false;
        }
    }
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

static void NativePubSubHandler(PubSub* pubsub, const char* topic, void* data, void* user) {
    HEditEvent* ev = static_cast<HEditEvent*>(data);

    // Enter JS
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(user_context.Get(isolate));

    int argc;
    Local<v8::Value> argv[3];

    // Some events carry extra data that we may still want to pass to JS
    switch (ev->type) {
        
        case HEDIT_EVENT_TYPE_MODE_SWITCH: {
            HEditModeEvent* ev2 = reinterpret_cast<HEditModeEvent*>(ev);
            argc = 2;
            argv[0] = v8_str(topic);
            argv[1] = v8_str(ev2->new_mode->name);
            break;
        }
        
        case HEDIT_EVENT_TYPE_VIEW_SWITCH: {
            HEditViewEvent* ev2 = reinterpret_cast<HEditViewEvent*>(ev);
            argc = 2;
            argv[0] = v8_str(topic);
            argv[1] = v8_str(ev2->new_view->name);
            break;
        }
        
        case HEDIT_EVENT_TYPE_FILE_CHANGE: {
            HEditFileChangeEvent* ev2 = reinterpret_cast<HEditFileChangeEvent*>(ev);
            argc = 3;
            argv[0] = v8_str(topic);
            argv[1] = Integer::NewFromUnsigned(isolate, (uint32_t) ev2->offset);
            argv[2] = Integer::NewFromUnsigned(isolate, (uint32_t) ev2->len);
            break;
        }
        
        default:
            argc = 1;
            argv[0] = v8_str(topic);
            break;
            
    }

    // Invoke the JS broker
    TryCatch tt;
    if (js_event_broker.Get(isolate)->Call(user_context.Get(isolate), Null(isolate), argc, argv).IsEmpty()) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        log_error("Error during JS event dispatch: %s", c_str(str));
    }
    
}


// ----------------------------------------------------------------------------------------------------------


// __hedit.require("module");
static void Require(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();

    assert(args.Length() == 1);

    String::Utf8Value name(isolate, args[0]);

    // Here we take a shortcut and skip some checks, since the `__hedit` object
    // is visible only to the builtin modules

    std::shared_ptr<JsBuiltinModule> builtin = JsBuiltinModule::FromName(std::string(*name));
    if (builtin == nullptr) {
        isolate->ThrowException(String::Concat(v8_str("Cannot resolve module "), args[0]->ToString(ctx).ToLocalChecked()));
        return;
    }

    if (!builtin->Eval(isolate)) {
        isolate->ThrowException(String::Concat(v8_str("Error evaluating module "), args[0]->ToString(ctx).ToLocalChecked()));
        return;
    }

    args.GetReturnValue().Set(builtin->GetHandle().Get(isolate)->GetModuleNamespace());

}

// __hedit.registerEventBroker(function (eventName, ...args) {});
static void RegisterEventBroker(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);

    assert(args.Length() == 1);
    assert(args[0]->IsFunction());

    Global<Function> f(isolate, Local<Function>::Cast(args[0]));
    js_event_broker = std::move(f);
}

// __hedit.registerFormatGuessFunction(f);
static void RegisterFormatGuessFunction(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);

    assert(args.Length() == 1);
    assert(args[0]->IsFunction());

    format_guess_function = Global<Function>(isolate, Local<Function>::Cast(args[0]));
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

static TickitPen* ParsePen(Local<Context> ctx, Local<Object> obj, const char* name) {
    HandleScope handle_scope(ctx->GetIsolate());
    
    Local<Object> desc = Local<Object>::Cast(obj->Get(ctx, v8_str(name)).ToLocalChecked());

    TickitPen* pen = tickit_pen_new();
    if (pen == NULL) {
        log_fatal("Out of memory.");
        return NULL;
    }

    int fg = desc->Get(ctx, v8_str("fg")).ToLocalChecked()->Int32Value(ctx).FromJust();
    int bg = desc->Get(ctx, v8_str("bg")).ToLocalChecked()->Int32Value(ctx).FromJust();
    bool bold = desc->Get(ctx, v8_str("bold")).ToLocalChecked()->BooleanValue(ctx).FromJust();
    bool under = desc->Get(ctx, v8_str("under")).ToLocalChecked()->BooleanValue(ctx).FromJust();

    tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, fg);
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, bg);
    tickit_pen_set_bool_attr(pen, TICKIT_PEN_BOLD, bold);
    tickit_pen_set_bool_attr(pen, TICKIT_PEN_UNDER, under);

    return pen;
}

// __hedit.setTheme({ pen1: { ... pen attrs ... }, ... });
static void SetTheme(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 1);
    assert(args[0]->IsObject());

    Theme* theme = (Theme*) malloc(sizeof(Theme));
    if (theme == NULL) {
        log_fatal("Out of memory.");
        return;
    }

#define P(name) \
    do { \
        TickitPen* pen = ParsePen(ctx, obj, #name); \
        if (pen == NULL) { \
            return; \
        } \
        theme->name = pen; \
    } while (false);

    Local<Object> obj = Local<Object>::Cast(args[0]);
    P(text);
    P(linenos);
    P(error);
    P(block_cursor);
    P(soft_cursor);
    P(statusbar);
    P(commandbar);
    P(log_debug);
    P(log_info);
    P(log_warn);
    P(log_error);
    P(log_fatal);
    P(white);
    P(gray);
    P(blue);
    P(red);
    P(pink);
    P(green);
    P(purple);
    P(orange);

    hedit_switch_theme(hedit, theme);
}

// __hedit.mode();
static void GetMode(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 0);

    args.GetReturnValue().Set(v8_str(hedit->mode->name));
}

// __hedit.view();
static void GetView(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 0);

    args.GetReturnValue().Set(v8_str(hedit->view->name));
}

// __hedit.emitKeys("keys");
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

static void JsCommandFree(HEdit* hedit, void* user) {
    
    // Enter the js user context
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Local<Context> ctx = user_context.Get(isolate);
    Context::Scope context_scope(ctx);

    Persistent<Function>* handler = static_cast<Persistent<Function>*>(user);
    handler->Reset();
    delete handler;
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

    bool res = hedit_command_register(hedit, *command_name, JsCommandHandler, JsCommandFree, handler);
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

// __hedit.get("name");
static void GetOption(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 1);

    String::Utf8Value name(isolate, args[0]);

    // Retrive the option
    Option* opt = (Option*) map_get(hedit->options, *name);
    if (opt == NULL) {
        isolate->ThrowException(
            String::Concat(v8_str("Unknown option: "), args[0]->ToString(ctx).ToLocalChecked())
        );
        return;
    }

    // Native options have a simple primitive type
    Local<v8::Value> ret;
    switch (opt->type) {
        case HEDIT_OPTION_TYPE_INT:
            ret = Integer::New(isolate, opt->value.i);
            break;
        case HEDIT_OPTION_TYPE_BOOL:
            ret = Boolean::New(isolate, opt->value.b);
            break;
        case HEDIT_OPTION_TYPE_STRING:
            ret = String::NewFromUtf8(isolate, opt->value.str);
            break;
        default:
            abort();
    }

    args.GetReturnValue().Set(ret);
}

static bool JsOptionHandler(HEdit* hedit, Option* opt, const ::Value* v, void* user) {
    auto handler = (Persistent<Function>*) user;
    
    // Enter the js user context
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Local<Context> ctx = user_context.Get(isolate);
    Context::Scope context_scope(ctx);

    // Delegate to the JS handler
    TryCatch tt;
    Local<v8::Value> ret;
    Local<v8::Value> args[] = { v8_str(v->str) };
    MaybeLocal<v8::Value> maybeRet = handler->Get(isolate)->Call(ctx, Null(isolate), 1, args);
    if (!maybeRet.ToLocal(&ret)) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        log_error("Exception during JS option callback: %s", c_str(str));
        return false;
    }

    return ret->BooleanValue(ctx).FromMaybe(false);
}

static void JsOptionFree(HEdit* hedit, Option* opt, void* user) {
    
    // Enter the js user context
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Local<Context> ctx = user_context.Get(isolate);
    Context::Scope context_scope(ctx);

    auto handler = (Persistent<Function>*) user;
    handler->Reset();
    delete handler;
}

// __hedit.registerOption("name", "defaultValue", handler);
static void RegisterOption(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 3);

    if (!args[2]->IsFunction()) {
        isolate->ThrowException(v8_str("Expected function."));
        return;
    }

    String::Utf8Value name(isolate, args[0]);
    String::Utf8Value defaultValue(isolate, args[1]);
    Persistent<Function>* handler = new Persistent<Function>(isolate, Local<Function>::Cast(args[2]));

    ::Value v = { 0, false, *defaultValue };
    bool res = hedit_option_register(hedit, *name, HEDIT_OPTION_TYPE_STRING, v, JsOptionHandler, JsOptionFree, handler);
    args.GetReturnValue().Set(res);

    if (!res) {
        handler->Reset();
        delete handler;
    }
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

// __hedit.file_isOpen();
static void FileIsOpen(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 0);

    args.GetReturnValue().Set(hedit->file != NULL);
}

// __hedit.file_name();
static void FileName(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 0);
    assert(hedit->file != NULL);

    const char* fname = hedit_file_name(hedit->file);
    args.GetReturnValue().Set(fname != NULL ? (Local<v8::Value>) v8_str(fname) : (Local<v8::Value>) Null(isolate));
}

// __hedit.file_size();
static void FileSize(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 0);
    assert(hedit->file != NULL);

    args.GetReturnValue().Set((double) hedit_file_size(hedit->file));
}

// __hedit.file_isDirty();
static void FileIsDirty(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 0);
    assert(hedit->file != NULL);

    args.GetReturnValue().Set(hedit_file_is_dirty(hedit->file));
}

// __hedit.file_undo();
static void FileUndo(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 0);
    assert(hedit->file != NULL);

    size_t unused;
    bool res = hedit_file_undo(hedit->file, &unused);
    args.GetReturnValue().Set(res);

    if (res) {
        hedit_redraw_view(hedit);
    }
}

// __hedit.file_redo();
static void FileRedo(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 0);
    assert(hedit->file != NULL);

    size_t unused;
    bool res = hedit_file_redo(hedit->file, &unused);
    args.GetReturnValue().Set(res);

    if (res) {
        hedit_redraw_view(hedit);
    }
}

// __hedit.file_commit();
static void FileCommit(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 0);
    assert(hedit->file != NULL);

    args.GetReturnValue().Set(hedit_file_commit_revision(hedit->file));
}

// __hedit.file_insert(offset, data);
static void FileInsert(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 2);
    assert(hedit->file != NULL);

    size_t offset = args[0]->IntegerValue(ctx).FromJust();
    String::Utf8Value data(isolate, args[1]);

    bool res = hedit_file_insert(hedit->file, offset, (const unsigned char*) *data, data.length());
    args.GetReturnValue().Set(res);

    if (res) {
        hedit_redraw_view(hedit);
    }
}

// __hedit.file_delete(offset, len);
static void FileDelete(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 2);
    assert(hedit->file != NULL);

    size_t offset = args[0]->IntegerValue(ctx).FromJust();
    size_t len = args[1]->IntegerValue(ctx).FromJust();

    bool res = hedit_file_delete(hedit->file, offset, len);
    args.GetReturnValue().Set(res);

    if (res) {
        hedit_redraw_view(hedit);
    }
}

// __hedit.file_setFormat(format);
static void FileSetFormat(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 1);

    if (hedit->file == NULL) {
        return;
    }
    
    if (!args[0]->IsObject()) {
        isolate->ThrowException(v8_str("Expected object."));
        return;
    }

    Local<Object> obj = Local<Object>::Cast(args[0]);
    if (!obj->Has(ctx, Symbol::GetIterator(isolate)).FromJust()) {
        isolate->ThrowException(v8_str("Expected iterable object."));
        return;
    }

    JsFormat* format = new JsFormat(isolate, ctx, obj);
    hedit_set_format(hedit, format);
}

// __hedit.file_read(offset, len);
static void FileRead(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 2);
    assert(hedit->file != NULL);

    size_t offset = args[0]->IntegerValue(ctx).FromJust();
    size_t len = args[1]->IntegerValue(ctx).FromJust();

    // Clamp the length so that it does not exceed the actual length of the file
    if (offset + len > hedit_file_size(hedit->file)) {
        len -= offset + len - hedit_file_size(hedit->file);
    }

    // Allocate an ArrayBuffer to hold the results
    Local<ArrayBuffer> buf = ArrayBuffer::New(isolate, len);
    char* dest = (char*) buf->GetContents().Data();

    // Read the file and copy to the arraybuffer
    FileIterator* it = hedit_file_iter(hedit->file, offset, len);
    const unsigned char* chunk;
    size_t chunk_len;
    size_t off = 0;
    while (hedit_file_iter_next(it, &chunk, &chunk_len)) {
        memcpy(dest + off, chunk, chunk_len);
        off += chunk_len;
    }
    hedit_file_iter_free(it);

    args.GetReturnValue().Set(buf);
}

// __hedit.statusbar_showMessage(msg, sticky);
static void StatusbarShowMessage(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 2);

    String::Utf8Value message(isolate, args[0]);
    bool sticky = args[1]->BooleanValue(ctx).FromJust();

    hedit_statusbar_show_message(hedit->statusbar, sticky, *message);
}

// __hedit.statusbar_hideMessage();
static void StatusbarHideMessage(const FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    HandleScope handle_scope(isolate);
    HEdit* hedit = (HEdit*) Local<External>::Cast(args.Data())->Value();

    assert(args.Length() == 0);

    hedit_statusbar_show_message(hedit->statusbar, false, NULL);
}


// ----------------------------------------------------------------------------------------------------------


static MaybeLocal<Module> ModuleResolveCallback(Local<Context> ctx, Local<String> specifier, Local<Module> referrer) {
    EscapableHandleScope handle_scope(isolate);

    // A module can import only builtins
    String::Utf8Value s(isolate, specifier);
    std::shared_ptr<JsBuiltinModule> builtin = JsBuiltinModule::FromName(std::string(c_str(s)));
    if (builtin == nullptr) {
        isolate->ThrowException(String::Concat(v8_str("Cannot resolve module "), specifier));
        return MaybeLocal<Module>();
    }

    // Builtin modules inside the `private` folder can be accessed only from other builtin modules
    if (builtin->GetName().find("hedit/private/", 0) == 0) {
        if (ctx != builtin_context.Get(isolate)) {
            isolate->ThrowException(v8_str("Private modules can be requested only by builtin code."));
            return MaybeLocal<Module>();
        }
    }

    // Evaluate and return the module
    if (builtin->Eval(isolate)) {
        return MaybeLocal<Module>(handle_scope.Escape(builtin->GetHandle().Get(isolate)));
    } else {
        return MaybeLocal<Module>();
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
        SET("require", Require);
        SET("registerEventBroker", RegisterEventBroker);
        SET("registerFormatGuessFunction", RegisterFormatGuessFunction);
        SET("log", Log);
        SET("setTheme", SetTheme);
        SET("mode", GetMode);
        SET("view", GetView);
        SET("emitKeys", EmitKeys);
        SET("command", Command);
        SET("registerCommand", RegisterCommand);
        SET("map", MapKeys);
        SET("set", SetOption);
        SET("get", GetOption);
        SET("registerOption", RegisterOption);
        SET("switchMode", SwitchMode);
        SET("file_isOpen", FileIsOpen);
        SET("file_name", FileName);
        SET("file_size", FileSize);
        SET("file_isDirty", FileIsDirty);
        SET("file_undo", FileUndo);
        SET("file_redo", FileRedo);
        SET("file_commit", FileCommit);
        SET("file_insert", FileInsert);
        SET("file_delete", FileDelete);
        SET("file_setFormat", FileSetFormat);
        SET("file_read", FileRead);
        SET("statusbar_showMessage", StatusbarShowMessage);
        SET("statusbar_hideMessage", StatusbarHideMessage);
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

        // Register an interest for all the topics in the default pubsub context
        native_pubsub_subscription = pubsub_register(pubsub_default(), "hedit/*", NativePubSubHandler, NULL);
        if (native_pubsub_subscription == NULL) {
            log_fatal("Cannot register native pubsub handler.");
            return false;
        }

        // Execute the builtin initializer
        if (!JsBuiltinModule::FromName("hedit/private/__init")->Eval(isolate)) {
            abort();
        }

        // Load the user config
        LoadUserConfig();
    }

    return true;
}

void hedit_js_teardown(HEdit* hedit) {
    log_debug("V8 teardown.");

    // Remove event handlers and brokers
    pubsub_unregister(native_pubsub_subscription);
    js_event_broker.Reset();
    format_guess_function.Reset();

    // Dispose the contexts
    builtin_context.Reset();
    user_context.Reset();

    // Tear down V8
    isolate->Dispose();
    V8::Dispose();
    V8::ShutdownPlatform();
    delete create_params.array_buffer_allocator;
}

void hedit_set_format(HEdit* hedit, Format* format) {
    if (hedit->format != NULL) {
        delete hedit->format;
    }
    hedit->format = format;
    hedit_redraw(hedit);
}

void hedit_format_guess(HEdit* hedit) {

    assert(hedit->file != NULL);    

    // Enter JS
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(user_context.Get(isolate));

    TryCatch tt;
    Local<v8::Value> ret;
    if (!format_guess_function.Get(isolate)->Call(user_context.Get(isolate), Null(isolate), 0, {}).ToLocal(&ret)) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        log_error("Exception during JS format guessing: %s", c_str(str));
        hedit_option_set(hedit, "format", "none");
        return;
    }

    // The JS function returns a string with the name of the format to use:
    // use that name to set the value of the `format` option.
    String::Utf8Value name(isolate, ret);
    hedit_option_set(hedit, "format", *name);

}

void hedit_format_free(Format* format) {
    delete format;
}

FormatIterator* hedit_format_iter(Format* format) {
    
    // Enter JS
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(user_context.Get(isolate));

    return format->Iterator();

}

FormatSegment* hedit_format_iter_seek(FormatIterator* it, size_t pos) {

    // Enter JS
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(user_context.Get(isolate));

    return it->Seek(pos);

}

FormatSegment* hedit_format_iter_current(FormatIterator* it) {

    // Enter JS
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(user_context.Get(isolate));

    return it->Current();

}

FormatSegment* hedit_format_iter_next(FormatIterator* it) {

    // Enter JS
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(user_context.Get(isolate));

    return it->Next();

}

void hedit_format_iter_free(FormatIterator* it) {
    delete it;
}

JsFormatIterator* JsFormat::Iterator() {
    HandleScope handle_scope(_isolate);

    Local<Context> ctx = _ctx.Get(_isolate);
    Local<Object> obj = _obj.Get(_isolate);

    // An iterable object has the @@iterator method that constructs an iterator
    Local<Symbol> sym = Symbol::GetIterator(_isolate);
    Local<v8::Value> iteratorConstructor;
    if (!obj->Get(ctx, sym).ToLocal(&iteratorConstructor)) {
        log_fatal("Invalid iterator.");
        return NULL;
    }
    if (!iteratorConstructor->IsFunction()) {
        log_fatal("Invalid iterator.");
        return NULL;
    }

    TryCatch tt;
    Local<v8::Value> iterator;
    if (!Local<Function>::Cast(iteratorConstructor)->Call(ctx, obj, 0, {}).ToLocal(&iterator)) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        log_fatal("Invalid iterator: %s", c_str(str));
        return NULL;
    }
    
    if (!iterator->IsObject()) {
        log_fatal("Invalid iterator.");
        return NULL;
    }

    return new JsFormatIterator(_isolate, Local<Object>::Cast(iterator));
}

JsFormatIterator::JsFormatIterator(Isolate* isolate, Local<Object> jsIterator)
        : _isolate(isolate),
          _jsIterator(isolate, jsIterator)
{
    _current.name = (const char*) &_currentName;

    HandleScope handle_scope(isolate);
    Local<Context> ctx = isolate->GetCurrentContext();

    // Cache the `next` function
    Local<v8::Value> nextFunction = jsIterator->Get(ctx, v8_str("next")).ToLocalChecked();
    _nextFunction.Reset(_isolate, Local<Function>::Cast(nextFunction));

    // Cache the `seek` function
    Local<v8::Value> seekFunction = jsIterator->Get(ctx, v8_str("seek")).ToLocalChecked();
    _seekFunction.Reset(_isolate, Local<Function>::Cast(seekFunction));
}

void JsFormatIterator::UnpackJsSegment(Local<Object> seg) {
    EscapableHandleScope handle_scope(_isolate);
    Local<Context> ctx = _isolate->GetCurrentContext();
    
    // Start unpacking the other properties of the JS object in the native structure
    String::Utf8Value name(isolate, seg->Get(ctx, v8_str("name")).ToLocalChecked());
    strncpy((char*) &_currentName, *name, MAX_SEGMENT_NAME_LEN);
    _currentName[MAX_SEGMENT_NAME_LEN - 1] = '\0';
    _current.from = (size_t) seg->Get(ctx, v8_str("from")).ToLocalChecked()->IntegerValue(ctx).FromJust();
    _current.to = (size_t) seg->Get(ctx, v8_str("to")).ToLocalChecked()->IntegerValue(ctx).FromJust();
    _current.color = (int) seg->Get(ctx, v8_str("color")).ToLocalChecked()->IntegerValue(ctx).FromJust();

    _initialized = true;
}

bool JsFormatIterator::AdvanceIterator() {
    EscapableHandleScope handle_scope(_isolate);
    Local<Context> ctx = _isolate->GetCurrentContext();

    Local<Object> iterator = _jsIterator.Get(_isolate);
    Local<Function> nextFunction = _nextFunction.Get(_isolate);

    // Call the `next()` method of the JS iterator
    TryCatch tt;
    Local<v8::Value> res;
    if (!nextFunction->Call(ctx, iterator, 0, {}).ToLocal(&res)) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        log_fatal("Invalid iterator: %s", c_str(str));
        return false;
    }
    if (!res->IsObject()) {
        log_fatal("Invalid iterator.");
        return false;
    }

    // The returned object has two properties: `value` and `done`

    Local<Object> resobj = Local<Object>::Cast(res);
    bool done = resobj->Get(ctx, v8_str("done")).ToLocalChecked()->BooleanValue(ctx).FromJust();
    if (done) {
        _done = true;
        return false;
    }

    Local<v8::Value> val = resobj->Get(ctx, v8_str("value")).ToLocalChecked();
    UnpackJsSegment(Local<Object>::Cast(val));
    
    return true;
}

FormatSegment* JsFormatIterator::Seek(size_t pos) {

    // Return NULL if we reached the end
    if (_done) {
        return NULL;
    }

    HandleScope handle_scope(_isolate);
    Local<Context> ctx = _isolate->GetCurrentContext();
    
    Local<Object> iterator = _jsIterator.Get(_isolate);
    Local<Function> seekFunction = _seekFunction.Get(_isolate);
    Local<v8::Value> args[] = {
        Integer::NewFromUnsigned(_isolate, (uint32_t) pos)
    };
    
    // Call the `seek()` method of the JS iterator
    TryCatch tt;
    Local<v8::Value> res;
    if (!seekFunction->Call(ctx, iterator, 1, args).ToLocal(&res)) {
        Local<v8::Value> ex = tt.Exception();
        String::Utf8Value str(isolate, ex);
        log_fatal("Invalid iterator: %s", c_str(str));
        return NULL;
    }

    // The `seek()` method is expected to return a js segment, or null
    if (res->IsNull()) {
        _done = true;
        return NULL;
    }
    UnpackJsSegment(Local<Object>::Cast(res));

    return &_current;
    
}

FormatSegment* JsFormatIterator::Next() {

    // Return NULL if we reached the end
    if (_done) {
        return NULL;
    }

    // Advance the JS iterator
    return this->AdvanceIterator() ? &_current : NULL;

}

FormatSegment* JsFormatIterator::Current() {

    // Return NULL if we reached the end
    if (_done) {
        return NULL;
    }

    // Call `Next()` if the iterator has never been initialized
    if (!_initialized) {
        return this->Next();
    }

    return &_current;
}
