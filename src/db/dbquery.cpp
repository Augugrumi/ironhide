//
// Created by zanna on 05/10/18.
//

#include "dbquery.h"

// DBQuery
db::DBQuery::DBQuery(const utils::Address& r_a) : roulette_addr(r_a) {
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl == nullptr) {
        perror("Impossible to properly init curl");
        exit(EXIT_FAILURE);
    }
}

db::DBQuery::DBQuery(const std::string& address, uint16_t port)
        : db::DBQuery(utils::Address(address, port)) {}

size_t db::DBQuery::curl_callback(void* ptr, size_t size,
                                  size_t nmemb, std::string* data) {
    data->append(reinterpret_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

std::string db::DBQuery::create_entry(const Query& query) {
    struct curl_slist* header = nullptr;
    std::string req_data_res;
    std::string req_addr = utils::URLBuilder()
            .set_address(roulette_addr)
            .add_path(ENDPOINT_PREFIX)
            .build();
    std::string json_query = query.to_json();

    LOG(ldebug, "URL to send data: " + req_addr);
    LOG(ldebug, "Data to send: " + json_query);

    header = curl_slist_append(header, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
    curl_easy_setopt(curl, CURLOPT_URL, req_addr.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, db::DBQuery::curl_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &req_data_res);
    //curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_query.c_str());

    /* Perform the request, res will get the return code */
    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(header);
    if (handle_req(
                res,
                std::bind<bool>(&db::DBQuery::is_op_ok, this, req_data_res))) {

        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        Json::Value response;

        std::string errors;
        reader->parse(req_data_res.c_str(),
                      req_data_res.c_str() + req_data_res.size(),
                      &response,
                      &errors);


        return response[reply::CONTENT]["id"].asString();
    }

    return std::string();
}

bool db::DBQuery::update_endpoint(const Query& query, const Endpoint& to_add) {
    struct curl_slist* header = nullptr;
    std::string req_data_res;
    std::string req_addr = utils::URLBuilder()
            .set_address(roulette_addr)
            .add_path(query.to_url())
            .build();
    std::string json_data = to_add.to_json();

    LOG(ldebug, "URL to send data: " + req_addr);
    LOG(ldebug, "Data to send: " + json_data);

    header = curl_slist_append(header, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
    curl_easy_setopt(curl, CURLOPT_URL, req_addr.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, db::DBQuery::curl_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &req_data_res);
    //curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());

    CURLcode res = curl_easy_perform(curl);
    return handle_req(
                res,
                std::bind<bool>(&db::DBQuery::is_op_ok, this, req_data_res));
}

db::DBQuery::Entry db::DBQuery::get_entry(const char* id) {
    const std::string req_addr = utils::URLBuilder()
            .set_address(roulette_addr)
            .add_path(ENDPOINT_PREFIX)
            .add_path(id)
            .build();
    std::string req_data_res;

    curl_easy_setopt(curl, CURLOPT_URL, req_addr.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, db::DBQuery::curl_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &req_data_res);

    CURLcode res = curl_easy_perform(curl);
    if (handle_req(
                res,
                std::bind<bool>(&db::DBQuery::is_op_ok, this, req_data_res))) {

        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        Json::Value response;

        std::string errors;
        reader->parse(req_data_res.c_str(),
                      req_data_res.c_str() + req_data_res.size(),
                      &response,
                      &errors);

        Json::Value content = response[reply::CONTENT];

        protocol_type prt = content[query::PROTOCOL] == "tcp" ? TCP : UDP;
        Endpoint ingress(content[query::INGRESS_IP].asString(),
                         content[query::SOCK_INGRESS].asString(),
                         INGRESS_T);

        Endpoint egress(content[query::EGRESS_IP].asString(),
                        content[query::SOCK_EGRESS].asString(),
                        EGRESS_T);


        return db::DBQuery::Entry::Builder()
                .set_item_id(content["_id"]["$oid"].asString())
                .set_id_sfc(content[query::SFC_ID].asString())
                .set_ip_src(content[query::SRC_IP].asString())
                .set_ip_dst(content[query::DST_IP].asString())
                .set_port_src(content[query::SRC_PORT].asUInt())
                .set_port_dst(content[query::DST_PORT].asUInt())
                .set_protocol(prt)
                .set_endpoint(ingress)
                .set_endpoint(egress)
                .build();
    } else {
        return db::DBQuery::Entry::Builder().build();
    }

}

bool db::DBQuery::delete_entry(const char* id) {
    const std::string req_addr = utils::URLBuilder()
            .set_address(roulette_addr)
            .add_path(ENDPOINT_PREFIX)
            .add_path(id)
            .build();
    std::string req_data_res;

    curl_easy_setopt(curl, CURLOPT_URL, req_addr.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, db::DBQuery::curl_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &req_data_res);

    CURLcode res = curl_easy_perform(curl);
    return handle_req(
                res,
                std::bind<bool>(&db::DBQuery::is_op_ok, this, req_data_res));
}

bool db::DBQuery::handle_req(const CURLcode& res, std::function<bool()> cb) {
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        throw db::exceptions::ios_base::failure("Error while making the request"
                                            "to route backend");
    } else {
        return cb();
    }
}

bool db::DBQuery::is_op_ok(const std::string& req) {
    LOG(ldebug, "Request performed successfully");

    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value response;

    std::string errors;
    LOG(ldebug, "Data obtained: " + req);
    bool read = reader->parse(req.c_str(),
                              req.c_str() + req.size(),
                              &response,
                              &errors);

    if (read) { // Read perform successfully
        return response[reply::RESULT] == reply::OK;
    } else {
        throw db::exceptions::ios_base::failure("Error while parsing the JSON"
                                            " reply");
    }
}

db::DBQuery::~DBQuery() {
    if (curl != nullptr) {
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}
// End DBQuery

// Endpoint
db::DBQuery::Endpoint::Endpoint(const std::string& new_ip,
                                const std::string& new_socket_id,
                                endpoint_type end_type)
    : ip(new_ip), socket_id(new_socket_id), type_of_proxy(end_type) {
}

std::string db::DBQuery::Endpoint::get_ip() const {
    return ip;
}

std::string db::DBQuery::Endpoint::get_socket_id() const {
    return socket_id;
}

db::endpoint_type db::DBQuery::Endpoint::get_endpoint_typology() const {
    return type_of_proxy;
}

std::string db::DBQuery::Endpoint::to_json() const {
    Json::Value json;

    if (type_of_proxy == INGRESS_T) {
        json[query::INGRESS_IP] = ip;
        json[query::SOCK_INGRESS] = socket_id;
    } else {
        json[query::EGRESS_IP] = ip;
        json[query::SOCK_EGRESS] = socket_id;
    }

    std::string res = json.asString();
    return res;
}
// End Endpoint

// Query
db::DBQuery::Query::Query(const std::string& new_item_id,
                          const std::string& new_ip_src,
                          const std::string& new_ip_dst,
                          uint16_t new_port_src,
                          uint16_t new_port_dst,
                          db::protocol_type type_of_protocol,
                          const std::string& new_id_sfc,
                          const std::vector<Endpoint>& new_endpoints)
    : item_id(new_item_id), ip_src(new_ip_src), ip_dst(new_ip_dst),
      port_src(new_port_src), port_dst(new_port_dst), prt(type_of_protocol),
      id_sfc(new_id_sfc), endpoints(new_endpoints) {
}

std::string db::DBQuery::Query::get_ip_src() const {
    return ip_src;
}

std::string db::DBQuery::Query::get_ip_dst() const {
    return ip_dst;
}

uint16_t db::DBQuery::Query::get_port_src() const {
    return port_src;
}

uint16_t db::DBQuery::Query::get_port_dst() const {
    return port_dst;
}

db::protocol_type db::DBQuery::Query::get_protocol_type() const {
    return prt;
}

std::string db::DBQuery::Query::get_id_sfc() const {
    return id_sfc;
}

std::vector<db::DBQuery::Endpoint> db::DBQuery::Query::get_all_endpoints() const {
    return endpoints;
}

db::DBQuery::Endpoint db::DBQuery::Query::get_endpoint(db::endpoint_type endpoint) const {
    for(auto it = endpoints.cbegin(); it != endpoints.cend(); it++) {
        if (it->get_endpoint_typology() == endpoint) {
            return *it;
        }
    }
    throw db::exceptions::logic_failure::endpoint_not_found("Endpoint not found");
}

// Pay attention! Here item_id is not included in the JSON file.
std::string db::DBQuery::Query::to_json() const {
    Json::Value json_res;

    json_res[query::SRC_IP] = ip_src;
    json_res[query::DST_IP] = ip_dst;
    json_res[query::SRC_PORT] = port_src;
    json_res[query::DST_PORT] = port_dst;
    json_res[query::SFC_ID] = id_sfc;
    (prt == protocol_type::TCP)? json_res[query::PROTOCOL] = "tcp" :
            json_res[query::PROTOCOL] = "udp";
    for (auto it = endpoints.cbegin(); it != endpoints.cend(); it++) {
        if (it->get_endpoint_typology() == INGRESS_T) {
            json_res[query::SOCK_INGRESS] = it->get_socket_id();
            json_res[query::INGRESS_IP] = it->get_ip();
        } else {
            json_res[query::SOCK_EGRESS] = it->get_socket_id();
            json_res[query::EGRESS_IP] = it->get_ip();
        }
    }

    return json_res.asString();
}

std::string db::DBQuery::Query::to_url() const {
    utils::URLBuilder query_builder =  utils::URLBuilder()
            .add_path(ENDPOINT_PREFIX);

    if (endpoints.size() != 0) {
        if (endpoints.size() == 1) {
            endpoints[0].get_endpoint_typology() == INGRESS_T ?
                        query_builder.add_path(query::EGRESS) :
                        query_builder.add_path(query::INGRESS);

            query_builder.add_path(ip_src)
                    .add_path(ip_dst)
                    .add_path(std::to_string(port_src))
                    .add_path(std::to_string(port_dst))
                    .add_path(id_sfc)
                    .add_path(
                        prt == TCP ? "tcp" : "udp"
                        )
                    .add_path(endpoints[0].get_ip())
                    .add_path(endpoints[0].get_socket_id());

            return query_builder.build();
        } else {
            throw exceptions::logic_failure::query_ambiguous("More than one "
                                                             "endpoint for this"
                                                             "query: which should"
                                                             "I use?");
        }
    } else {
        query_builder.add_path(item_id);
        return query_builder.build();
    }
}
// End Query

// Query::Builder
db::DBQuery::Query::Builder& db::DBQuery::Query::Builder::set_item_id(const std::string& item_id) {
    this->item_id = item_id;
    return *this;
}
db::DBQuery::Query::Builder& db::DBQuery::Query::Builder::set_ip_src(const std::string& ip_src) {
    this->ip_src = ip_src;
    return *this;
}
db::DBQuery::Query::Builder& db::DBQuery::Query::Builder::set_ip_dst(const std::string& ip_dst) {
    this->ip_dst = ip_dst;
    return *this;
}
db::DBQuery::Query::Builder& db::DBQuery::Query::Builder::set_port_src(uint16_t port_src) {
    this->port_src = port_src;
    return *this;
}
db::DBQuery::Query::Builder& db::DBQuery::Query::Builder::set_port_dst(uint16_t port_dst) {
    this->port_dst = port_dst;
    return *this;
}
db::DBQuery::Query::Builder& db::DBQuery::Query::Builder::set_protocol(db::protocol_type prt) {
    this->prt = prt;
    return *this;
}
db::DBQuery::Query::Builder& db::DBQuery::Query::Builder::set_id_sfc(const std::string& id_sfc) {
    this->id_sfc = id_sfc;
    return *this;
}
db::DBQuery::Query::Builder& db::DBQuery::Query::Builder::set_endpoint(const Endpoint& endpoint) {
    for (auto it = endpoints.begin(); it != endpoints.end(); it++) {
        if (it->get_endpoint_typology() == endpoint.get_endpoint_typology()) {
            endpoints.erase(it);
            endpoints.push_back(endpoint);
            return *this;
        }
    }
    endpoints.push_back(endpoint);
    return *this;
}
db::DBQuery::Query db::DBQuery::Query::Builder::build() const {
    return Query(item_id,
                 ip_src,
                 ip_dst,
                 port_src,
                 port_dst,
                 prt,
                 id_sfc,
                 endpoints);
}
// End Query::Builder
