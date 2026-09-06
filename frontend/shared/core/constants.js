// Numeric values double as map keys in settings.track_class_colors —
// renumbering breaks saved settings.
export const ShippingClass = {
    OTHER: 0,
    UNKNOWN: 1,
    CARGO: 2,
    B: 3,
    PASSENGER: 4,
    SPECIAL: 5,
    TANKER: 6,
    HIGHSPEED: 7,
    FISHING: 8,
    PLANE: 9,
    HELICOPTER: 10,
    STATION: 11,
    ATON: 12,
    SARTEPIRB: 13,
};

// ship.mmsi_type discriminator.
export const CLASS_A = 1;
export const CLASS_B = 2;
export const BASESTATION = 3;
export const SAR = 4;
export const SARTEPIRB = 5;
export const ATON = 6;
