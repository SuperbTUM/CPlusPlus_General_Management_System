#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/instance.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>

#include "SimpleJSON/json.hpp"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

namespace defaultsetting {
    constexpr char MongoDbUri[] = "mongodb://localhost:27017";
    constexpr char DatabaseName[] = "base database";
    constexpr char CollectionName[] = "base collection";

    class mongodb_database {
        private:
            mongocxx::uri uri;
            mongocxx::client client;
            mongocxx::database db;
            
            virtual ~mongodb_database() {}
        
        public:
            mongodb_database() : uri(mongocxx::uri(MongoDbUri)), client(mongocxx::client(uri)), db(client[DatabaseName]) {}
            mongodb_database(mongocxx::uri uri_, mongocxx::client client_, mongocxx::database db_): uri(uri_), client(client_), db(db_) {
            }
            mongodb_database(const mongodb_database& database) {}
            void connect2server(int port = 27017) {
                static mongocxx::instance instance{};
                mongocxx::uri uri_new("mongodb://localhost:" + to_string(port));
                mongocxx::client client_connected(uri_new);
                client = client_connected;
            }

            bool insert(const string &username, const string &password, const string &identity, const string &status = "valid") {
                mongocxx::collection coll = db[collectionName];
                auto builder = bsoncxx::builder::stream::document{};
                bsoncxx::document::value to_add = builder << "username" << username << "password"
                << password << "identity" << identity << "status" << status << bsoncxx::builder::stream::finalize;
                bsoncxx::stdx::optional<mongocxx::result::insert_one> maybe_result =
                 coll.insert_one(to_add.view());
                if(maybe_result) return maybe_result -> inserted_id().get_oid().value.to_string().size() != 0;
                return false;
            }

            bool update(string& username, string& attribute, auto&& attribute_val) {
                mongocxx::collection coll = db[collectionName];
                auto builder = bsoncxx::builder::stream::document{};
                bsoncxx::oid user_id(username);
                bsoncxx::document::value query_statement = builder << "username" << user_id << bsoncxx::builder::stream::finalize;
                bsoncxx::document::value update_statement = builder << "$inc" << bsoncxx::builder::stream::open_document <<
                attribute << attribute_val << bsoncxx::builder::stream::close_document << bsoncxx::builder::stream::finalize;

                bsoncxx::stdx::optional<mongocxx::result::update> maybe_result = 
                coll.update_one(query_statement.view(), update_statement.view());
                if(maybe_result) return maybe_result -> modified_count() == 1;
                return false;
            }

            bool remove(const string &username) {
                mongocxx::collection coll = db[collectionName];
                auto builder = bsoncxx::builder::stream::document{};
                bsoncxx::oid user_id(username);

                bsoncxx::document::value query_statement = builder << "username" << user_id << bsoncxx::builder::stream::finalize;

                bsoncxx::stdx::optional<mongocxx::result::delete_result> maybe_result = coll.delete_one(query_statement.view());
                if(maybe_result) return maybe_result -> deleted_count() == 1;
                return false;
            }

            json::JSON GetAllDocuments() {
                mongocxx::collection coll = db[collectionName];
                mongocxx::cursor cursor = coll.find({});
                json::JSON result;
                result["users"] = json::Array();

                if(cursor.begin() != cursor.end()) {
                    for (auto& doc: cursor) {
                        result["users"].append(bsoncxx::to_json(doc));
                    }
                }
                return result;
            }

    };
}
