// The search panel in the header: what opening it steps aside for, and what a
// picked result opens.

import { createSearch } from '@aiscatcher/ui/search.js';
import { hasValidCoords } from '@aiscatcher/core/geo.js';
import { settings } from '../../core/state.js';
import { ships as shipsDB } from '../../core/store.js';
import { getShipName, getCallSign } from '../../core/names.js';
import { ol, map } from '../../map.js';
import { focus } from '../../data.js';
import { showNotification } from '../../dialog.js';
import { setPanels } from '../../panels.js';
import { selectMapTab } from '../../tabs/index.js';
import { ui } from '../../ui.js';
import { showTargetcard, closeTargetcard } from '../targetcard/card.js';
import { openFocus } from '../follow/follow.js';
import * as mapObjects from '../mapobjects/mapobjects.js';
import { createLocalSearch } from './search.js';
import { register } from '../../actions.js';

export const searchPanel = createSearch({
    root: document.getElementById('search-panel'),
    scrim: document.getElementById('search-scrim'),
    triggers: [document.getElementById('header-search-button')],
    storageKey: 'viewerSearchType',
    types: [
        { id: 'all', label: 'Everything', placeholder: 'Search vessels, stations and places…' },
        { id: 'ships', label: 'Vessels', placeholder: 'Name, MMSI, callsign or IMO…' },
        { id: 'stations', label: 'Stations', placeholder: 'Station name or identifier…' },
        { id: 'ports', label: 'Places', placeholder: 'Place name or UN/LOCODE…' },
    ],
    search: createLocalSearch({focus: () => focus, ships: () => shipsDB, shipName: getShipName, callSign: getCallSign}),
    onOpen: () => {
        if (settings.tab !== 'map') selectMapTab();
        ui.dismiss();
        closeTargetcard();
        setPanels({table: false, measure: false});
    },
    onSelect: async result => {
        if (result.type === 'ship') {
            await openFocus(result.id);
        } else {
            if (hasValidCoords(result.lat, result.lon)) {
                map.getView().animate({center: ol.proj.fromLonLat([result.lon, result.lat]), zoom: 12, duration: 500});
            }
            if (result.type === 'port' || result.type === 'place') mapObjects.openPorts([result.object]);
            else if (result.type === 'station') showTargetcard('station', Number(result.id));
        }
    },
    onError: () => showNotification('Could not open the search result', 'error'),
});

export function closeSearch() {
    searchPanel.close();
}

register({
    toggleSearch: () => searchPanel.toggle(),
});
