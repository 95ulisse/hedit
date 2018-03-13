#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "core.h"
#include "commands.h"
#include "file.h"
#include "format.h"
#include "util/log.h"
#include "util/map.h"
#include "util/event.h"


struct ArgIterator {
    char* ptr;
};

/** Type of the command handlers. */
typedef struct {
    bool (*handler)(HEdit*, bool force, ArgIterator* args, void* user);
    void (*free)(HEdit*, void* user);
    void* user;
} Command;


// Forward declarations
static void it_init(ArgIterator* args, char* ptr);


static bool quit(HEdit* hedit, bool force, ArgIterator* args, void* user) {
    
    // Do not exit if there's a dirty file open
    if (hedit->file != NULL && hedit_file_is_dirty(hedit->file) && !force) {
        log_error("There are unsaved changes. Save your changes with :write, or use :quit! to exit discarding changes.");
        return false;
    }

    tickit_stop(hedit->tickit);
    return true;
}

static bool edit(HEdit* hedit, bool force, ArgIterator* args, void* user) {

    if (hedit->file != NULL && !force) {
        log_error("Another file is already opened.");
        return false;
    }

    const char* path = it_next(args);
    if (path == NULL) {
        log_error(":edit requires path of file to open.");
        return false;
    }

    File* f = hedit_file_open(path);
    if (f == NULL) {
        return false;
    }

    if (hedit->file != NULL) {
        event_fire(&hedit->ev_file_close, hedit, hedit->file);
        hedit_file_close(hedit->file);
    }

    hedit->file = f;
    hedit_format_guess(hedit);
    event_fire(&hedit->ev_file_open, hedit, hedit->file);

    hedit_switch_view(hedit, HEDIT_VIEW_EDIT);

    return true;
}

static bool new(HEdit* hedit, bool force, ArgIterator* args, void* user) {

    if (hedit->file != NULL && !force) {
        log_error("Another file is already opened.");
        return false;
    }

    File* f = hedit_file_open(NULL);
    if (f == NULL) {
        return false;
    }

    // Close the exiting file (if any)
    if (hedit->file != NULL) {
        event_fire(&hedit->ev_file_close, hedit, hedit->file);
        hedit_file_close(hedit->file);
    }

    hedit->file = f;
    hedit_format_guess(hedit);
    event_fire(&hedit->ev_file_open, hedit, hedit->file);

    hedit_switch_view(hedit, HEDIT_VIEW_EDIT);

    return true;
}

static bool close(HEdit* hedit, bool force, ArgIterator* args, void* user) {

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

static bool write(HEdit* hedit, bool force, ArgIterator* args, void* user) {

    if (hedit->file == NULL) {
        log_error("No file open.");
        return false;
    }

    // Optionally save to a different path
    const char* name = it_next(args);
    if (name == NULL) {
        name = hedit_file_name(hedit->file);
    }

    if (name == NULL) {
        log_error("Missing file name.");
        return false;
    }

    event_fire(&hedit->ev_file_before_write, hedit, hedit->file);
    bool res = hedit_file_save(hedit->file, name, SAVE_MODE_AUTO);
    if (res) {
        event_fire(&hedit->ev_file_write, hedit, hedit->file);
    }

    return res;

}

static bool wq(HEdit* hedit, bool force, ArgIterator* args, void* user) {
    ArgIterator empty = { 0 };
    return write(hedit, force, args, user)
        && quit(hedit, force, &empty, user);
}

static bool set(HEdit* hedit, bool force, ArgIterator* args, void* user) {
    
    // Option name
    const char* name = it_next(args);
    if (!name) {
        log_error("Option name required. Usage: set option [value]");
        return false;
    }

    // Value might be optional
    const char* value = it_next(args);

    return hedit_option_set(hedit, name, value);

}

static bool map(HEdit* hedit, bool force, ArgIterator* args, void* user) {

    // Mode name
    const char* modename = it_next(args);
    if (!modename) {
        log_error("Usage: map <mode> <from> <to>");
        return false;
    }
    Mode* mode = hedit_mode_from_name(modename);
    if (!mode) {
        log_error("Unknown mode: %s.", modename);
        return false;
    }

    // From key
    const char* from = it_next(args);
    if (!from) {
        log_error("Usage: map <mode> <from> <to>");
        return false;
    }

    // To key
    const char* to = it_next(args);
    if (!to) {
        log_error("Usage: map <mode> <from> <to>");
        return false;
    }

    return hedit_map_keys(hedit, mode->id, from, to, force);
}

static bool logview(HEdit* hedit, bool force, ArgIterator* args, void* user) {
    hedit_switch_view(hedit, HEDIT_VIEW_LOG);
    return true;
}



// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------



bool hedit_command_register(HEdit* hedit, const char* name,
                            bool (*cb)(HEdit*, bool, ArgIterator*, void* user),
                            void (*free_f)(HEdit*, void* user), void* user)
{

    Command* cmd = malloc(sizeof(Command));
    if (cmd == NULL) {
        log_fatal("Out of memory.");
        return false;
    }
    cmd->handler = cb;
    cmd->free = free_f;
    cmd->user = user;

    if (!map_put(hedit->commands, name, cmd)) {
        if (errno != EEXIST) {
            log_error("Cannot register command %s.", name);
        }
        free(cmd);
        return false;
    }

    return true;
}

static bool free_command(const char* key, void* value, void* data) {
    Command* cmd = value;
    if (cmd->free != NULL) {
        cmd->free((HEdit*) data, cmd->user);
    }
    free(cmd);
    return true;
}

void hedit_command_free_all(HEdit* hedit) {
    map_iterate(hedit->commands, free_command, hedit);
}

bool hedit_init_commands(HEdit* hedit) {

    // Allocate the map
    hedit->commands = map_new();
    if (!hedit->commands) {
        log_fatal("Out of memory.");
        return false;
    }

#define REG(c) hedit_command_register(hedit, #c, c, NULL, NULL);
#define REG2(c, alias) REG(c); hedit_command_register(hedit, #alias, c, NULL, NULL);

    // Register the single commands.
    REG2(quit, q);
    REG2(edit, e);
    REG(close);
    REG(new);
    REG2(write, w);
    REG(wq);
    REG(set);
    REG(map);
    hedit_command_register(hedit, "log", logview, NULL, NULL);

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
    char* cmd = (char*) it_next(args);
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
    const Map* map = map_prefix(hedit->commands, command_name);
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

    Command* cmd = map_first(map, NULL);
    log_debug("Executing command %s.", name);
    return cmd->handler(hedit, force, &args, cmd->user);

}



// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------



static void it_init(ArgIterator* args, char* ptr) {
    args->ptr = ptr;
}

const char* it_next(ArgIterator* args) {

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
    const char* base = ptr;
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