#ifndef __JS_H__
#define __JS_H__

#include <stdbool.h>

#include "core.h"

#ifdef __cplusplus

#include <v8.h>

class JsBuiltinModule {
public:
    JsBuiltinModule(std::string name, const unsigned char* source, unsigned int source_len);

    const std::string& GetName();
    const char* GetSource();
    size_t GetSourceLen();
    v8::Persistent<v8::Module>& GetModule();

    static JsBuiltinModule* FromName(std::string name);

private:
    const std::string _name;
    const char* _source;
    size_t _source_len;
    v8::Persistent<v8::Module> _module;

    // This is the map of all the builtin modules.
    // It is filled at build time by generating another .cc file with the initializer.
    // See `scripts/gen-js.sh` for more details.
    static std::map<std::string, JsBuiltinModule*> _all_modules;
};

#endif


// -------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

/** Initializer for V8. This must be called exactly once. */
bool hedit_js_init(int argc, char** argv);

/** Releases all the resources held by V8. This must be called exactly once. */
void hedit_js_teardown();

/** Loads and executes (if present) the user-specific configuration files. */
void hedit_js_load_user_config(HEdit*);

#ifdef __cplusplus
}
#endif

#endif
