"""Read native inline and streamed packages without changing the source database."""

import sqlite3


def open_packages(database, **options):
    connection = sqlite3.connect(database, **options)
    try:
        prepare_packages(connection)
    except sqlite3.Error:
        connection.close()
        raise
    return connection


def prepare_packages(connection):
    streamed = connection.execute(
        "SELECT 1 FROM main.sqlite_master WHERE type='table' AND name='package_bodies'"
    ).fetchone()
    if streamed:
        query = (
            "SELECT p.key,CASE WHEN b.rowid IS NULL THEN p.bytes ELSE b.bytes END AS bytes,"
            "p.crc,p.codec,p.native_bytes,"
            "CASE WHEN b.rowid IS NULL THEN p.rowid ELSE b.rowid END AS rowid,"
            "CASE WHEN b.rowid IS NULL THEN 'packages' ELSE 'package_bodies' END AS body_table,"
            "CASE WHEN b.rowid IS NULL THEN length(p.bytes) ELSE length(b.bytes) END AS stored_bytes "
            "FROM main.packages p LEFT JOIN main.package_bodies b ON b.key=p.key"
        )
    else:
        query = (
            "SELECT p.*,p.rowid AS rowid,'packages' AS body_table,length(p.bytes) AS stored_bytes "
            "FROM main.packages p"
        )
    connection.execute("CREATE TEMP VIEW IF NOT EXISTS native_packages AS " + query)
