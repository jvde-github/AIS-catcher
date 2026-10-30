// What the pages, the wizard and the configuration layer reach the app by. The app fills these in
// at start; until then they do nothing.
export const hub = {
    // the session expired: the login asks for the password again, the edits on the page stay
    authRequired() {},
    // the wizard was cancelled while no password is set: the login is the backstop
    passwordBackstop() {},
    // the wizard set the password: signed in
    authGranted() {},
    // a save the receiver takes at its next start
    configSaved() {},
    // the receiver's status as last read (its port, its address, what listens), once the read under way is in
    status: () => Promise.resolve({}),
    // the status read afresh
    refreshStatus: () => Promise.resolve({})
};
