// The viewer's configuration from the server, one object for the session: the
// updater (features/server-config) writes a newer revision into it in place.
import { initialServerConfig } from '../features/server-config/server-config.js';

export const config = initialServerConfig();
