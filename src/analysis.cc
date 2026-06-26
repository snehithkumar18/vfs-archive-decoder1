#include "analysis.h"
#include "logger.h"
#include "math_utils.h"
#include <cmath>
#include <map>
#include <numeric>
#include <algorithm>
#include <cstring>

namespace PixelForge {

PixelForgeErrorCode Analysis::ComputeStats(const Image& src, ImageStats& stats) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();
    const auto& data = src.getData();

    double sum_r = 0, sum_g = 0, sum_b = 0;
    double sq_sum_r = 0, sq_sum_g = 0, sq_sum_b = 0;

    std::vector<uint32_t> hist(256, 0);
    size_t total_elements = static_cast<size_t>(w) * h;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = (static_cast<size_t>(y) * w + x) * ch;
            if (ch >= 3) {
                uint8_t r = data[idx + 0];
                uint8_t g = data[idx + 1];
                uint8_t b = data[idx + 2];
                sum_r += r; sum_g += g; sum_b += b;
                sq_sum_r += r * r; sq_sum_g += g * g; sq_sum_b += b * b;

                uint8_t gray = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
                hist[gray]++;
            } else {
                uint8_t gray = data[idx];
                sum_r += gray; sum_g += gray; sum_b += gray;
                sq_sum_r += gray * gray; sq_sum_g += gray * gray; sq_sum_b += gray * gray;
                hist[gray]++;
            }
        }
    }

    stats.mean_r = static_cast<float>(sum_r / total_elements);
    stats.mean_g = static_cast<float>(sum_g / total_elements);
    stats.mean_b = static_cast<float>(sum_b / total_elements);

    stats.variance_r = static_cast<float>((sq_sum_r / total_elements) - (stats.mean_r * stats.mean_r));
    stats.variance_g = static_cast<float>((sq_sum_g / total_elements) - (stats.mean_g * stats.mean_g));
    stats.variance_b = static_cast<float>((sq_sum_b / total_elements) - (stats.mean_b * stats.mean_b));

    // Compute entropy
    stats.entropy = 0.0f;
    for (int i = 0; i < 256; ++i) {
        if (hist[i] > 0) {
            float p = static_cast<float>(hist[i]) / total_elements;
            stats.entropy -= p * std::log2(p);
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

uint8_t Analysis::ComputeOtsuThreshold(const Image& src) {
    if (!src.isValid()) return 128;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();
    const auto& data = src.getData();

    std::vector<uint32_t> hist(256, 0);
    size_t total_pixels = static_cast<size_t>(w) * h;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = (static_cast<size_t>(y) * w + x) * ch;
            uint8_t gray = (ch >= 3) ? 
                static_cast<uint8_t>(0.299f * data[idx + 0] + 0.587f * data[idx + 1] + 0.114f * data[idx + 2]) : 
                data[idx];
            hist[gray]++;
        }
    }

    double sum = 0;
    for (int i = 0; i < 256; ++i) sum += i * hist[i];

    double sumB = 0;
    double wB = 0;
    double wF = 0;

    double varMax = 0;
    uint8_t threshold = 0;

    for (int t = 0; t < 256; ++t) {
        wB += hist[t];
        if (wB == 0) continue;

        wF = total_pixels - wB;
        if (wF == 0) break;

        sumB += static_cast<double>(t * hist[t]);

        double mB = sumB / wB;
        double mF = (sum - sumB) / wF;

        double varBetween = wB * wF * (mB - mF) * (mB - mF);

        if (varBetween > varMax) {
            varMax = varBetween;
            threshold = static_cast<uint8_t>(t);
        }
    }

    return threshold;
}

PixelForgeErrorCode Analysis::Threshold(const Image& src, Image& dst, uint8_t threshold) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, PixelFormat::Grayscale);
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t src_idx = (static_cast<size_t>(y) * w + x) * ch;
            uint8_t gray = (ch >= 3) ? 
                static_cast<uint8_t>(0.299f * src_data[src_idx + 0] + 0.587f * src_data[src_idx + 1] + 0.114f * src_data[src_idx + 2]) : 
                src_data[src_idx];
            
            dst_data[static_cast<size_t>(y) * w + x] = (gray >= threshold) ? 255 : 0;
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

// Disjoint Set Union (DSU) helper class for CCL labelling
class UnionFind {
    std::vector<uint32_t> parent;
public:
    uint32_t find(uint32_t i) {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    }

