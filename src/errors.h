#ifndef ERRORS_H
#define ERRORS_H

#include <string>

enum class VFSErrorCode {
    SUCCESS = 0,
    ERROR_GENERIC = 1,
    ERROR_NOT_FOUND = 2,
    ERROR_EXISTS = 3,
    ERROR_INVALID_PATH = 4,
    ERROR_NOT_DIR = 5,
    ERROR_NOT_FILE = 6,
    ERROR_INVALID_HEADER = 7,
    ERROR_CHECKSUM_MISMATCH = 8,
    ERROR_DECOMPRESSION_FAILED = 9,
    ERROR_OUT_OF_MEMORY = 10,
    ERROR_INVALID_FD = 11,
    ERROR_ACCESS_DENIED = 12,
    ERROR_CACHE_FULL = 13
};

inline std::string error_to_string(VFSErrorCode code) {
    switch (code) {
        case VFSErrorCode::SUCCESS: return "Success";
        case VFSErrorCode::ERROR_GENERIC: return "Generic Error";
        case VFSErrorCode::ERROR_NOT_FOUND: return "File or Directory Not Found";
        case VFSErrorCode::ERROR_EXISTS: return "File or Directory Already Exists";
        case VFSErrorCode::ERROR_INVALID_PATH: return "Invalid Path format";
        case VFSErrorCode::ERROR_NOT_DIR: return "Target is not a directory";
        case VFSErrorCode::ERROR_NOT_FILE: return "Target is not a file";
        case VFSErrorCode::ERROR_INVALID_HEADER: return "Invalid Archive Header";
        case VFSErrorCode::ERROR_CHECKSUM_MISMATCH: return "Archive Checksum Mismatch";
        case VFSErrorCode::ERROR_DECOMPRESSION_FAILED: return "Data Decompression Failed";
        case VFSErrorCode::ERROR_OUT_OF_MEMORY: return "Out of Memory";
        case VFSErrorCode::ERROR_INVALID_FD: return "Invalid File Descriptor";
        case VFSErrorCode::ERROR_ACCESS_DENIED: return "Access Denied";
        case VFSErrorCode::ERROR_CACHE_FULL: return "LRU Cache Capacity Exceeded";
        default: return "Unknown Error Code";
    }
}

#endif // ERRORS_H
