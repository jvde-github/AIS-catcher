import {loadInitialServerConfig} from './features/server-config.js';

async function start() {
    await loadInitialServerConfig();
    await import('./script.js');
}
start();
