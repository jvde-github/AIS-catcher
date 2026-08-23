let shipNameOverride = null;
let callSignOverride = null;

export function setShipNameProvider(fn) { shipNameOverride = fn; }
export function setCallSignProvider(fn) { callSignOverride = fn; }

export const getShipName = (ship) => {
    if (shipNameOverride) {
        const r = shipNameOverride(ship);
        if (r != null) return r;
    }
    return ship.shipname;
};

export const getCallSign = (ship) => {
    if (callSignOverride) {
        const r = callSignOverride(ship);
        if (r != null) return r;
    }
    return ship.callsign;
};
