#ifndef VFS_STATS_H
#define VFS_STATS_H

#include "vfs.h"
#include <string>
#include <sstream>

class VFSStats {
private:
    static void print_node_recursive(VFSNode* node, int depth, std::stringstream& ss);
    static void export_node_xml(VFSNode* node, int depth, std::stringstream& ss);
    static size_t calculate_node_size_recursive(VFSNode* node);
    static size_t count_nodes_recursive(VFSNode* node);

public:
    static std::string generate_ascii_tree(VFS& vfs);
    static std::string export_to_xml(VFS& vfs);
    static size_t calculate_total_size(VFS& vfs);
    static size_t count_total_nodes(VFS& vfs);
};

#endif // VFS_STATS_H
