#ifndef ERRORS_H
#define ERRORS_H

#include <string>

namespace PixelForge {

enum class PixelForgeErrorCode {
    SUCCESS = 0,
    ERR_INVALID_PARAMETER = 1,
    ERR_UNSUPPORTED_FORMAT = 2,
    ERR_OUT_OF_MEMORY = 3,
    ERR_OUT_OF_BOUNDS = 4,
    ERR_DECODING_FAILED = 5,
    ERR_ENCODING_FAILED = 6,
    ERR_GENERIC = 7
};

inline std::string error_to_string(PixelForgeErrorCode code) {
    switch (code) {
        case PixelForgeErrorCode::SUCCESS: return "Success";
        case PixelForgeErrorCode::ERR_INVALID_PARAMETER: return "Invalid Parameter";
        case PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT: return "Unsupported Format";
        case PixelForgeErrorCode::ERR_OUT_OF_MEMORY: return "Out of Memory";
        case PixelForgeErrorCode::ERR_OUT_OF_BOUNDS: return "Out of Bounds";
        case PixelForgeErrorCode::ERR_DECODING_FAILED: return "Decoding Failed";
        case PixelForgeErrorCode::ERR_ENCODING_FAILED: return "Encoding Failed";
        case PixelForgeErrorCode::ERR_GENERIC: return "Generic Error";
        default: return "Unknown Error Code";
    }
}

} // namespace PixelForge

#endif // ERRORS_H
