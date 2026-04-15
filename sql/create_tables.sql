-- Run this file connected to the 'postgres' default database.
-- After the database is created, switch to it before running the CREATE TABLE.

CREATE TABLE IF NOT EXISTS day_ahead_prices (
    time_utc   TIMESTAMPTZ NOT NULL,
    price_area TEXT        NOT NULL,
    price_dkk  FLOAT8,

    PRIMARY KEY (time_utc, price_area)
);
