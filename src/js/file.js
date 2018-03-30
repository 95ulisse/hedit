/**
 * Functions to operate on the currently open file.
 * @module hedit/file
 */

import EventEmitter from 'hedit/private/eventemitter';

class File extends EventEmitter {

    /**
     * Returns a boolean indicating whether a file is currently open or not.
     * @alias module:hedit/file.isOpen
     * @type {boolean}
     * @readonly
     */
    get isOpen() {
        return __hedit.file_isOpen();
    }

    /**
     * Returns the name of the currently open file, or `null` if no file is open.
     * @alias module:hedit/file.name
     * @type {?string}
     * @readonly
     */
    get name() {
        return this.isOpen ? __hedit.file_name() : null;
    }

    /**
     * Returns the size in bytes of the currently open file, or `-1` if no file is open.
     * @alias module:hedit/file.size
     * @type {number}
     * @readonly
     */
    get size() {
        return this.isOpen ? __hedit.file_size() : -1;
    }

    /**
     * Returns a value indicating whether any modification to the file has been done
     * since it was open.
     * @alias module:hedit/file.isDirty
     * @type {boolean}
     * @readonly
     */
    get isDirty() {
        return this.isOpen && __hedit.file_isDirty();
    }

    /**
     * Reverts the last change applied to the currently open file.
     * @alias module:hedit/file.undo
     * @return {boolean} Returns `true` if the file changed, `false` otherwise.
     */
    undo() {
        return this.isOpen && __hedit.file_undo();
    }

    /**
     * Reapplies the last undone change to the currently open file.
     * @alias module:hedit/file.redo
     * @return {boolean} Returns `true` if the file changed, `false` otherwise.
     */
    redo() {
        return this.isOpen && __hedit.file_redo();
    }

    /**
     * Commits a new revision to the currently open file.
     *
     * A revision is a group of changes the can be undone together:
     * this means that you can perform multiple insertions and deletions
     * and commit them in a single atomically undoable revision.
     *
     * @alias module:hedit/file.commit
     * @return {boolean} Returns `true` if the commit succeeded.
     */
    commit() {
        return this.isOpen && __hedit.file_commit();
    }

    /**
     * Inserts new data into the currently open file.
     * @alias module:hedit/file.insert
     * @param {number} pos - Position of the insertion.
     * @param {string} data - Data to insert.
     * @return {boolean} Returns `true` if the insertion succeeded.
     */
    insert(pos, data) {
        return this.isOpen && __hedit.file_insert(0 + pos, data);
    }

    /**
     * Deletes a portion of the currently open file.
     * @alias module:hedit/file.delete
     * @param {number} pos - Index of the first byte to delete.
     * @param {number} len - How many bytes to delete.
     * @return {boolean} Returns `true` if the deletion succeeded.
     */
    delete(pos, len) {
        return this.isOpen && __hedit.file_delete(0 + pos, 0 + len);
    }

    /**
     * Reads a portion of the currently open file.
     * @alias module:hedit/file.read
     * @param {number} pos - Index of the first byte to read.
     * @param {number} len - How many bytes to read.
     * @return {ArrayBuffer} ArrayBuffer with the contents of the file.
     */
    read(pos, len) {
        if (!this.isOpen) {
            return null;
        } else {
            return __hedit.file_read(0 + pos, 0 + len);
        }
    }

};

const file = new File();

__hedit.registerEventBroker((name, ...args) => {
    if (name.indexOf('file_') == 0) {
        file.emit(name.substring(5).replace(/_([a-z])/g, m => m[1].toUpperCase()), ...args);
    }
});

/**
 * Event raised when a new file is opened by the user.
 * @event open
 */

/**
 * Event raised just before trying to the save the file to disk.
 * @event beforeWrite
 */

/**
 * Event raised when the file has been successfully written to disk.
 * @event write
 */

/**
 * Event raised when the open file is closed.
 * @event close
 */

/**
 * Event raised when the contents of the file change.
 * @event change
 * @param {integer} offset - Offset of the change.
 * @param {integer} len - Length of the change.
 */

export default file;
