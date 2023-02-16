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
    type integer,
    sender integer,
    channel character(1),
    signal_level float,
    ppm float
);

CREATE TABLE ais_nmea (
    id integer references ais_message(id),
    nmea varchar(80)
);

CREATE TABLE ais_basestation (
    id integer references ais_message(id),
    mmsi integer,
    timestamp integer,
    lat float,
    lon float
);

CREATE TABLE ais_sar_position (
    id integer references ais_message(id),
    mmsi integer,
    timestamp integer,
    alt integer,
    speed integer,
    lat float,
    lon float,
    course integer
);

CREATE TABLE ais_aton (
    id integer references ais_message(id),
    mmsi integer,
    timestamp integer,
    aid_type integer,
    name varchar(20),
    lon float,
    lat float,
    to_bow integer,
    to_stern integer,
    to_port integer,
    to_starboard integer
);

CREATE TABLE ais_vessel_pos (
    id integer references ais_message(id),
    mmsi integer,
    timestamp integer,
    status integer,
    turn float,
    speed float,
    lat float,
    lon float,
    course float,
    heading float
);

CREATE TABLE ais_vessel_static (
    id integer references ais_message(id),
    mmsi integer,
    timestamp integer,
    imo integer,
    callsign varchar(7),
    shipname varchar(20),
    shiptype integer,
    to_port integer,
    to_bow integer,
    to_stern integer,
    to_starboard integer,
    eta varchar(12),
    draught float,
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


INSERT INTO ais_keys (key_str) VALUES ('lat'),('lon'),('destination'),('heading'),('course'),('speed'),('shiptype'),('to_bow'),('to_stern'),('to_starboard'),('to_port');
INSERT INTO ais_keys (key_str) VALUES ('status'),('draught'),('callsign'),('shipname'),('alt');

CREATE INDEX ais_property_id_key_idx ON ais_property (id, key);