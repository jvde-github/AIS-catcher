#pragma once

#include <string>
#include <vector>

#ifdef HASSQLITE
#include <cstdint>
#include <sqlite3.h>
#endif

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
    MapTiles();
    virtual ~MapTiles() = default;

    virtual bool open(const std::string &source) = 0;
    virtual bool isValidTile(int z, int x, int y) const = 0;
    virtual const std::vector<unsigned char> &getTile(int z, int x, int y, std::string &s) = 0;
    virtual std::string generatePluginCode(bool overlay) const = 0;

    std::string getName() const { return name; }
    std::string getAttribution() const { return attribution; }
    int getMinZoom() const { return minZoom; }
    int getMaxZoom() const { return maxZoom; }
    std::string getFormat() const { return format; }
    std::string getLayerID() const { return layerID; }

    bool isValidCoordinate(int z, int x, int y) const;
};

#ifdef HASSQLITE
class MBTilesSupport : public MapTiles
{
private:
    sqlite3 *db;
    std::vector<int> zoomMapping;

    void loadMetadata();
public:
    MBTilesSupport();
    ~MBTilesSupport() override;

    bool open(const std::string &filename) override;
    bool isValidTile(int z, int x, int y) const override;
    const std::vector<unsigned char> &getTile(int z, int x, int y, std::string &contentType) override;
    std::string generatePluginCode(bool overlay) const override;

    int getMBTilesZoom(int olZoom) const;
};
#endif

class FileSystemTiles : public MapTiles
{
private:
    std::string basePath;
    std::vector<int> availableZooms;

    bool pathExists(const std::string &path) const;
    bool isDirectory(const std::string &path) const;
    std::string getFilename(const std::string &path) const;
    std::string getExtension(const std::string &filename) const;
    std::vector<std::string> listDirectory(const std::string &path) const;

    void scanDirectory();
    void detectFormat();

public:
    FileSystemTiles() = default;
    ~FileSystemTiles() override = default;

    bool open(const std::string &directoryPath) override;
    bool isValidTile(int z, int x, int y) const override;
    const std::vector<unsigned char> &getTile(int z, int x, int y, std::string &contentType) override;
    std::string generatePluginCode(bool overlay) const override;
};
