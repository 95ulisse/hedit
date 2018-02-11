/**
 * General set of functions to programmatically interact with the statusbar.
 * @module hedit/statusbar
 */

export default {
    
    /**
     * Shows a message on the statusbar.
     * @param {string} message - Message to show.
     * @param {boolean} [sticky = false] - `true` if the message should persist between mode changes.
     * @see [hideMessage]{@link module:hedit/statusbar.hideMessage} to hide a sticky message.
     *
     * @example
     * statusbar.showMessage("Hello world!");
     */
    showMessage(message, sticky = false) {
        __hedit.statusbar_showMessage(message, sticky);
    },

    /**
     * Hides the currently visible message.
     */
    hideMessage() {
        __hedit.statusbar_hideMessage();
    }

};