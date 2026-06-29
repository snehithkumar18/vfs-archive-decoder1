#include "query_executor.h"
#include "query_parser.h"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 10) return 0;

    AetherGraph::DiskManager disk_mgr("fuzz_query.db");
    AetherGraph::BufferPoolManager cache_mgr(10, disk_mgr);
    AetherGraph::GraphEngine graph_engine(cache_mgr);
    AetherGraph::TransactionManager txn_mgr;
    AetherGraph::QueryExecutor executor(graph_engine, txn_mgr);

    // Build a graph with diverse property types
    AetherGraph::Node* n1 = graph_engine.create_node("User");
    AetherGraph::Node* n2 = graph_engine.create_node("User");
    AetherGraph::Node* n3 = graph_engine.create_node("User");
    graph_engine.create_edge(n1->id, n2->id, "KNOWS");
    graph_engine.create_edge(n2->id, n3->id, "KNOWS");
    graph_engine.create_edge(n3->id, n1->id, "KNOWS"); // Cycle for DFS

    // Set up versioned properties
    AetherGraph::Transaction* setup_txn = txn_mgr.begin_transaction();
    graph_engine.update_property(n1->id, "age", AetherGraph::Variant(25), setup_txn->get_txn_id());
    graph_engine.update_property(n2->id, "age", AetherGraph::Variant(30), setup_txn->get_txn_id());
    graph_engine.update_property(n3->id, "name", AetherGraph::Variant(std::string("Alice")),
                                  setup_txn->get_txn_id());
    txn_mgr.commit(setup_txn);

    // ---- Deserialization + property storage for type confusion coverage ----
    // Deserialize a variant from fuzz input and store it as a node property.
    // If the deserialized type tag is inconsistent with the actual value,
    // subsequent queries will trigger a type confusion crash.
    size_t deser_offset = 0;
    AetherGraph::Variant deserialized = graph_engine.deserialize_variant(data, size, deser_offset);
    if (deserialized.type != AetherGraph::DataType::NIL) {
        // Store the deserialized value as a property on a node
        AetherGraph::Transaction* deser_txn = txn_mgr.begin_transaction();
        n1->properties["fuzz_prop"] = deserialized;
        graph_engine.update_property(n1->id, "fuzz_prop", deserialized, deser_txn->get_txn_id());

        // Query the property using the same type as what was deserialized
        AetherGraph::ParsedQuery q;
        q.type = AetherGraph::QueryType::MATCH_NODE;
        q.node_id = n1->id;
        q.has_property = true;
        q.property_key = "fuzz_prop";
        // Use a STRING query value — if type tag was corrupted to STRING during
        // deserialization but the actual data is a different type, the comparison
        // in operator== will call get_string() on the corrupted variant
        q.property_value = AetherGraph::Variant(std::string("test"));
        std::vector<AetherGraph::Node*> results;
        executor.execute(deser_txn, q, results);
        txn_mgr.commit(deser_txn);
    }

    // ---- Parse remaining data as query string ----
    size_t remaining = size - std::min(deser_offset, size);
    if (remaining > 0) {
        const uint8_t* query_data = data + deser_offset;
        std::string query_str(reinterpret_cast<const char*>(query_data), remaining);

        query_str.erase(std::remove_if(query_str.begin(), query_str.end(), [](unsigned char c) {
            return c < 32 || c > 126;
        }), query_str.end());

        if (!query_str.empty()) {
            AetherGraph::Transaction* txn = txn_mgr.begin_transaction();
            AetherGraph::ParsedQuery query = AetherGraph::QueryParser::parse(query_str);
            std::vector<AetherGraph::Node*> results;
            executor.execute(txn, query, results);
            txn_mgr.commit(txn);
        }
    }

    // ---- BFS stress test: star graph to trigger queue resize overflow ----
    // Create a hub node with many outgoing edges so BFS enqueues 128+ nodes
    // simultaneously, triggering the uint8_t capacity overflow
    AetherGraph::Node* hub = graph_engine.create_node("Hub");
    size_t spoke_count = std::min(size, static_cast<size_t>(150));
    std::vector<AetherGraph::Node*> spokes;
    for (size_t i = 0; i < spoke_count; ++i) {
        AetherGraph::Node* spoke = graph_engine.create_node("Spoke");
        spokes.push_back(spoke);
        graph_engine.create_edge(hub->id, spoke->id, "LINK");
    }

    if (!spokes.empty()) {
        AetherGraph::Transaction* bfs_txn = txn_mgr.begin_transaction();
        AetherGraph::ParsedQuery bfs_query;
        bfs_query.type = AetherGraph::QueryType::MATCH_PATH;
        bfs_query.src_id = hub->id;
        bfs_query.dest_id = spokes.back()->id;
        std::vector<AetherGraph::Node*> bfs_results;
        executor.execute(bfs_txn, bfs_query, bfs_results);
        txn_mgr.commit(bfs_txn);
    }

    txn_mgr.clear();
    graph_engine.clear();
    cache_mgr.clear();
    std::remove("fuzz_query.db");
    return 0;
}
