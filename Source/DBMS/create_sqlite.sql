-- SQLite variant of create_pg.sql. Load bearing: INTEGER PRIMARY KEY aliases the
-- rowid (any other spelling stores NULL), timestamps are TEXT, and foreign keys
-- need PRAGMA foreign_keys = ON per connection.

-- children of ais_message must be dropped before it
DROP TABLE IF EXISTS ais_position;
DROP TABLE IF EXISTS ais_static;
DROP TABLE IF EXISTS ais_state;
DROP TABLE IF EXISTS ais_stats_hourly;
DROP TABLE IF EXISTS ais_message;

CREATE TABLE ais_message (
    id           INTEGER PRIMARY KEY,
    mmsi         INTEGER,
    received_at  TEXT,
    published_at TEXT DEFAULT CURRENT_TIMESTAMP,
    station_id   INTEGER,
    type         INTEGER,
    channel      TEXT,
    signal_level REAL,
    ppm          REAL,
    nmea         TEXT
);

CREATE TABLE ais_position (
    msg_id  INTEGER REFERENCES ais_message(id) ON DELETE CASCADE,
    lat     REAL,
    lon     REAL,
    speed   REAL,
    course  REAL,
    heading REAL,
    status  INTEGER,
    turn    REAL,
    alt     INTEGER
);

CREATE TABLE ais_static (
    msg_id       INTEGER REFERENCES ais_message(id) ON DELETE CASCADE,
    shipname     TEXT,
    callsign     TEXT,
    imo          INTEGER,
    shiptype     INTEGER,
    aid_type     INTEGER,
    to_bow       INTEGER,
    to_stern     INTEGER,
    to_port      INTEGER,
    to_starboard INTEGER,
    draught      REAL,
    destination  TEXT,
    eta          TEXT
);

CREATE TABLE ais_state (
    mmsi         INTEGER PRIMARY KEY,
    first_seen   TEXT,
    received_at  TEXT,
    station_id   INTEGER,
    signalpower  REAL,
    ppm          REAL,
    imo          INTEGER,
    callsign     TEXT,
    shipname     TEXT,
    shiptype     INTEGER,
    to_bow       INTEGER,
    to_stern     INTEGER,
    to_port      INTEGER,
    to_starboard INTEGER,
    eta          TEXT,
    draught      REAL,
    destination  TEXT,
    status       INTEGER,
    turn         REAL,
    speed        REAL,
    lat          REAL,
    lon          REAL,
    course       REAL,
    heading      REAL,
    aid_type     INTEGER,
    alt          INTEGER,
    count        INTEGER,
    msg_types    INTEGER,
    channels     INTEGER
);

CREATE TABLE ais_stats_hourly (
    station_id INTEGER,
    bucket     TEXT,
    msgs       INTEGER,
    vessels    INTEGER,
    channel_a  INTEGER,
    channel_b  INTEGER,
    channel_c  INTEGER,
    channel_d  INTEGER,
    level_min  REAL,
    level_max  REAL,
    ppm        REAL,
    PRIMARY KEY (station_id, bucket)
);

CREATE INDEX idx_message_mmsi_time ON ais_message (mmsi, received_at);
CREATE INDEX idx_message_time      ON ais_message (received_at);
CREATE INDEX idx_position_msg      ON ais_position (msg_id);
CREATE INDEX idx_static_msg        ON ais_static (msg_id);
