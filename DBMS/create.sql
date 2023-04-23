DROP TABLE IF EXISTS ais_nmea;
DROP TABLE IF EXISTS ais_basestation;
DROP TABLE IF EXISTS ais_aton;
DROP TABLE IF EXISTS ais_sar_position;
DROP TABLE IF EXISTS ais_vessel_pos;
DROP TABLE IF EXISTS ais_vessel_static;
DROP TABLE IF EXISTS ais_property;
DROP TABLE IF EXISTS ais_keys;
DROP TABLE IF EXISTS ais_message;
DROP TABLE IF EXISTS _id;

CREATE TABLE ais_message (
    id serial primary key,
    mmsi integer,
    received_at timestamp,
    published_at timestamp,
    station_id smallint,
    type smallint,
    channel character(1),
    signal_level real,
    ppm real
);

CREATE TABLE _id (
    id integer
);

CREATE TABLE ais_nmea (
    mmsi integer,
    received_at timestamp,
    station_id smallint,
    msg_id integer references ais_message(id) ON DELETE SET NULL,
    nmea varchar(80)
);

CREATE TABLE ais_basestation (
    mmsi integer,
    received_at timestamp,
    station_id smallint,
    msg_id integer references ais_message(id) ON DELETE SET NULL,
    lat real,
    lon real
);

CREATE TABLE ais_sar_position (
    mmsi integer,
    received_at timestamp,
    station_id smallint,
    msg_id integer references ais_message(id) ON DELETE SET NULL,
    alt smallint,
    speed smallint,
    lat real,
    lon real,
    course smallint
);

CREATE TABLE ais_aton (
    mmsi integer,
    received_at timestamp,
    station_id smallint,
    msg_id integer references ais_message(id) ON DELETE SET NULL,
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
    mmsi integer,
    received_at timestamp,
    station_id smallint,
    msg_id integer references ais_message(id) ON DELETE SET NULL,
    status smallint,
    turn real,
    speed real,
    lat real,
    lon real,
    course real,
    heading real
);

CREATE TABLE ais_vessel_static (
    mmsi integer,
    received_at timestamp,
    station_id smallint,
    msg_id integer references ais_message(id) ON DELETE SET NULL,
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
    msg_id integer references ais_message(id),
    key integer references ais_keys(key_id),
    value varchar(20)
);


/*
INSERT INTO ais_keys (key_str) VALUES ('destination');
*/
