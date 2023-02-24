DROP TABLE IF EXISTS ais_nmea;
DROP TABLE IF EXISTS ais_basestation;
DROP TABLE IF EXISTS ais_aton;
DROP TABLE IF EXISTS ais_sar_position;
DROP TABLE IF EXISTS ais_vessel_pos;
DROP TABLE IF EXISTS ais_vessel_static;
DROP TABLE IF EXISTS ais_property;
DROP TABLE IF EXISTS ais_keys;
DROP TABLE IF EXISTS ais_message;

CREATE TABLE ais_message (
    id serial primary key,
    mmsi integer,
    timestamp integer,
    type smallint,
    sender smallint,
    channel character(1),
    signal_level real,
    ppm real
);

CREATE TABLE ais_nmea (
    id integer references ais_message(id),
    nmea varchar(80)
);

CREATE TABLE ais_basestation (
    id integer references ais_message(id),
    mmsi integer,
    timestamp integer,
    lat real,
    lon real
);

CREATE TABLE ais_sar_position (
    id integer references ais_message(id),
    mmsi integer,
    timestamp integer,
    alt smallint,
    speed smallint,
    lat real,
    lon real,
    course smallint
);

CREATE TABLE ais_aton (
    id integer references ais_message(id),
    mmsi integer,
    timestamp integer,
    aid_type smallint,
    name varchar(20),
    lon real,
    lat real,
    to_bow smallint,
    to_stern smallint,
    to_port smallint,
    to_starboard smallint
);

CREATE TABLE ais_vessel_pos (
    id integer references ais_message(id),
    mmsi integer,
    timestamp integer,
    status smallint,
    turn real,
    speed real,
    lat real,
    lon real,
    course real,
    heading real
);

CREATE TABLE ais_vessel_static (
    id integer references ais_message(id),
    mmsi integer,
    timestamp integer,
    imo integer,
    callsign varchar(7),
    shipname varchar(20),
    shiptype smallint,
    to_port smallint,
    to_bow smallint,
    to_stern smallint,
    to_starboard smallint,
    eta varchar(12),
    draught real,
    destination varchar(20)
);

CREATE TABLE ais_keys (
    key_id serial primary key,
    key_str varchar(20)
);

CREATE TABLE ais_property (
    id integer references ais_message(id),
    key integer references ais_keys(key_id),
    value varchar(20)
);


INSERT INTO ais_keys (key_str) VALUES ('destination');

CREATE INDEX ais_property_id_key_idx ON ais_property (id, key);
