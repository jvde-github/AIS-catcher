#pragma once

#include <string>
#include <vector>
#include <utility>

#include <sqlite3.h>
#include <sstream>
#include <cstdint>

class MapTiles
{
protected:
    std::string name;
    std::string attribution;
    std::string layerID;
    std::string format;

    std::vector<unsigned char> tileData;

    int minZoom;
    int maxZoom;

public:
    MapTiles() : minZoom(0), maxZoom(18)
    {
        layerID = std::to_string((uintptr_t)this);
    }
    virtual ~MapTiles() = default;

    virtual bool open(const std::string &source) = 0;
    virtual bool isValidTile(int z, int x, int y) const { return false; };
    virtual const std::vector<unsigned char> &getTile(int z, int x, int y, std::string &s) { s.clear(); tileData.clear(); return tileData; };

    virtual std::string generatePluginCode(bool overlay) { return ""; };

    std::string getName() const { return name; }
    std::string getAttribution() const { return attribution; }
    int getMinZoom() const { return minZoom; }
    int getMaxZoom() const { return maxZoom; }
    std::string getFormat() const { return format; }
    std::string getLayerID() const { return layerID; }

    bool isValidCoordinate(int z, int x, int y) const
    {
        if (z < minZoom || z > maxZoom)
        {
            return false;
        }

        int maxTile = 1 << z;
        return (x >= 0 && x < maxTile && y >= 0 && y < maxTile);
    }
};

class MBTilesSupport : public MapTiles
{
#ifdef HASSQLITE
private:
    sqlite3 *db = nullptr;
    std::vector<int> zoomMapping;

    void loadMetadata()
    {
        sqlite3_stmt *stmt;

        const char *zoomQuery = "SELECT DISTINCT zoom_level FROM tiles ORDER BY zoom_level";
        if (sqlite3_prepare_v2(db, zoomQuery, -1, &stmt, nullptr) != SQLITE_OK)
        {
            throw std::runtime_error("MBTILES: Failed to prepare zoom query.");
        }

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            zoomMapping.push_back(sqlite3_column_int(stmt, 0));
        }
        sqlite3_finalize(stmt);

        if (zoomMapping.empty())
        {
            throw std::runtime_error("MBTILES: No zoom levels found in database.");
        }

        minZoom = 0;
        maxZoom = zoomMapping.size() - 1;

        const char *query = "SELECT name, value FROM metadata";
        if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK)
        {
            throw std::runtime_error("MBTILES: Failed to prepare metadata query");
        }

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            std::string key = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            std::string value = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));

            if (key == "name")
                name = value;
            else if (key == "attribution")
                attribution = value;
            else if (key == "format")
                format = value;
        }

        sqlite3_finalize(stmt);
    }

public:
    MBTilesSupport() : db(nullptr) {}

    ~MBTilesSupport() override
    {
        if (db)
            sqlite3_close(db);
    }

    bool open(const std::string &filename) override
    {
        if (sqlite3_open_v2(filename.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
        {
            return false;
        }

        try
        {
            loadMetadata();
            return true;
        }
        catch (const std::exception &e)
        {
            if (db)
            {
                sqlite3_close(db);
                db = nullptr;
            }
            return false;
        }
    }

    bool isValidTile(int z, int x, int y) const override
    {
        return true;
    }

    int getMBTilesZoom(int olZoom) const
    {
        if (olZoom < 0 || olZoom >= zoomMapping.size())
            return -1;

        return zoomMapping[olZoom];
    }
    const std::vector<unsigned char> &getTile(int z, int x, int y, std::string &contentType) override
    {
        contentType.clear();
        tileData.clear();

        int mbtilesZoom = getMBTilesZoom(z);
        if (mbtilesZoom == -1)
        {
            return tileData;
        }

        int tmsY = (1 << mbtilesZoom) - 1 - y;

        sqlite3_stmt *stmt;
        const char *query = "SELECT tile_data FROM tiles WHERE zoom_level = ? AND tile_column = ? AND tile_row = ?";

        if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK)
        {
            Error() << "MBTILES: Failed to prepare SQL statement: " << sqlite3_errmsg(db);
            return tileData;
        }

        sqlite3_bind_int(stmt, 1, mbtilesZoom);
        sqlite3_bind_int(stmt, 2, x);
        sqlite3_bind_int(stmt, 3, tmsY);

        int result = sqlite3_step(stmt);
        if (result == SQLITE_ROW)
        {
            const void *data = sqlite3_column_blob(stmt, 0);
            int size = sqlite3_column_bytes(stmt, 0);

            tileData.resize(size);
            std::memcpy(tileData.data(), data, size);

            contentType = format == "png" ? "image/png" : format == "jpg" || format == "jpeg" ? "image/jpeg"
                                                      : format == "pbf"                       ? "application/x-protobuf"
                                                                                              : "application/octet-stream";
        }

        sqlite3_finalize(stmt);
        return tileData;
    }

    virtual std::string generatePluginCode(bool overlay) const
    {
        if (!db)
            return "";

        std::stringstream ss;

        if (overlay)
            ss << "addOverlayLayer";
        else
            ss << "addTileLayer";

        ss << "(\"" << name << "\", new ol.layer.Tile({\n"
           << "    source: new ol.source.XYZ({\n"
           << "        url: '/tiles/" << layerID << "/{z}/{x}/{y}',\n"
           << "        attributions: '" << attribution << "',\n"
           << "        tileGrid: new ol.tilegrid.TileGrid({\n"
           << "            extent: ol.proj.get('EPSG:3857').getExtent(),\n"
           << "            origin: ol.extent.getTopLeft(ol.proj.get('EPSG:3857').getExtent()),\n"
           << "            minZoom: " << minZoom << ",\n"
           << "            maxZoom: " << maxZoom << ",\n"
           << "            resolutions: [\n";

        double baseResolution = 156543.03392804097;

        for (size_t i = 0; i < zoomMapping.size(); i++)
        {
            ss << "                " << (baseResolution / (1 << zoomMapping[i]));
            if (i < zoomMapping.size() - 1)
                ss << ",";
            ss << "\n";
        }

        ss << "            ],\n"
           << "            tileSize: [256, 256]\n"
           << "        })\n"
           << "    })\n"
           << "}));\n";

        return ss.str();
    }
#else
public:
    bool open(const std::string &filename) override
    {
        throw std::runtime_error("MBTiles not supported, please compile with SQLITE support.");
    }

#endif
};