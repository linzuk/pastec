/*****************************************************************************
 * Copyright (C) 2014 Visualink
 *
 * Authors: Adrien Maglo <adrien@visualink.io>
 *
 * This file is part of Pastec.
 *
 * Pastec is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Pastec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Pastec.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <iostream>
#include <stdlib.h>

#ifndef __APPLE__
#include <jsoncpp/json/json.h>
#else
#include <json/json.h>
#endif

#include <requesthandler.h>
#include <messages.h>
#include <featureextractor.h>
#include <searcher.h>
#include <indexcollection.h>

#include <imageloader.h>
#include <opencv2/highgui/highgui.hpp>


RequestHandler::RequestHandler(FeatureExtractor *featureExtractor,
               Searcher *imageSearcher, IndexCollection *indexCol, string authKey)
    : featureExtractor(featureExtractor), imageSearcher(imageSearcher),
      indexCol(indexCol), authKey(authKey)
{ }


/**
 * @brief Parse an URI.
 * @param uri the uri string.
 * @return the vector containing all the URI element between slashes.
 */
vector<string> RequestHandler::parseURI(string uri)
{
    vector<string> ret;

    if (uri == "/" || uri[0] != '/')
        return ret;

    size_t pos1 = 1;
    size_t pos2;

    while ((pos2 = uri.find('/', pos1)) != string::npos)
    {
        ret.push_back(uri.substr(pos1, pos2 - pos1));
        pos1 = pos2 + 1;
    }

    ret.push_back(uri.substr(pos1, uri.length() - pos1));

    return ret;
}


/**
 * @brief Test that a given parsed URI corresponds to a given request pattern.
 * @param parsedURI the parsed URI.
 * @param p_pattern the request pattern.
 * @return true if there is a correspondance, else false.
 */
bool RequestHandler::testURIWithPattern(vector<string> parsedURI, string p_pattern[])
{
    unsigned i = 0;
    for (;; ++i)
    {
        if (p_pattern[i] == "")
            break;
        if (i >= parsedURI.size())
            return false;
        if (p_pattern[i] == "IDENTIFIER")
        {
            // Test we have a number here.
            if (parsedURI[i].length() == 0)
                return false;
            char* p;
            long n = strtol(parsedURI[i].c_str(), &p, 10);
            if (*p != 0)
                return false;
            if (n < 0)
                return false;
        }
        else if (p_pattern[i] == "INDEX_NAME")
        {
            1;
        }
        else if (p_pattern[i] != parsedURI[i])
            return false;
    }

    if (i != parsedURI.size())
        return false;

    return true;
}


/**
 * @brief RequestHandler::handlePost
 * @param uri
 * @param p_data
 * @return
 */
