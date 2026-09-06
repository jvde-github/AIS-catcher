import * as units from '../../shared/core/units.js';
import { settings } from './state.js';

export const u = units.create({
    system: () => settings.metric,
    coordinates: () => settings.coordinate_format,
});

export const {
    getDistanceConversion, getDistanceVal, getDistanceUnit,
    getSpeedConversion, getSpeedVal, getSpeedUnit,
    getDimVal, getDimUnit, getDraughtVal, getShipDimension,
    formatCoordinate, getLatValFormat, getLonValFormat, getChangeVal,
} = u;
