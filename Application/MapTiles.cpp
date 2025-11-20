#include "MapTiles.h"

#include <string>
#include <vector>
#include <utility>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <stdexcept>

#include "Logger.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

// MapTiles base
MapTiles::MapTiles() : minZoom(0), maxZoom(18)
{
    layerID = std::to_string((uintptr_t)this);
}

bool MapTiles::isValidCoordinate(int z, int x, int y) const
{
    if (z < minZoom || z > maxZoom)
    {
        return false;
    }

    int maxTile = 1 << z;
    return (x >= 0 && x < maxTile && y >= 0 && y < maxTile);
}

// MBTilesSupport implementation
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

bool MBTilesSupport::open(const std::string &filename)
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

bool MBTilesSupport::isValidTile(int z, int x, int y) const
{
    return true;
}

int MBTilesSupport::getMBTilesZoom(int olZoom) const
{
    if (olZoom < 0 || olZoom >= zoomMapping.size())
        return -1;

    return zoomMapping[olZoom];
}

const std::vector<unsigned char> &MBTilesSupport::getTile(int z, int x, int y, std::string &contentType)
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

std::string MBTilesSupport::generatePluginCode(bool overlay) const
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
#endif

// FileSystemTiles implementation
bool FileSystemTiles::pathExists(const std::string &path) const
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES;
#else
    struct stat statbuf;
    return stat(path.c_str(), &statbuf) == 0;
#endif
}

bool FileSystemTiles::isDirectory(const std::string &path) const
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path.c_str());
    return (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) != 0)
        return false;
    return S_ISDIR(statbuf.st_mode);
#endif
}

std::string FileSystemTiles::getFilename(const std::string &path) const
{
    size_t pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

std::string FileSystemTiles::getExtension(const std::string &filename) const
{
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos)
    {
        std::string ext = filename.substr(pos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext;
    }
    return "";
}

std::vector<std::string> FileSystemTiles::listDirectory(const std::string &path) const
{
    std::vector<std::string> entries;

#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    std::string searchPath = path + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0)
            {
                entries.push_back(findData.cFileName);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
#else
    DIR *dir = opendir(path.c_str());
    if (dir)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
            {
                entries.push_back(entry->d_name);
            }
        }
        closedir(dir);
    }
#endif
    return entries;
}

void FileSystemTiles::scanDirectory()
{
    availableZooms.clear();

    if (!pathExists(basePath) || !isDirectory(basePath))
    {
        throw std::runtime_error("FSTILES: Invalid directory path: " + basePath);
    }

    std::vector<std::string> entries = listDirectory(basePath);
    for (const std::string &entry : entries)
    {
        std::string fullPath = basePath + "/" + entry;
        if (isDirectory(fullPath))
        {
            try
            {
                int zoom = std::stoi(entry);
                if (zoom >= 0 && zoom <= 25)
                {
                    availableZooms.push_back(zoom);
                }
            }
            catch (const std::exception &)
            {
                // Ignore non-numeric directories
            }
        }
    }

    if (availableZooms.empty())
    {
        throw std::runtime_error("FSTILES: No valid zoom directories found");
    }

    std::sort(availableZooms.begin(), availableZooms.end());
    minZoom = availableZooms.front();
    maxZoom = availableZooms.back();

    name = getFilename(basePath);
    attribution = "Local tiles from " + name;

    detectFormat();
}

void FileSystemTiles::detectFormat()
{
    for (int zoom : availableZooms)
    {
        std::string zoomDir = basePath + "/" + std::to_string(zoom);
        std::vector<std::string> xEntries = listDirectory(zoomDir);

        for (const std::string &xEntry : xEntries)
        {
            std::string xDir = zoomDir + "/" + xEntry;
            if (isDirectory(xDir))
            {
                std::vector<std::string> yEntries = listDirectory(xDir);

                for (const std::string &yEntry : yEntries)
                {
                    std::string yPath = xDir + "/" + yEntry;
                    if (pathExists(yPath) && !isDirectory(yPath))
                    {
                        std::string ext = getExtension(yEntry);

                        if (ext == ".png")
                            format = "png";
                        else if (ext == ".jpg" || ext == ".jpeg")
                            format = "jpg";
                        else if (ext == ".pbf")
                            format = "pbf";
                        else
                            format = "unknown";

                        return;
                    }
                }
            }
        }
    }
    format = "png"; // Default fallback
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

bool FileSystemTiles::isValidTile(int z, int x, int y) const
{
    if (!isValidCoordinate(z, x, y))
        return false;

    std::string tilePath = basePath + "/" + std::to_string(z) + "/" +
                            std::to_string(x) + "/" + std::to_string(y);

    // Check for common tile extensions
    std::vector<std::string> extensions;
    extensions.push_back(".png");
    extensions.push_back(".jpg");
    extensions.push_back(".jpeg");
    extensions.push_back(".pbf");

    for (const std::string &ext : extensions)
    {
        if (pathExists(tilePath + ext))
            return true;
    }
    return false;
}

const std::vector<unsigned char> &FileSystemTiles::getTile(int z, int x, int y, std::string &contentType)
{
    contentType.clear();
    tileData.clear();

    if (!isValidCoordinate(z, x, y))
        return tileData;

    std::string baseTilePath = basePath + "/" + std::to_string(z) + "/" +
                                std::to_string(x) + "/" + std::to_string(y);

    // Try different extensions
    std::vector<std::pair<std::string, std::string>> extensions;
    extensions.push_back(std::make_pair(".png", "image/png"));
    extensions.push_back(std::make_pair(".jpg", "image/jpeg"));
    extensions.push_back(std::make_pair(".jpeg", "image/jpeg"));
    extensions.push_back(std::make_pair(".pbf", "application/x-protobuf"));

    for (const auto &extPair : extensions)
    {
        std::string tilePath = baseTilePath + extPair.first;

        if (pathExists(tilePath))
        {
            std::ifstream file(tilePath.c_str(), std::ios::binary);
            if (!file.is_open())
                continue;

            file.seekg(0, std::ios::end);
            std::streamsize fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            tileData.resize(fileSize);
            if (file.read(reinterpret_cast<char *>(tileData.data()), fileSize))
            {
                contentType = extPair.second;
                break;
            }
        }
    }

    return tileData;
}

std::string FileSystemTiles::generatePluginCode(bool overlay) const
{
    std::stringstream ss;

    if (overlay)
        ss << "addOverlayLayer";
    else
        ss << "addTileLayer";

    ss << "(\"" << name << "\", new ol.layer.Tile({\n"
        << "    source: new ol.source.XYZ({\n"
        << "        url: '/tiles/" << layerID << "/{z}/{x}/{y}',\n"
        << "        attributions: '" << attribution << "',\n"
        << "        minZoom: " << minZoom << ",\n"
        << "        maxZoom: " << maxZoom << "\n"
        << "    })\n"
        << "}));\n";

    return ss.str();
}