void RequestHandler::handleRequest(ConnectionInfo &conInfo)
{
    vector<string> parsedURI = parseURI(conInfo.url);

    string p_image[] = {"indexes", "INDEX_NAME", "images", "IDENTIFIER", ""};
    string p_tag[] = {"indexes", "INDEX_NAME", "images", "IDENTIFIER", "tag", ""};
    string p_searchImage[] = {"indexes", "INDEX_NAME", "searcher", ""};
    string p_ioIndex[] = {"indexes", "INDEX_NAME", "io", ""};
    string p_imageIds[] = {"indexes", "INDEX_NAME", "imageIds", ""};
    string p_indexes[] = {"indexes", "INDEX_NAME", ""};
    string p_root[] = {""};

    Json::Value ret;
    u_int32_t i_ret;
    conInfo.answerCode = MHD_HTTP_OK;

    if (authKey != "" && conInfo.authKey != authKey) {
        conInfo.answerCode = MHD_HTTP_FORBIDDEN;
        ret["type"] = Converter::codeToString(AUTHENTIFICATION_ERROR);
    }
    else if (testURIWithPattern(parsedURI, p_image)
        && conInfo.connectionType == PUT)
    {
        Index *index;
        i_ret = indexCol->get(parsedURI[1], &index);
        u_int32_t i_imageId = atoi(parsedURI[3].c_str());

        if (i_ret == OK)
        {
            unsigned i_nbFeaturesExtracted;
            i_ret = featureExtractor->processNewImage(index,
                i_imageId, conInfo.uploadedData.size(), conInfo.uploadedData.data(),
                i_nbFeaturesExtracted);

            ret["image_id"] = Json::Value(i_imageId);
            if (i_ret == IMAGE_ADDED)
                ret["nb_features_extracted"] = Json::Value(i_nbFeaturesExtracted);
        }
        ret["type"] = Converter::codeToString(i_ret);
    }
    else if (testURIWithPattern(parsedURI, p_image)
             && conInfo.connectionType == DELETE)
    {
        Index *index;
        i_ret = indexCol->get(parsedURI[1], &index);
        u_int32_t i_imageId = atoi(parsedURI[3].c_str());

        if (i_ret == OK)
        {
            i_ret = index->removeImage(i_imageId);
            ret["type"] = Converter::codeToString(i_ret);
        }
        ret["image_id"] = Json::Value(i_imageId);
    }
    else if (testURIWithPattern(parsedURI, p_tag)
             && conInfo.connectionType == PUT)
    {
        Index *index;
        i_ret = indexCol->get(parsedURI[1], &index);
        u_int32_t i_imageId = atoi(parsedURI[3].c_str());

        string dataStr(conInfo.uploadedData.begin(),
                       conInfo.uploadedData.end());

        if (i_ret == OK)
            i_ret = index->addTag(i_imageId, dataStr);

        ret["type"] = Converter::codeToString(i_ret);
    }
    else if (testURIWithPattern(parsedURI, p_tag)
             && conInfo.connectionType == DELETE)
    {
        Index *index;
        i_ret = indexCol->get(parsedURI[1], &index);
        u_int32_t i_imageId = atoi(parsedURI[3].c_str());

        if (i_ret == OK)
            i_ret = index->removeTag(i_imageId);

        ret["type"] = Converter::codeToString(i_ret);
    }
    else if (testURIWithPattern(parsedURI, p_searchImage)
             && conInfo.connectionType == POST)
    {
        Index *index;
        i_ret = indexCol->get(parsedURI[1], &index);

        if (i_ret == OK)
        {
            SearchRequest req;

            req.imageData = conInfo.uploadedData;
            req.client = NULL;
            u_int32_t i_ret = imageSearcher->searchImage(index, req);

            ret["type"] = Converter::codeToString(i_ret);
            if (i_ret == SEARCH_RESULTS)
                ret["results"] = searchResult(req);
        }
    }
    else if (testURIWithPattern(parsedURI, p_image)
        && conInfo.connectionType == GET)
    {
        Index *index;
        i_ret = indexCol->get(parsedURI[1], &index);

        if (i_ret == OK)
        {
            SearchRequest req;

            req.imageId = atoi(parsedURI[3].c_str());
            req.client = NULL;
            i_ret = imageSearcher->searchSimilar(index, req);

            ret["type"] = Converter::codeToString(i_ret);

            if (i_ret == SEARCH_RESULTS)
                ret["results"] = searchResult(req);
        }
    }
    else if (testURIWithPattern(parsedURI, p_ioIndex)
             && conInfo.connectionType == POST)
    {
        Index *index;
        i_ret = indexCol->get(parsedURI[1], &index);

        if (i_ret == OK)
        {
            string dataStr(conInfo.uploadedData.begin(),
                           conInfo.uploadedData.end());

            Json::Value data = StringToJson(dataStr);
            if (data["type"] == "LOAD")
                i_ret = index->load(data["index_path"].asString());
            else if (data["type"] == "WRITE")
                i_ret = index->write(data["index_path"].asString());
            else if (data["type"] == "LOAD_TAGS")
                i_ret = index->loadTags(data["index_tags_path"].asString());
            else if (data["type"] == "WRITE_TAGS")
                i_ret = index->writeTags(data["index_tags_path"].asString());
            else if (data["type"] == "CLEAR")
                i_ret = index->clear();
            else
                i_ret = MISFORMATTED_REQUEST;
        } 

        ret["type"] = Converter::codeToString(i_ret);
    }
    else if (testURIWithPattern(parsedURI, p_imageIds)
             && conInfo.connectionType == GET)
    {
        Index *index;
        i_ret = indexCol->get(parsedURI[1], &index);

        if (i_ret == OK)
        {
            vector<u_int32_t> imageIds;
            i_ret = index->getImageIds(imageIds);

            // Return the image ids
            Json::Value imageIdsVal(Json::arrayValue);
            for (unsigned i = 0; i < imageIds.size(); ++i)
                imageIdsVal.append(imageIds[i]);
            ret["image_ids"] = imageIdsVal;
        }

        ret["type"] = Converter::codeToString(i_ret);
    }
    else if (testURIWithPattern(parsedURI, p_root)
             && conInfo.connectionType == POST)
    {
        string dataStr(conInfo.uploadedData.begin(),
                       conInfo.uploadedData.end());

        Json::Value data = StringToJson(dataStr);
        if (data["type"] == "PING")
        {
            cout << "Ping received." << endl;
            i_ret = PONG;
        }
        else
            i_ret = MISFORMATTED_REQUEST;

        ret["type"] = Converter::codeToString(i_ret);
    }
    else if (testURIWithPattern(parsedURI, p_indexes)
             && conInfo.connectionType == POST)
    {
        i_ret = indexCol->addIndex(parsedURI[1]);
        ret["type"] = Converter::codeToString(i_ret);
    }
    else if (testURIWithPattern(parsedURI, p_indexes)
             && conInfo.connectionType == DELETE)
    {
        i_ret = indexCol->removeIndex(parsedURI[1]);
        ret["type"] = Converter::codeToString(i_ret);
    }
    else if (testURIWithPattern(parsedURI, p_indexes)
             && conInfo.connectionType == GET) {
        Index *index;
        i_ret = indexCol->get(parsedURI[1], &index);
        ret["type"] = Converter::codeToString(i_ret);
    }
    else
    {
        conInfo.answerCode = MHD_HTTP_INTERNAL_SERVER_ERROR;
        ret["type"] = Converter::codeToString(MISFORMATTED_REQUEST);
    }

    conInfo.answerString = JsonToString(ret);
}


