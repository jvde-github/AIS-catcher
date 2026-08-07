/*
	Copyright(c) 2021-2026 jvde.github@gmail.com et al.

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "MapTiles.h"
#include "Logger.h"

#include <fstream>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <stdexcept>


#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

namespace
{
    struct TileExt
    {
        const char *ext;
        const char *format;
        const char *mime;
    };

    const TileExt tile_exts[] = {
        {".png", "png", "image/png"},
        {".jpg", "jpg", "image/jpeg"},
        {".jpeg", "jpg", "image/jpeg"},
        {".pbf", "pbf", "application/x-protobuf"},
    };

    const char *mimeForFormat(const std::string &format)
    {
        if (format == "png")
            return "image/png";
        if (format == "jpg" || format == "jpeg")
            return "image/jpeg";
        if (format == "pbf")
            return "application/x-protobuf";
        return "application/octet-stream";
    }

    const TileExt *extInfo(const std::string &filename)
    {
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos)
            return nullptr;

        std::string ext = filename.substr(pos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        for (const TileExt &t : tile_exts)
            if (ext == t.ext)
                return &t;
        return nullptr;
    }
}

MapTiles::MapTiles() : minZoom(0), maxZoom(18)
{
    static int next_layer = 0;
    layerID = std::to_string(++next_layer);
}

bool MapTiles::isValidCoordinate(int z, int x, int y) const
{
    if (z < minZoom || z > maxZoom)
        return false;

    int maxTile = 1 << z;
    return x >= 0 && x < maxTile && y >= 0 && y < maxTile;
}

std::string MapTiles::pluginCode(bool overlay, const std::string &sourceOptions) const
{
    std::stringstream ss;

    ss << (overlay ? "addOverlayLayer" : "addTileLayer")
       << "(\"" << name << "\", new ol.layer.Tile({\n"
       << "    source: new ol.source.XYZ({\n"
       << "        url: '/tiles/" << layerID << "/{z}/{x}/{y}',\n"
       << "        attributions: '" << attribution << "',\n"
       << sourceOptions
       << "    })\n"
       << "}));\n";

    return ss.str();
}

#ifdef HASSQLITE
MBTilesSupport::MBTilesSupport() : db(nullptr) {}

MBTilesSupport::~MBTilesSupport()
{
    if (db)
        sqlite3_close(db);
}

void MBTilesSupport::loadMetadata()
{
    sqlite3_stmt *stmt;

    const char *zoomQuery = "SELECT DISTINCT zoom_level FROM tiles ORDER BY zoom_level";
    if (sqlite3_prepare_v2(db, zoomQuery, -1, &stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error("MBTILES: Failed to prepare zoom query.");

    while (sqlite3_step(stmt) == SQLITE_ROW)
        zoomMapping.push_back(sqlite3_column_int(stmt, 0));
    sqlite3_finalize(stmt);

    if (zoomMapping.empty())
        throw std::runtime_error("MBTILES: No zoom levels found in database.");

    minZoom = 0;
    maxZoom = (int)zoomMapping.size() - 1;

    const char *query = "SELECT name, value FROM metadata";
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error("MBTILES: Failed to prepare metadata query");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *key_text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        const char *value_text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));

        if (!key_text || !value_text)
            continue;

        std::string key = key_text;

        if (key == "name")
            name = value_text;
        else if (key == "attribution")
            attribution = value_text;
        else if (key == "format")
            format = value_text;
    }

    sqlite3_finalize(stmt);
}

bool MBTilesSupport::open(const std::string &filename)
{
    if (sqlite3_open_v2(filename.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
        return false;

    try
    {
        loadMetadata();
        return true;
    }
    catch (const std::exception &)
    {
        sqlite3_close(db);
        db = nullptr;
        return false;
    }
}

int MBTilesSupport::getMBTilesZoom(int olZoom) const
{
    if (olZoom < 0 || olZoom >= (int)zoomMapping.size())
        return -1;

    return zoomMapping[olZoom];
}

const std::vector<unsigned char> &MBTilesSupport::getTile(int z, int x, int y, std::string &contentType)
{
    contentType.clear();
    tileData.clear();

    int mbtilesZoom = getMBTilesZoom(z);
    if (mbtilesZoom == -1)
        return tileData;

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

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const void *data = sqlite3_column_blob(stmt, 0);
        int size = sqlite3_column_bytes(stmt, 0);

        tileData.resize(size);
        std::memcpy(tileData.data(), data, size);

        contentType = mimeForFormat(format);
    }

    sqlite3_finalize(stmt);
    return tileData;
}

std::string MBTilesSupport::generatePluginCode(bool overlay) const
{
    if (!db)
        return "";

    const double baseResolution = 156543.03392804097;
    std::stringstream ss;

    ss << "        tileGrid: new ol.tilegrid.TileGrid({\n"
       << "            extent: ol.proj.get('EPSG:3857').getExtent(),\n"
       << "            origin: ol.extent.getTopLeft(ol.proj.get('EPSG:3857').getExtent()),\n"
       << "            minZoom: " << minZoom << ",\n"
       << "            maxZoom: " << maxZoom << ",\n"
       << "            resolutions: [\n";

    for (size_t i = 0; i < zoomMapping.size(); i++)
        ss << "                " << (baseResolution / (1 << zoomMapping[i]))
           << (i + 1 < zoomMapping.size() ? ",\n" : "\n");

    ss << "            ],\n"
       << "            tileSize: [256, 256]\n"
       << "        })\n";

    return pluginCode(overlay, ss.str());
}
#endif

bool FileSystemTiles::isDirectory(const std::string &path) const
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat statbuf;
    return stat(path.c_str(), &statbuf) == 0 && S_ISDIR(statbuf.st_mode);
#endif
}

std::vector<std::string> FileSystemTiles::listDirectory(const std::string &path) const
{
    std::vector<std::string> entries;

#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (strcmp(findData.cFileName, ".") && strcmp(findData.cFileName, ".."))
                entries.push_back(findData.cFileName);
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
#else
    DIR *dir = opendir(path.c_str());
    if (dir)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
            if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
                entries.push_back(entry->d_name);
        closedir(dir);
    }
#endif
    return entries;
}

void FileSystemTiles::scanDirectory()
{
    availableZooms.clear();

    if (!isDirectory(basePath))
        throw std::runtime_error("FSTILES: Invalid directory path: " + basePath);

    for (const std::string &entry : listDirectory(basePath))
    {
        if (!isDirectory(basePath + "/" + entry))
            continue;

        try
        {
            int zoom = std::stoi(entry);
            if (zoom >= 0 && zoom <= 25)
                availableZooms.push_back(zoom);
        }
        catch (const std::exception &)
        {
            // Ignore non-numeric directories
        }
    }

    if (availableZooms.empty())
        throw std::runtime_error("FSTILES: No valid zoom directories found");

    std::sort(availableZooms.begin(), availableZooms.end());
    minZoom = availableZooms.front();
    maxZoom = availableZooms.back();

    size_t pos = basePath.find_last_of("/\\");
    name = pos != std::string::npos ? basePath.substr(pos + 1) : basePath;
    attribution = "Local tiles from " + name;

    detectFormat();
}

void FileSystemTiles::detectFormat()
{
    for (int zoom : availableZooms)
    {
        std::string zoomDir = basePath + "/" + std::to_string(zoom);

        for (const std::string &xEntry : listDirectory(zoomDir))
        {
            std::string xDir = zoomDir + "/" + xEntry;
            if (!isDirectory(xDir))
                continue;

            for (const std::string &yEntry : listDirectory(xDir))
            {
                if (isDirectory(xDir + "/" + yEntry))
                    continue;

                const TileExt *t = extInfo(yEntry);
                format = t ? t->format : "unknown";
                return;
            }
        }
    }
    format = "png";
}

bool FileSystemTiles::open(const std::string &directoryPath)
{
    basePath = directoryPath;

    try
    {
        scanDirectory();
        return true;
    }
    catch (const std::exception &)
    {
        return false;
    }
}

const std::vector<unsigned char> &FileSystemTiles::getTile(int z, int x, int y, std::string &contentType)
{
    contentType.clear();
    tileData.clear();

    if (!isValidCoordinate(z, x, y))
        return tileData;

    std::string base = basePath + "/" + std::to_string(z) + "/" +
                       std::to_string(x) + "/" + std::to_string(y);

    for (const TileExt &t : tile_exts)
    {
        std::ifstream file((base + t.ext).c_str(), std::ios::binary | std::ios::ate);
        if (!file.is_open())
            continue;

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        tileData.resize((size_t)size);
        if (file.read(reinterpret_cast<char *>(tileData.data()), size))
        {
            contentType = t.mime;
            return tileData;
        }

        tileData.clear();
    }

    return tileData;
}

std::string FileSystemTiles::generatePluginCode(bool overlay) const
{
    std::stringstream ss;

    ss << "        minZoom: " << minZoom << ",\n"
       << "        maxZoom: " << maxZoom << "\n";

    return pluginCode(overlay, ss.str());
}
