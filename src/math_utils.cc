#include "math_utils.h"

namespace PixelForge {

HSVColor RGBToHSV(uint8_t r, uint8_t g, uint8_t b) {
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;

    float max_c = std::max({rf, gf, bf});
    float min_c = std::min({rf, gf, bf});
    float delta = max_c - min_c;

    HSVColor hsv;
    hsv.v = max_c;

    if (delta < 0.00001f) {
        hsv.h = 0.0f;
        hsv.s = 0.0f;
        return hsv;
    }

    if (max_c > 0.0f) {
        hsv.s = delta / max_c;
    } else {
        hsv.s = 0.0f;
        hsv.h = 0.0f;
        return hsv;
    }

    if (rf >= max_c) {
        hsv.h = (gf - bf) / delta;
    } else if (gf >= max_c) {
        hsv.h = 2.0f + (bf - rf) / delta;
    } else {
        hsv.h = 4.0f + (rf - gf) / delta;
    }

    hsv.h *= 60.0f;
    if (hsv.h < 0.0f) {
        hsv.h += 360.0f;
    }

    return hsv;
}

void HSVToRGB(const HSVColor& hsv, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (hsv.s <= 0.0f) {
        uint8_t val = static_cast<uint8_t>(std::clamp(hsv.v * 255.0f, 0.0f, 255.0f));
        r = val;
        g = val;
        b = val;
        return;
    }

    float h = hsv.h;
    if (h >= 360.0f) h = 0.0f;
    h /= 60.0f;

    int i = static_cast<int>(h);
    float ff = h - i;
    float p = hsv.v * (1.0f - hsv.s);
    float q = hsv.v * (1.0f - (hsv.s * ff));
    float t = hsv.v * (1.0f - (hsv.s * (1.0f - ff)));

    float rf = 0.0f, gf = 0.0f, bf = 0.0f;
    switch (i) {
        case 0: rf = hsv.v; gf = t;     bf = p;     break;
        case 1: rf = q;     gf = hsv.v; bf = p;     break;
        case 2: rf = p;     gf = hsv.v; bf = t;     break;
        case 3: rf = p;     gf = q;     bf = hsv.v; break;
        case 4: rf = t;     gf = p;     bf = hsv.v; break;
        case 5: default:
            rf = hsv.v; gf = p;     bf = q;     break;
    }

    r = static_cast<uint8_t>(std::clamp(rf * 255.0f, 0.0f, 255.0f));
    g = static_cast<uint8_t>(std::clamp(gf * 255.0f, 0.0f, 255.0f));
    b = static_cast<uint8_t>(std::clamp(bf * 255.0f, 0.0f, 255.0f));
}

HSLColor RGBToHSL(uint8_t r, uint8_t g, uint8_t b) {
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;

    float max_c = std::max({rf, gf, bf});
    float min_c = std::min({rf, gf, bf});
    float sum_c = max_c + min_c;
    float delta = max_c - min_c;

    HSLColor hsl;
    hsl.l = sum_c / 2.0f;

    if (delta < 0.00001f) {
        hsl.h = 0.0f;
        hsl.s = 0.0f;
        return hsl;
    }

    if (hsl.l <= 0.5f) {
        hsl.s = delta / sum_c;
    } else {
        hsl.s = delta / (2.0f - sum_c);
    }

    if (rf >= max_c) {
        hsl.h = (gf - bf) / delta;
    } else if (gf >= max_c) {
        hsl.h = 2.0f + (bf - rf) / delta;
    } else {
        hsl.h = 4.0f + (rf - gf) / delta;
    }

    hsl.h *= 60.0f;
    if (hsl.h < 0.0f) {
        hsl.h += 360.0f;
    }

    return hsl;
}

static float HueToRGBHelper(float v1, float v2, float vH) {
    if (vH < 0.0f) vH += 1.0f;
    if (vH > 1.0f) vH -= 1.0f;
    if ((6.0f * vH) < 1.0f) return (v1 + (v2 - v1) * 6.0f * vH);
    if ((2.0f * vH) < 1.0f) return v2;
    if ((3.0f * vH) < 2.0f) return (v1 + (v2 - v1) * ((2.0f / 3.0f) - vH) * 6.0f);
    return v1;
}

void HSLToRGB(const HSLColor& hsl, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (hsl.s <= 0.0f) {
        uint8_t val = static_cast<uint8_t>(std::clamp(hsl.l * 255.0f, 0.0f, 255.0f));
        r = val;
        g = val;
        b = val;
        return;
    }

    float v2;
    if (hsl.l < 0.5f) {
        v2 = hsl.l * (1.0f + hsl.s);
    } else {
        v2 = (hsl.l + hsl.s) - (hsl.s * hsl.l);
    }

    float v1 = 2.0f * hsl.l - v2;

    float h = hsl.h / 360.0f;

    float rf = HueToRGBHelper(v1, v2, h + (1.0f / 3.0f));
    float gf = HueToRGBHelper(v1, v2, h);
    float bf = HueToRGBHelper(v1, v2, h - (1.0f / 3.0f));

    r = static_cast<uint8_t>(std::clamp(rf * 255.0f, 0.0f, 255.0f));
    g = static_cast<uint8_t>(std::clamp(gf * 255.0f, 0.0f, 255.0f));
    b = static_cast<uint8_t>(std::clamp(bf * 255.0f, 0.0f, 255.0f));
}

YCbCrColor RGBToYCbCr(uint8_t r, uint8_t g, uint8_t b) {
    YCbCrColor ycc;
    ycc.y  =  0.29900f * r + 0.58700f * g + 0.11400f * b;
    ycc.cb = -0.16874f * r - 0.33126f * g + 0.50000f * b + 128.0f;
    ycc.cr =  0.50000f * r - 0.41869f * g - 0.08131f * b + 128.0f;
    return ycc;
}

void YCbCrToRGB(const YCbCrColor& ycc, uint8_t& r, uint8_t& g, uint8_t& b) {
    float rf = ycc.y + 1.40200f * (ycc.cr - 128.0f);
    float gf = ycc.y - 0.34414f * (ycc.cb - 128.0f) - 0.71414f * (ycc.cr - 128.0f);
    float bf = ycc.y + 1.77200f * (ycc.cb - 128.0f);

    r = static_cast<uint8_t>(std::clamp(rf, 0.0f, 255.0f));
    g = static_cast<uint8_t>(std::clamp(gf, 0.0f, 255.0f));
    b = static_cast<uint8_t>(std::clamp(bf, 0.0f, 255.0f));
}

XYZColor RGBToXYZ(uint8_t r, uint8_t g, uint8_t b) {
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;

    // Inverse sRGB companding
    rf = (rf > 0.04045f) ? std::pow((rf + 0.055f) / 1.055f, 2.4f) : (rf / 12.92f);
    gf = (gf > 0.04045f) ? std::pow((gf + 0.055f) / 1.055f, 2.4f) : (gf / 12.92f);
    bf = (bf > 0.04045f) ? std::pow((bf + 0.055f) / 1.055f, 2.4f) : (bf / 12.92f);

    rf *= 100.0f;
    gf *= 100.0f;
    bf *= 100.0f;

    // D65 illuminant conversion matrix
    XYZColor xyz;
    xyz.x = rf * 0.4124564f + gf * 0.3575761f + bf * 0.1804375f;
    xyz.y = rf * 0.2126729f + gf * 0.7151522f + bf * 0.0721750f;
    xyz.z = rf * 0.0193339f + gf * 0.1191920f + bf * 0.9503041f;

    return xyz;
}

void XYZToRGB(const XYZColor& xyz, uint8_t& r, uint8_t& g, uint8_t& b) {
    float x = xyz.x / 100.0f;
    float y = xyz.y / 100.0f;
    float z = xyz.z / 100.0f;

    // Conversion matrix back to RGB
    float rf = x *  3.2404542f + y * -1.5371385f + z * -0.4985314f;
    float gf = x * -0.9692660f + y *  1.8760108f + z *  0.0415560f;
    float bf = x *  0.0556434f + y * -0.2040259f + z *  1.0572252f;

    // sRGB companding
    rf = (rf > 0.0031308f) ? (1.055f * std::pow(rf, 1.0f / 2.4f) - 0.055f) : (12.92f * rf);
    gf = (gf > 0.0031308f) ? (1.055f * std::pow(gf, 1.0f / 2.4f) - 0.055f) : (12.92f * gf);
    bf = (bf > 0.0031308f) ? (1.055f * std::pow(bf, 1.0f / 2.4f) - 0.055f) : (12.92f * bf);

    r = static_cast<uint8_t>(std::clamp(rf * 255.0f, 0.0f, 255.0f));
    g = static_cast<uint8_t>(std::clamp(gf * 255.0f, 0.0f, 255.0f));
    b = static_cast<uint8_t>(std::clamp(bf * 255.0f, 0.0f, 255.0f));
}

static float XYZToLabHelper(float val) {
    return (val > 0.008856f) ? std::pow(val, 1.0f / 3.0f) : (7.787f * val + 16.0f / 116.0f);
}

static float LabToXYZHelper(float val) {
    float val3 = val * val * val;
    return (val3 > 0.008856f) ? val3 : ((val - 16.0f / 116.0f) / 7.787f);
}

LabColor XYZToLab(const XYZColor& xyz) {
    // Reference white D65
    const float ref_x = 95.047f;
    const float ref_y = 100.000f;
    const float ref_z = 108.883f;

    float x = XYZToLabHelper(xyz.x / ref_x);
    float y = XYZToLabHelper(xyz.y / ref_y);
    float z = XYZToLabHelper(xyz.z / ref_z);

    LabColor lab;
    lab.l = (116.0f * y) - 16.0f;
    lab.a = 500.0f * (x - y);
    lab.b = 200.0f * (y - z);
    return lab;
}

XYZColor LabToXYZ(const LabColor& lab) {
    const float ref_x = 95.047f;
    const float ref_y = 100.000f;
    const float ref_z = 108.883f;

    float y = (lab.l + 16.0f) / 116.0f;
    float x = lab.a / 500.0f + y;
    float z = y - lab.b / 200.0f;

    XYZColor xyz;
    xyz.x = LabToXYZHelper(x) * ref_x;
    xyz.y = LabToXYZHelper(y) * ref_y;
    xyz.z = LabToXYZHelper(z) * ref_z;
    return xyz;
}

LabColor RGBToLab(uint8_t r, uint8_t g, uint8_t b) {
    return XYZToLab(RGBToXYZ(r, g, b));
}

void LabToRGB(const LabColor& lab, uint8_t& r, uint8_t& g, uint8_t& b) {
    XYZToRGB(LabToXYZ(lab), r, g, b);
}

} // namespace PixelForge
