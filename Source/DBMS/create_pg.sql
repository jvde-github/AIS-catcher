-- tables from the previous schema revision
DROP TABLE IF EXISTS ais_nmea;
DROP TABLE IF EXISTS ais_basestation;
DROP TABLE IF EXISTS ais_aton;
DROP TABLE IF EXISTS ais_sar_position;
DROP TABLE IF EXISTS ais_vessel_pos;
DROP TABLE IF EXISTS ais_vessel_static;
DROP TABLE IF EXISTS ais_property;
DROP TABLE IF EXISTS ais_vessel;
DROP TABLE IF EXISTS ais_keys;

-- every table referencing ais_message must be dropped before it
DROP TABLE IF EXISTS ais_position;
DROP TABLE IF EXISTS ais_static;
DROP TABLE IF EXISTS ais_state;
DROP TABLE IF EXISTS ais_stats_hourly;
DROP TABLE IF EXISTS ais_message;

-- one row per received message; written when position, static or nmea is on
CREATE TABLE ais_message (
    id           bigserial primary key,
    mmsi         integer,
    received_at  timestamptz,
    published_at timestamptz DEFAULT current_timestamp,
    station_id   smallint,
    type         smallint,
    channel      character(1),
    signal_level real,
    ppm          real,
    nmea         text
);

-- dynamic payload: types 1, 2, 3, 4, 9, 18, 19, 21, 27
CREATE TABLE ais_position (
    msg_id  bigint references ais_message(id) ON DELETE CASCADE,
    lat     real,
    lon     real,
    speed   real,
    course  real,
    heading real,
    status  smallint,
    turn    real,
    alt     smallint
);

-- static payload: types 5, 19, 21, 24; an aton's name lands in shipname
CREATE TABLE ais_static (
    msg_id       bigint references ais_message(id) ON DELETE CASCADE,
    shipname     varchar(20),
    callsign     varchar(7),
    imo          integer,
    shiptype     smallint,
    aid_type     smallint,
    to_bow       smallint,
    to_stern     smallint,
    to_port      smallint,
    to_starboard smallint,
    draught      real,
    destination  varchar(20),
    eta          varchar(12)
);

-- one row per MMSI: latest known values plus counters
CREATE TABLE ais_state (
    mmsi         integer primary key,
    first_seen   timestamptz,
    received_at  timestamptz,
    station_id   smallint,
    signalpower  real,
    ppm          real,
    imo          integer,
    callsign     varchar(7),
    shipname     varchar(20),
    shiptype     smallint,
    to_bow       smallint,
    to_stern     smallint,
    to_port      smallint,
    to_starboard smallint,
    eta          varchar(12),
    draught      real,
    destination  varchar(20),
    status       smallint,
    turn         real,
    speed        real,
    lat          real,
    lon          real,
    course       real,
    heading      real,
    aid_type     smallint,
    alt          smallint,
    count        integer,
    msg_types    integer,
    channels     smallint
);

-- hourly reception statistics, independent of message logging
CREATE TABLE ais_stats_hourly (
    station_id   smallint,
    bucket       timestamptz,
    msgs         integer,
    vessels      integer,
    channel_a    integer,
    channel_b    integer,
    channel_c    integer,
    channel_d    integer,
    level_min    real,
    level_max    real,
    ppm          real,
    PRIMARY KEY (station_id, bucket)
);

CREATE INDEX idx_message_mmsi_time ON ais_message (mmsi, received_at);
CREATE INDEX idx_message_time      ON ais_message (received_at);
CREATE INDEX idx_position_msg      ON ais_position (msg_id);
CREATE INDEX idx_static_msg        ON ais_static (msg_id);
