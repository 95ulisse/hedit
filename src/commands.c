#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "core.h"
#include "commands.h"
#include "file.h"
#include "util/log.h"
#include "util/map.h"
#include "util/event.h"

/** Iterator over the arguments of a command. */
typedef struct {
    char* ptr;
} ArgIterator;
static void it_init(ArgIterator*, char*);
static char* it_next(ArgIterator*);

/** Type of all the functions in the map. */
typedef bool (*CommandCallback)(HEdit*, bool force, ArgIterator* args);

/** Map of all the default builtin commands. */
Map* hedit_commands;


static bool quit(HEdit* hedit, bool force, ArgIterator* args) {
    
    // Do not exit if there's a dirty file open
    if (hedit->file != NULL && hedit_file_is_dirty(hedit->file) && !force) {
        log_error("There are unsaved changes. Save your changes with :write, or use :quit! to exit discarding changes.");
        return false;
    }

    tickit_stop(hedit->tickit);
    return true;
}

static bool open(HEdit* hedit, bool force, ArgIterator* args) {

    if (hedit->file != NULL) {
        log_error("Another file is already opened.");
        return false;
    }

    char* path = it_next(args);
    if (path == NULL) {
        log_error(":open requires path of file to open.");
        return false;
    }

    File* f = hedit_file_open(path);
    if (f == NULL) {
        return false;
    }

    hedit->file = f;
    event_fire(&hedit->ev_file_open, hedit, hedit->file);

    hedit_switch_view(hedit, HEDIT_VIEW_EDIT);

    return true;
}

static bool close(HEdit* hedit, bool force, ArgIterator* args) {

    if (hedit->file == NULL) {
        log_error("No file open.");
        return false;
    }

    if (hedit_file_is_dirty(hedit->file) && !force) {
        log_error("There are unsaved changes. Save your changes with :write, or use :close! to discard changes.");
        return false;
    }

    event_fire(&hedit->ev_file_close, hedit, hedit->file);

    hedit_file_close(hedit->file);
    hedit->file = NULL;

    hedit_switch_view(hedit, HEDIT_VIEW_SPLASH);

    return true;

}

static bool write(HEdit* hedit, bool force, ArgIterator* args) {

    if (hedit->file == NULL) {
        log_error("No file open.");
        return false;
    }

    // Optionally set a new name
    char* name = it_next(args);
    if (name != NULL) {
        if (!hedit_file_set_name(hedit->file, name)) {
            return false;
        }
    }

    event_fire(&hedit->ev_file_beforewrite, hedit, hedit->file);
    bool res = hedit_file_save(hedit->file);
    event_fire(&hedit->ev_file_write, hedit, hedit->file);

    return res;

}

static bool w(HEdit* hedit, bool force, ArgIterator* args) {
    return write(hedit, force, args);
}

static bool wq(HEdit* hedit, bool force, ArgIterator* args) {
    ArgIterator empty = { 0 };
    return w(hedit, force, args)
        && quit(hedit, force, &empty);
}



// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------



bool hedit_init_commands() {

    // Allocate the map
    hedit_commands = map_new();
    if (!hedit_commands) {
        log_fatal("Out of memory.");
        return false;
    }

#define REG(c) \
    if (!map_put(hedit_commands, #c, (void*) c)) { \
        log_fatal("Cannot register command " #c "."); \
        return false; \
    }

    // Register the single commands.
    // Ignores the function pointer <-> void* cast warning.
#pragma GCC diagnostic ignored "-Wpedantic"
    REG(quit);
    REG(open);
    REG(close);
    REG(w);
    REG(write);
    REG(wq);
#pragma GCC diagnostic warning "-Wpedantic"

    return true;

}

struct check_unique_params {
    int count;
    char names[256];
    size_t names_size;
};

static bool check_unique(const char* key, void* value, void* user) {
    struct check_unique_params* params = user;
    
    params->count++;

    // Append the name to the array of ambiguous matches
    char* ptr = params->names + params->names_size;
    size_t key_len = strlen(key);
    if (params->names_size + key_len + 3 > 256) { // 2 for ',' and ' ' + 1 for '\0'
        return false;
    }
    *(ptr++) = ',';
    *(ptr++) = ' ';
    memcpy(ptr, key, key_len);
    ptr[key_len] = '\0';
    params->names_size += 2 + key_len;

    return true;

}

static bool parse_command_line(char* line, char** command_name, bool* force, ArgIterator* args) {

    // The expected format is:
    // command[!] arg1 arg2 "compound arg" compound\ arg2
    // So here we can treat the command name as the first "argument".

    it_init(args, line);
    char* cmd = it_next(args);
    if (cmd == NULL) {
        return false;
    }

    *command_name = cmd;

    // If the command name ends in a "!", it is forced
    size_t cmdlen = strlen(cmd);
    if (cmd[cmdlen - 1] == '!') {
        *force = true;
        cmd[cmdlen - 1] = '\0';
    }

    return true;

}

bool hedit_command_exec(HEdit* hedit, char* str) {

    // Parse the command line to separate the command from its arguments
    char* command_name = NULL;
    bool force = false;
    ArgIterator args = { 0 };
    if (!parse_command_line(str, &command_name, &force, &args)) {
        return false;
    }

    // Look for the command in the map
    const Map* map = map_prefix(hedit_commands, command_name);
    if (map_empty(map)) {
        log_error("Command %s not registered.", command_name);
        return false;
    }

    // If there's an exact match, do not look for further expansions
    const char* name;
    map_first(map, &name);
    if (strcmp(name, command_name) != 0) {

        struct check_unique_params params = { 0 };
        map_iterate(map, check_unique, &params);
        if (params.count > 1) {
            log_error("Ambiguous match. Possible commands: %s", params.names + 2);
            return false;
        }

    }

    CommandCallback cmd;
    *(void **)&(cmd) = map_first(map, NULL); // Trick to avoid warning for function <-> void* cast
    log_debug("Executing command %s.", name);
    return cmd(hedit, force, &args);

}



// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------



static void it_init(ArgIterator* args, char* ptr) {
    args->ptr = ptr;
}

static char* it_next(ArgIterator* args) {

    if (args == NULL || args->ptr == NULL || args->ptr[0] == '\0') {
        return NULL;
    }

    // Skip any initial whitespace
    char* ptr = args->ptr;
    while (isspace(*ptr)) {
        ptr++;
    }

    if (*ptr == '\0') {
        args->ptr = ptr;
        return NULL;
    }

    // Double quotes act like escape that allow capturing spaces
    char* base = ptr;
    if (*ptr == '"') {

        do {
            ptr++;
        } while (*ptr != '"' && *ptr != '\0');

        if (*ptr == '\0') {
            // End of the string
            args->ptr = ptr;
            return base + 1;
        } else {
            // There's still something more to parse
            *ptr = '\0';
            args->ptr = ptr + 1;
            return base + 1;
        }

    } else {
        
        while (!isspace(*ptr) && *ptr != '\0') {
            ptr++;
        }

        if (*ptr == '\0') {
            // End of the string
            args->ptr = ptr;
            return base;
        } else {
            // There's still something more to parse
            *ptr = '\0';
            args->ptr = ptr + 1;
            return base;
        }

    }

}