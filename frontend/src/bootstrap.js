import {loadInitialServerConfig} from './features/server-config/server-config.js';

async function start() {
    await loadInitialServerConfig();
    await import('./script.js');
}
start();