    void unite(uint32_t i, uint32_t j) {
        uint32_t root_i = find(i);
        uint32_t root_j = find(j);
        if (root_i != root_j) {
            parent[root_i] = root_j;
        }
    }

    uint32_t add_label() {
        uint32_t new_label = static_cast<uint32_t>(parent.size());
        parent.push_back(new_label);
        return new_label;
    }
};

PixelForgeErrorCode Analysis::LabelComponents(const Image& src, Image& dst, uint32_t& num_components) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    
    // Allocate 32-bit output image workspace first to hold labels without overflow
    std::vector<uint32_t> label_map(static_cast<size_t>(w) * h, 0);
    UnionFind uf;
    uf.add_label(); // Label 0 is background

    // Grayscale source check
    Image gray_src;
    if (src.getFormat() != PixelFormat::Grayscale) {
        src.convertTo(PixelFormat::Grayscale);
        gray_src = src;
    } else {
        gray_src = src;
    }
    const auto& src_data = gray_src.getData();

    // Pass 1: Assign preliminary labels
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = static_cast<size_t>(y) * w + x;
            if (src_data[idx] == 0) continue; // Background

            std::vector<uint32_t> neighbors;
            if (x > 0 && label_map[idx - 1] > 0) neighbors.push_back(label_map[idx - 1]); // Left
            if (y > 0) {
                if (label_map[idx - w] > 0) neighbors.push_back(label_map[idx - w]); // Top
                if (x > 0 && label_map[idx - w - 1] > 0) neighbors.push_back(label_map[idx - w - 1]); // Top-Left
                if (x + 1 < w && label_map[idx - w + 1] > 0) neighbors.push_back(label_map[idx - w + 1]); // Top-Right
            }

            if (neighbors.empty()) {
                label_map[idx] = uf.add_label();
            } else {
                uint32_t min_label = *std::min_element(neighbors.begin(), neighbors.end());
                label_map[idx] = min_label;
                for (uint32_t neighbor : neighbors) {
                    uf.unite(neighbor, min_label);
                }
            }
        }
    }

    // Pass 2: Resolve equivalence classes
    std::map<uint32_t, uint32_t> label_remap;
    uint32_t next_flat_label = 1;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = static_cast<size_t>(y) * w + x;
            if (label_map[idx] == 0) continue;

            uint32_t root = uf.find(label_map[idx]);
            if (label_remap.find(root) == label_remap.end()) {
                label_remap[root] = next_flat_label++;
            }
            label_map[idx] = label_remap[root];
        }
    }

    num_components = next_flat_label - 1;

    // Convert labels into RGB image representation for visualization
    dst.allocate(w, h, PixelFormat::RGB888);
    auto& dst_data = dst.getData();
    
    // Create random colors for each flat label
    std::vector<uint8_t> colors(static_cast<size_t>(next_flat_label) * 3, 0);
    for (uint32_t i = 1; i < next_flat_label; ++i) {
        colors[i * 3 + 0] = static_cast<uint8_t>((i * 57) % 200 + 55);
        colors[i * 3 + 1] = static_cast<uint8_t>((i * 113) % 200 + 55);
        colors[i * 3 + 2] = static_cast<uint8_t>((i * 179) % 200 + 55);
    }

    for (size_t i = 0; i < label_map.size(); ++i) {
        uint32_t label = label_map[i];
        dst_data[i * 3 + 0] = colors[label * 3 + 0];
        dst_data[i * 3 + 1] = colors[label * 3 + 1];
        dst_data[i * 3 + 2] = colors[label * 3 + 2];
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Analysis::CannyEdges(const Image& src, Image& dst, float low_threshold, float high_threshold) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();

    // 1. Convert to Grayscale
    Image gray;
    if (src.getFormat() != PixelFormat::Grayscale) {
        Image temp = src;
        temp.convertTo(PixelFormat::Grayscale);
        gray = temp;
    } else {
        gray = src;
    }

    // 2. Gaussian Blur (5x5) to reduce noise
    std::vector<float> blurred(static_cast<size_t>(w) * h, 0.0f);
    const auto& gray_data = gray.getData();

    const float gauss_kernel[5][5] = {
        {2/159.f,  4/159.f,  5/159.f,  4/159.f, 2/159.f},
        {4/159.f,  9/159.f, 12/159.f,  9/159.f, 4/159.f},
        {5/159.f, 12/159.f, 15/159.f, 12/159.f, 5/159.f},
        {4/159.f,  9/159.f, 12/159.f,  9/159.f, 4/159.f},
        {2/159.f,  4/159.f,  5/159.f,  4/159.f, 2/159.f}
    };

    for (int y = 0; y < static_cast<int>(h); ++y) {
        for (int x = 0; x < static_cast<int>(w); ++x) {
            float sum = 0.0f;
            for (int ky = -2; ky <= 2; ++ky) {
                int py = std::clamp(y + ky, 0, static_cast<int>(h) - 1);
                for (int kx = -2; kx <= 2; ++kx) {
                    int px = std::clamp(x + kx, 0, static_cast<int>(w) - 1);
                    sum += gray_data[static_cast<size_t>(py) * w + px] * gauss_kernel[ky + 2][kx + 2];
                }
            }
            blurred[static_cast<size_t>(y) * w + x] = sum;
        }
    }

    // 3. Sobel Gradients
    std::vector<float> mag(static_cast<size_t>(w) * h, 0.0f);
    std::vector<float> dir(static_cast<size_t>(w) * h, 0.0f);

    for (int y = 1; y < static_cast<int>(h) - 1; ++y) {
        for (int x = 1; x < static_cast<int>(w) - 1; ++x) {
            size_t idx = static_cast<size_t>(y) * w + x;
            
            float gx = blurred[idx + 1 - w] - blurred[idx - 1 - w]
                     + 2.0f * blurred[idx + 1] - 2.0f * blurred[idx - 1]
                     + blurred[idx + 1 + w] - blurred[idx - 1 + w];
                     
            float gy = blurred[idx - 1 - w] + 2.0f * blurred[idx - w] + blurred[idx + 1 - w]
                     - blurred[idx - 1 + w] - 2.0f * blurred[idx + w] - blurred[idx + 1 + w];

            mag[idx] = std::sqrt(gx * gx + gy * gy);
            dir[idx] = std::atan2(gy, gx) * 180.f / 3.14159265f;
            if (dir[idx] < 0) dir[idx] += 180.0f;
        }
    }

    // 4. Non-Maximum Suppression
    std::vector<float> nms(static_cast<size_t>(w) * h, 0.0f);
    for (int y = 1; y < static_cast<int>(h) - 1; ++y) {
        for (int x = 1; x < static_cast<int>(w) - 1; ++x) {
            size_t idx = static_cast<size_t>(y) * w + x;
            float angle = dir[idx];
            float val = mag[idx];

            float n1 = 0, n2 = 0;
            if ((angle >= 0 && angle < 22.5) || (angle >= 157.5 && angle <= 180)) {
                n1 = mag[idx - 1];
                n2 = mag[idx + 1];
            } else if (angle >= 22.5 && angle < 67.5) {
                n1 = mag[idx - 1 - w];
                n2 = mag[idx + 1 + w];
            } else if (angle >= 67.5 && angle < 112.5) {
                n1 = mag[idx - w];
                n2 = mag[idx + w];
            } else {
                n1 = mag[idx + 1 - w];
                n2 = mag[idx - 1 + w];
            }

            if (val >= n1 && val >= n2) {
                nms[idx] = val;
            }
        }
    }

    // 5. Hysteresis Thresholding
    dst.allocate(w, h, PixelFormat::Grayscale);
    auto& dst_data = dst.getData();
    std::memset(dst_data.data(), 0, dst_data.size());

    std::vector<uint8_t> edge_type(static_cast<size_t>(w) * h, 0); // 0=none, 1=weak, 2=strong
    std::vector<size_t> strong_edges;

    for (size_t i = 0; i < nms.size(); ++i) {
        if (nms[i] >= high_threshold) {
            edge_type[i] = 2;
            strong_edges.push_back(i);
        } else if (nms[i] >= low_threshold) {
            edge_type[i] = 1;
        }
    }

    // Trace weak edges connected to strong edges
    while (!strong_edges.empty()) {
        size_t idx = strong_edges.back();
        strong_edges.pop_back();

        dst_data[idx] = 255;

        // Check 8-neighbors
        int x = idx % w;
        int y = idx / w;

        for (int ny = -1; ny <= 1; ++ny) {
            int py = y + ny;
            if (py < 0 || py >= static_cast<int>(h)) continue;
            for (int nx = -1; nx <= 1; ++nx) {
                int px = x + nx;
                if (px < 0 || px >= static_cast<int>(w)) continue;

                size_t n_idx = static_cast<size_t>(py) * w + px;
                if (edge_type[n_idx] == 1) {
                    edge_type[n_idx] = 2;
                    strong_edges.push_back(n_idx);
                }
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Analysis::HoughLines(const Image& src, std::vector<HoughLine>& lines, uint32_t threshold_votes) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();

    // Image must be grayscale/binary
    Image binary = src;
    if (binary.getFormat() != PixelFormat::Grayscale) {
        binary.convertTo(PixelFormat::Grayscale);
    }
    const auto& bin_data = binary.getData();

    // Theta resolution: 1 degree (180 bins)
    // Rho resolution: 1 pixel
    int max_rho = static_cast<int>(std::ceil(std::sqrt(w * w + h * h)));
    int num_rho_bins = 2 * max_rho + 1;
    int num_theta_bins = 180;

    std::vector<uint32_t> accumulator(static_cast<size_t>(num_rho_bins) * num_theta_bins, 0);

    // Vote
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            if (bin_data[static_cast<size_t>(y) * w + x] > 127) { // Edge pixel
                for (int t = 0; t < num_theta_bins; ++t) {
                    float theta_rad = t * 3.14159265f / 180.0f;
                    float rho = x * std::cos(theta_rad) + y * std::sin(theta_rad);
                    int rho_bin = static_cast<int>(std::round(rho)) + max_rho;
                    
                    if (rho_bin >= 0 && rho_bin < num_rho_bins) {
                        accumulator[static_cast<size_t>(rho_bin) * num_theta_bins + t]++;
                    }
                }
            }
        }
    }

    // Find peaks
    for (int r = 0; r < num_rho_bins; ++r) {
        for (int t = 0; t < num_theta_bins; ++t) {
            uint32_t votes = accumulator[static_cast<size_t>(r) * num_theta_bins + t];
            if (votes >= threshold_votes) {
                // Local maxima check
                bool is_max = true;
                for (int dr = -1; dr <= 1; ++dr) {
                    int nr = r + dr;
                    if (nr < 0 || nr >= num_rho_bins) continue;
                    for (int dt = -1; dt <= 1; ++dt) {
                        int nt = (t + dt + num_theta_bins) % num_theta_bins;
                        if (accumulator[static_cast<size_t>(nr) * num_theta_bins + nt] > votes) {
                            is_max = false;
                            break;
                        }
                    }
                    if (!is_max) break;
                }

                if (is_max) {
                    HoughLine hl;
                    hl.rho = static_cast<float>(r - max_rho);
                    hl.theta = static_cast<float>(t);
                    hl.votes = votes;
                    lines.push_back(hl);
                }
            }
        }
    }

    std::sort(lines.begin(), lines.end(), [](const HoughLine& a, const HoughLine& b) {
        return a.votes > b.votes;
    });

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Analysis::ComputeSSIM(const Image& img1, const Image& img2, float& ssim_val) {
    if (!img1.isValid() || !img2.isValid() || 
        img1.getWidth() != img2.getWidth() || 
        img1.getHeight() != img2.getHeight() ||
        img1.getFormat() != img2.getFormat()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = img1.getWidth();
    uint32_t h = img1.getHeight();
    uint32_t ch = img1.getChannels();

    Image g1 = img1;
    Image g2 = img2;
    if (ch != 1) {
        g1.convertTo(PixelFormat::Grayscale);
        g2.convertTo(PixelFormat::Grayscale);
    }

    const auto& d1 = g1.getData();
    const auto& d2 = g2.getData();

    double mean_x = 0, mean_y = 0;
    size_t N = static_cast<size_t>(w) * h;

    for (size_t i = 0; i < N; ++i) {
        mean_x += d1[i];
        mean_y += d2[i];
    }
    mean_x /= N;
    mean_y /= N;

    double var_x = 0, var_y = 0, cov_xy = 0;
    for (size_t i = 0; i < N; ++i) {
        double dx = d1[i] - mean_x;
        double dy = d2[i] - mean_y;
        var_x += dx * dx;
        var_y += dy * dy;
        cov_xy += dx * dy;
    }
    var_x /= (N - 1);
    var_y /= (N - 1);
    cov_xy /= (N - 1);

    // SSIM Constants
    const double C1 = 6.5025;  // (k1*L)^2, k1=0.01, L=255
    const double C2 = 58.5225; // (k2*L)^2, k2=0.03, L=255

    double numerator = (2 * mean_x * mean_y + C1) * (2 * cov_xy + C2);
    double denominator = (mean_x * mean_x + mean_y * mean_y + C1) * (var_x + var_y + C2);

    ssim_val = static_cast<float>(numerator / denominator);
    return PixelForgeErrorCode::SUCCESS;
}

} // namespace PixelForge
