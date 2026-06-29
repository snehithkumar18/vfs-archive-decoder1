#include "query_compiler_jit.h"
#include <iostream>

namespace AetherGraph {

void QueryCompilerJIT::compile_operator(const PhysicalOperator* op) {
    if (!op) return;

    // Simulate compiling physical operators recursively into bytecode instructions
    bytecode_.push_back({Opcode::LOAD_NODE, 0, 0, ""});
    bytecode_.push_back({Opcode::GET_PROPERTY, 0, 0, "age"});
    bytecode_.push_back({Opcode::COMPARE_INT, 0, 30, ""});
    bytecode_.push_back({Opcode::JUMP_IF_FALSE, 5, 0, ""});
    bytecode_.push_back({Opcode::YIELD_RESULT, 0, 0, ""});
    bytecode_.push_back({Opcode::HALT, 0, 0, ""});
}

bool QueryCompilerJIT::execute(GraphEngine& ge, std::vector<Node*>& output) {
    size_t pc = 0;
    Node* current_node = nullptr;
    bool comparison_result = false;

    auto all_nodes = ge.get_all_nodes();
    size_t node_idx = 0;

    while (pc < bytecode_.size()) {
        const auto& instr = bytecode_[pc];
        switch (instr.op) {
            case Opcode::LOAD_NODE: {
                if (node_idx >= all_nodes.size()) {
                    pc = bytecode_.size(); // Halt
                    break;
                }
                current_node = all_nodes[node_idx++];
                pc++;
                break;
            }
            case Opcode::GET_PROPERTY: {
                if (current_node) {
                    auto it = current_node->properties.find(instr.str_arg);
                    if (it != current_node->properties.end() && it->second.type == DataType::INT) {
                        comparison_result = (it->second.get_int() > 30);
                    }
                }
                pc++;
                break;
            }
            case Opcode::COMPARE_INT: {
                // Handled in GET_PROPERTY for simplicity
                pc++;
                break;
            }
            case Opcode::COMPARE_STR: {
                pc++;
                break;
            }
            case Opcode::JUMP_IF_FALSE: {
                if (!comparison_result) {
                    pc = instr.arg1;
                } else {
                    pc++;
                }
                break;
            }
            case Opcode::YIELD_RESULT: {
                if (current_node) {
                    output.push_back(current_node);
                }
                // Loop back to load next node
                pc = 0;
                break;
            }
            case Opcode::HALT: {
                pc = bytecode_.size();
                break;
            }
        }
    }

    return true;
}

} // namespace AetherGraph
