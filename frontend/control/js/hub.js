// Hooks into the app, which fills them in at start; until then they do nothing.
export const hub = {
    // the session expired: the login asks for the password again, the edits on the page stay
    authRequired() {},
    // the wizard was cancelled while no password is set: the login is the backstop
    passwordBackstop() {},
    // the wizard set the password: signed in
    authGranted() {},
    // the status as last read, once the read under way is in
    status: () => Promise.resolve({}),
    refreshStatus: () => Promise.resolve({})
};
