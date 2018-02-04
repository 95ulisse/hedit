import EventEmitter from 'hedit/private/eventemitter';

let file = new EventEmitter();
Object.assign(file, {
    get isOpen() {
        return __hedit.file_isOpen();
    },
    get name() {
        return file.isOpen ? __hedit.file_name() : null;
    },
    get size() {
        return file.isOpen ? __hedit.file_size() : -1;
    },
    get isDirty() {
        return file.isOpen && __hedit.file_isDirty();
    },
    undo() {
        return file.isOpen && __hedit.file_undo();
    },
    redo() {
        return file.isOpen && __hedit.file_redo();
    },
    commit() {
        return file.isOpen && __hedit.file_commit();
    },
    insert(pos, data) {
        return file.isOpen && __hedit.file_insert(0 + pos, data);
    },
    delete(pos, len) {
        return file.isOpen && __hedit.file_delete(0 + pos, 0 + len);
    }
});

__hedit.registerEventBroker((name, ...args) => {
    if (name.indexOf('file_') == 0) {
        file.emit(name.substring(5), ...args);
    }
});

export default file;