/**
 * @brief Build the JSON search result
 * @param req the search request
 * @return the built JSON
 */
Json::Value RequestHandler::searchResult(SearchRequest &req)
{
    Json::Value ret(Json::arrayValue);

    assert(req.results.size() == req.boundingRects.size());
    assert(req.results.size() == req.scores.size());
    assert(req.results.size() == req.tags.size());

    for (unsigned i = 0; i < req.results.size(); ++i)
    {
        Json::Value res;
        res["image_id"] = req.results[i];

        Rect r = req.boundingRects[i];
        Json::Value rVal;
        rVal["x"] = r.x; rVal["y"] = r.y;
        rVal["width"] = r.width; rVal["height"] = r.height;
        res["bounding_rect"] = rVal;

        res["score"] = req.scores[i];

        res["tag"] = req.tags[i];

        ret.append(res);
    }

    return ret;
}


/**
 * @brief Conver to JSON value to a string.
 * @param data the JSON value.
 * @return the converted string.
 */
string RequestHandler::JsonToString(Json::Value data)
{
    Json::FastWriter writer;
    return writer.write(data);
}


/**
 * @brief Convert a string to a JSON value.
 * @param str the string
 * @return the converted JSON value.
 */
Json::Value RequestHandler::StringToJson(string str)
{
    Json::Reader reader;
    Json::Value data;
    reader.parse(str, data);
    return data;
}
