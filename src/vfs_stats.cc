#include "vfs_stats.h"
#include "logger.h"

void VFSStats::print_node_recursive(VFSNode* node, int depth, std::stringstream& ss) {
    if (!node) return;
    
    // Print indent
    for (int i = 0; i < depth; ++i) {
        ss << "  ";
    }
    
    if (node->type == NodeType::Directory) {
        ss << "+- " << node->name << "/ (Directory)\n";
        DirectoryNode* dir = static_cast<DirectoryNode*>(node);
        for (const auto& [name, child] : dir->children) {
            print_node_recursive(child.get(), depth + 1, ss);
        }
    } else {
        FileNode* file = static_cast<FileNode*>(node);
        ss << "|- " << node->name << " (File, Size: " << file->data.size() << " bytes)\n";
    }
}

void VFSStats::export_node_xml(VFSNode* node, int depth, std::stringstream& ss) {
    if (!node) return;
    
    std::string indent(depth * 2, ' ');
    
    if (node->type == NodeType::Directory) {
        ss << indent << "<directory name=\"" << node->name << "\">\n";
        DirectoryNode* dir = static_cast<DirectoryNode*>(node);
        for (const auto& [name, child] : dir->children) {
            export_node_xml(child.get(), depth + 1, ss);
        }
        ss << indent << "</directory>\n";
    } else {
        FileNode* file = static_cast<FileNode*>(node);
        ss << indent << "<file name=\"" << node->name << "\" size=\"" 
           << file->data.size() << "\" original_size=\"" << file->original_size 
           << "\" compressed=\"" << (file->is_compressed ? "true" : "false") << "\" />\n";
    }
}

size_t VFSStats::calculate_node_size_recursive(VFSNode* node) {
    if (!node) return 0;
    if (node->type == NodeType::File) {
        return static_cast<FileNode*>(node)->data.size();
    }
    
    size_t sum = 0;
    DirectoryNode* dir = static_cast<DirectoryNode*>(node);
    for (const auto& [name, child] : dir->children) {
        sum += calculate_node_size_recursive(child.get());
    }
    return sum;
}

size_t VFSStats::count_nodes_recursive(VFSNode* node) {
    if (!node) return 0;
    if (node->type == NodeType::File) return 1;
    
    size_t count = 1; // Count directory itself
    DirectoryNode* dir = static_cast<DirectoryNode*>(node);
    for (const auto& [name, child] : dir->children) {
        count += count_nodes_recursive(child.get());
    }
    return count;
}

std::string VFSStats::generate_ascii_tree(VFS& vfs) {
    VFSLogger::get_instance().info("VFSStats", "Generating directory ASCII tree...");
    std::stringstream ss;
    ss << "/ (Root Directory)\n";
    for (const auto& [name, child] : vfs.root->children) {
        print_node_recursive(child.get(), 1, ss);
    }
    return ss.str();
}

std::string VFSStats::export_to_xml(VFS& vfs) {
    VFSLogger::get_instance().info("VFSStats", "Exporting directory to XML...");
    std::stringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ss << "<vfs>\n";
    for (const auto& [name, child] : vfs.root->children) {
        export_node_xml(child.get(), 1, ss);
    }
    ss << "</vfs>\n";
    return ss.str();
}

size_t VFSStats::calculate_total_size(VFS& vfs) {
    return calculate_node_size_recursive(vfs.root.get());
}

size_t VFSStats::count_total_nodes(VFS& vfs) {
    return count_nodes_recursive(vfs.root.get());
}
