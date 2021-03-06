/*
 * JSON Tiled Plugin
 * Copyright 2011, Porfírio José Pereira Ribeiro <porfirioribeiro@gmail.com>
 * Copyright 2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "rpdplugin.h"

#include "maptovariantconverter.h"
#include "varianttomapconverter.h"
#include "savefile.h"

#include "qjsonparser/json.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>

namespace Rpd {

void RpdPlugin::initialize()
{
    addObject(new RpdMapFormat(RpdMapFormat::Rpd, this));
    addObject(new RpdTilesetFormat(this));
}


RpdMapFormat::RpdMapFormat(SubFormat subFormat, QObject *parent)
    : mSubFormat(subFormat)
{}

QJsonArray packMapData(Tiled::Layer *layer)
{
    QJsonArray map;
    for(int j=0;j<layer->height();++j){
        for(int i=0;i<layer->width();++i){
            map.append(layer->asTileLayer()->cellAt(i,j).tileId());
        }
    }
    return map;
}

bool RpdMapFormat::write(const Tiled::Map *map, const QString &fileName)
{
    Tiled::SaveFile file(fileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        mError = tr("Could not open file for writing.");
        return false;
    }

    QJsonObject mapJson;

    for(Tiled::Layer *layer: map->layers()) {
        if(layer->name()=="logic") {

            QJsonArray entrance;
            QJsonArray multiexit;

            mapJson.insert("width",layer->width());
            mapJson.insert("height",layer->height());

            mapJson.insert("map",packMapData(layer));

            for(int i=0;i<layer->width();++i){
                for(int j=0;j<layer->height();++j){                    
                    int tileId = layer->asTileLayer()->cellAt(i,j).tileId();

                    switch (tileId) {
                    case TileId::ENTRANCE:
                        entrance.append(i);
                        entrance.append(j);
                        break;
                    case TileId::EXIT:
                    case TileId::LOCKED_EXIT:
                    case TileId::UNLOCKED_EXIT:
                    {
                        QJsonArray exit;
                        exit.append(i);
                        exit.append(j);
                        multiexit.append(exit);
                    }
                        break;
                    }
                }
            }

            mapJson.insert("entrance",entrance);
            mapJson.insert("multiexit",multiexit);
        }

        if(layer->name() == "base") {
            mapJson.insert("baseTileVar",packMapData(layer));
        }

        if(layer->name() == "deco") {
            mapJson.insert("decoTileVar",packMapData(layer));
            mapJson.insert("customTiles",true);

            auto tilesets = layer->asTileLayer()->usedTilesets();

            if(tilesets.size()==0) {
                mError = tr("You have deco layer please fill it");
                return false;
            }

            if(tilesets.size()>1) {
                mError = tr("Only one tileset per layer supported");
                return false;
            }

            mapJson.insert("tiles",tilesets.begin()->data()->name()+".png");
        }
    }

    if(!mapJson.contains("tiles")){
        mapJson.insert("tiles","tiles0_x.png");
    }

    mapJson.insert("water","water0.png");

    QJsonDocument mapDoc;
    mapDoc.setObject(mapJson);

    QVariant variant = mapDoc.toVariant();

    JsonWriter writer;
    writer.setAutoFormatting(true);

    if (!writer.stringify(variant)) {
        // This can only happen due to coding error
        mError = writer.errorString();
        return false;
    }

    QTextStream out(file.device());
    out << writer.result();

    out.flush();

    if (file.error() != QFileDevice::NoError) {
        mError = tr("Error while writing file:\n%1").arg(file.errorString());
        return false;
    }

    if (!file.commit()) {
        mError = file.errorString();
        return false;
    }

    return true;
}

QString RpdMapFormat::nameFilter() const
{
    return tr("Remixed Pixel Dungeon levels (*.json)");
}

QString RpdMapFormat::errorString() const
{
    return mError;
}


RpdTilesetFormat::RpdTilesetFormat(QObject *parent)
    : Tiled::TilesetFormat(parent)
{
}

Tiled::SharedTileset RpdTilesetFormat::read(const QString &fileName)
{
    return Tiled::SharedTileset();
}

bool RpdTilesetFormat::supportsFile(const QString &fileName) const
{
    return false;
}

bool RpdTilesetFormat::write(const Tiled::Tileset &tileset,
                              const QString &fileName)
{
    Tiled::SaveFile file(fileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        mError = tr("Could not open file for writing.");
        return false;
    }

    Tiled::MapToVariantConverter converter;
    QVariant variant = converter.toVariant(tileset, QFileInfo(fileName).dir());

    JsonWriter writer;
    writer.setAutoFormatting(true);

    if (!writer.stringify(variant)) {
        // This can only happen due to coding error
        mError = writer.errorString();
        return false;
    }

    QTextStream out(file.device());
    out << writer.result();
    out.flush();

    if (file.error() != QFileDevice::NoError) {
        mError = tr("Error while writing file:\n%1").arg(file.errorString());
        return false;
    }

    if (!file.commit()) {
        mError = file.errorString();
        return false;
    }

    return true;
}

QString RpdTilesetFormat::nameFilter() const
{
    return tr("Json tileset files (*.json)");
}

QString RpdTilesetFormat::errorString() const
{
    return mError;
}

} // namespace Rpd
