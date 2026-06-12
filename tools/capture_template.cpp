#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <filesystem>
#include <map>
#include <iomanip>
#include <sstream>
#include <regex>
#include <algorithm>
#include <vector>
#include <numeric>
#include <cmath>

namespace fs = std::filesystem;

static const std::map<int, std::string> CHAR_NAMES = {
    {1, "司令"}, {2, "军长"}, {3, "师长"}, {4, "旅长"},
    {5, "团长"}, {6, "营长"}, {7, "连长"}, {8, "排长"},
    {9, "工兵"}, {10, "地雷"}, {11, "炸弹"}, {12, "军旗"}
};

// ---------- filename parsing ----------

static int parse_char_id_from_filename(const std::string& path) {
    std::string stem = fs::path(path).stem().string();
    for (char sep : {'-', '_'}) {
        std::string s = stem;
        std::vector<std::string> parts;
        size_t pos;
        while ((pos = s.find(sep)) != std::string::npos) {
            parts.push_back(s.substr(0, pos));
            s = s.substr(pos + 1);
        }
        parts.push_back(s);
        if (parts.size() >= 2) {
            for (const auto& [id, cn_name] : CHAR_NAMES) {
                if (parts[1] == cn_name) return id;
            }
        }
        if (parts.size() == 2) {
            for (const auto& [id, cn_name] : CHAR_NAMES) {
                if (parts[0] == cn_name) return id;
            }
        }
    }
    return 0;
}

// ---------- quality scoring ----------

struct QualityScore {
    double total = 0.0;
    double size_consistency = 0.0;
    double centering = 0.0;
    double cleanliness = 0.0;
    double stroke_continuity = 0.0;
    int white_pixels = 0;
    int hole_count = 0;
    bool rejected = false;
    std::string reject_reason;
};

// Per-character expected white-pixel area (learned from first pass in batch mode).
// -1 means "not yet known" — size_consistency is skipped.
static std::map<int, double> expected_white_area;

static QualityScore score_template(const cv::Mat& norm64, int char_id = 0) {
    QualityScore qs;
    int total = 64 * 64;
    int white = cv::countNonZero(norm64);
    qs.white_pixels = white;
    double ratio = static_cast<double>(white) / total;

    // --- Hard rejections ---
    if (ratio < 0.06) {
        qs.rejected = true;
        qs.reject_reason = "white_ratio too low (" + std::to_string(ratio) + ")";
        return qs;
    }
    if (ratio > 0.60) {
        qs.rejected = true;
        qs.reject_reason = "white_ratio too high (" + std::to_string(ratio) + ")";
        return qs;
    }

    // --- Border cleanliness (5px border) ---
    int border_white = 0;
    for (int r = 0; r < 64; r++) {
        const uchar* row = norm64.ptr<uchar>(r);
        for (int c = 0; c < 64; c++) {
            if (row[c] && (r < 5 || r >= 59 || c < 5 || c >= 59))
                border_white++;
        }
    }
    double border_ratio = static_cast<double>(border_white) / std::max(1, white);
    if (border_ratio > 0.30) {
        qs.rejected = true;
        qs.reject_reason = "border white ratio too high (" + std::to_string(border_ratio) + ")";
        return qs;
    }
    qs.cleanliness = 1.0 - border_ratio;

    // --- Hole count (too many holes = broken strokes or noise) ---
    std::vector<std::vector<cv::Point>> cnts;
    std::vector<cv::Vec4i> hier;
    cv::findContours(norm64, cnts, hier, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);
    int holes = 0;
    for (size_t i = 0; i < cnts.size(); i++)
        if (hier[i][3] != -1 && cv::contourArea(cnts[i]) > 3) holes++;
    qs.hole_count = holes;
    if (holes > 8) {
        qs.rejected = true;
        qs.reject_reason = "too many holes (" + std::to_string(holes) + ")";
        return qs;
    }

    // --- Stroke continuity: largest connected component / total white ---
    double max_cc = 0;
    for (size_t i = 0; i < cnts.size(); i++) {
        if (hier[i][3] == -1) {  // external contour
            double a = cv::contourArea(cnts[i]);
            if (a > max_cc) max_cc = a;
        }
    }
    double continuity = (white > 0) ? (max_cc / white) : 0;
    // Complex chars with dots (like 旅, 长 have separate strokes) may have lower continuity.
    // Threshold at 0.3 lets through multi-component chars.
    if (continuity < 0.20) {
        qs.rejected = true;
        qs.reject_reason = "stroke continuity too low (" + std::to_string(continuity) + ")";
        return qs;
    }
    qs.stroke_continuity = std::min(1.0, continuity);

    // --- Centering: how far is the white-pixel centroid from (32,32)? ---
    double cx = 0, cy = 0;
    for (int r = 0; r < 64; r++) {
        const uchar* row = norm64.ptr<uchar>(r);
        for (int c = 0; c < 64; c++) {
            if (row[c]) { cx += c; cy += r; }
        }
    }
    cx /= std::max(1, white);
    cy /= std::max(1, white);
    double offset = std::sqrt((cx - 31.5) * (cx - 31.5) + (cy - 31.5) * (cy - 31.5));
    // offset up to 16px is acceptable; beyond that, score decreases
    qs.centering = 1.0 - std::min(1.0, offset / 16.0);

    // --- Size consistency (if expected area is known for this character) ---
    if (char_id > 0) {
        auto it = expected_white_area.find(char_id);
        if (it != expected_white_area.end() && it->second > 0) {
            double expected = it->second;
            double deviation = std::abs(white - expected) / expected;
            if (deviation > 0.40) {
                qs.rejected = true;
                qs.reject_reason = "size deviation too large (" + std::to_string(deviation)
                                 + "), expected~" + std::to_string(static_cast<int>(expected))
                                 + " got " + std::to_string(white);
                return qs;
            }
            qs.size_consistency = 1.0 - deviation;
        } else {
            qs.size_consistency = 0.5;  // neutral when unknown
        }
    } else {
        qs.size_consistency = 0.5;
    }

    // --- Composite score ---
    qs.total = 0.25 * qs.size_consistency
             + 0.25 * qs.centering
             + 0.25 * qs.cleanliness
             + 0.25 * qs.stroke_continuity;

    return qs;
}

// ---------- Otsu-based character extraction ----------

// Result of a single extraction attempt
struct Candidate {
    cv::Mat norm;
    QualityScore score;
    bool deskewed = false;
};

// Extract character from a piece ROI using Otsu binarization.
// Returns true and fills `cand` on success.
static bool extract_otsu(const cv::Mat& piece_bgr, cv::Mat& out_norm, QualityScore& out_score, bool deskew) {
    cv::Mat working = piece_bgr.clone();
    cv::Rect crop(0, 0, working.cols, working.rows);

    if (deskew) {
        cv::Mat gray;
        cv::cvtColor(working, gray, cv::COLOR_BGR2GRAY);

        // Binarize to find piece contour for deskew
        cv::Mat bin;
        cv::threshold(gray, bin, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        std::vector<std::vector<cv::Point>> cnts;
        cv::findContours(bin, cnts, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        if (cnts.empty()) return false;

        // Find largest contour
        int bi = 0;
        double ba = 0;
        for (size_t i = 0; i < cnts.size(); i++) {
            double a = cv::contourArea(cnts[i]);
            if (a > ba) { ba = a; bi = static_cast<int>(i); }
        }

        cv::RotatedRect rr = cv::minAreaRect(cnts[bi]);
        double angle = rr.angle;
        cv::Size sz = rr.size;
        if (sz.width < sz.height) { angle += 90.0; std::swap(sz.width, sz.height); }

        // Only deskew if rotation is significant (> 0.5 degrees)
        if (std::abs(angle) > 0.5) {
            cv::Mat rot_mat = cv::getRotationMatrix2D(rr.center, angle, 1.0);
            cv::warpAffine(working, working, rot_mat, working.size(),
                           cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

            crop = cv::Rect(
                std::max(0, static_cast<int>(rr.center.x - sz.width / 2)),
                std::max(0, static_cast<int>(rr.center.y - sz.height / 2)),
                std::min(working.cols, static_cast<int>(sz.width)),
                std::min(working.rows, static_cast<int>(sz.height)));
            if (crop.x < 0) { crop.width += crop.x; crop.x = 0; }
            if (crop.y < 0) { crop.height += crop.y; crop.y = 0; }
            if (crop.x + crop.width  > working.cols) crop.width  = working.cols - crop.x;
            if (crop.y + crop.height > working.rows) crop.height = working.rows - crop.y;
        }
    }

    crop &= cv::Rect(0, 0, working.cols, working.rows);
    if (crop.area() <= 0) return false;

    cv::Mat roi = working(crop).clone();
    cv::Mat roi_gray;
    cv::cvtColor(roi, roi_gray, cv::COLOR_BGR2GRAY);

    // Light bilateral filter to reduce noise while preserving edges
    cv::Mat denoised;
    cv::bilateralFilter(roi_gray, denoised, 5, 40, 40);

    // ---- Two-stage extraction: piece mask → character within piece ----

    // Stage A: Find the white piece on black background
    cv::Mat piece_bin;
    cv::threshold(denoised, piece_bin, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    // Verify piece was found: white_ratio should be 10%-55% (piece area relative to crop)
    double piece_wr = static_cast<double>(cv::countNonZero(piece_bin))
                    / (piece_bin.rows * piece_bin.cols);
    if (piece_wr < 0.05 || piece_wr > 0.60) {
        // Piece detection failed; try adaptive threshold
        int ttype = cv::THRESH_BINARY;
        cv::adaptiveThreshold(denoised, piece_bin, 255,
                              cv::ADAPTIVE_THRESH_GAUSSIAN_C, ttype, 51, 8);
        piece_wr = static_cast<double>(cv::countNonZero(piece_bin))
                 / (piece_bin.rows * piece_bin.cols);
        if (piece_wr < 0.03 || piece_wr > 0.65) return false;
    }

    // Find the piece contour (largest white region)
    std::vector<std::vector<cv::Point>> piece_cnts;
    cv::findContours(piece_bin, piece_cnts, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (piece_cnts.empty()) return false;

    int pidx = 0;
    double pmax = 0;
    for (size_t i = 0; i < piece_cnts.size(); i++) {
        double a = cv::contourArea(piece_cnts[i]);
        if (a > pmax) { pmax = a; pidx = static_cast<int>(i); }
    }

    // Create piece mask
    cv::Mat piece_mask = cv::Mat::zeros(denoised.size(), CV_8UC1);
    cv::drawContours(piece_mask, piece_cnts, pidx, cv::Scalar(255), cv::FILLED);

    // Stage B: Within the piece mask, find character strokes
    // Black text on white piece → THRESH_BINARY_INV inside the mask
    cv::Mat inside_piece;
    cv::bitwise_and(denoised, denoised, inside_piece, piece_mask);

    // Otsu only on non-zero pixels (inside the piece)
    cv::Mat char_bin;
    cv::threshold(inside_piece, char_bin, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    // Mask out background from char_bin
    cv::bitwise_and(char_bin, piece_mask, char_bin);

    // Check white_ratio within the piece
    double piece_area = cv::countNonZero(piece_mask);
    double char_white = cv::countNonZero(char_bin);
    double wr = (piece_area > 0) ? (char_white / piece_area) : 0;

    // If strokes are too sparse or too dense, adjust
    if (wr < 0.03 || wr > 0.50) {
        // Try adaptive threshold within piece
        cv::adaptiveThreshold(inside_piece, char_bin, 255,
                              cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                              cv::THRESH_BINARY_INV, 31, 6);
        cv::bitwise_and(char_bin, piece_mask, char_bin);
    }

    // Morphological open to remove small noise
    cv::Mat open_k = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(char_bin, char_bin, cv::MORPH_OPEN, open_k);

    // Close small gaps within strokes
    cv::Mat close_k = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));
    cv::morphologyEx(char_bin, char_bin, cv::MORPH_CLOSE, close_k);

    // Find character contours
    std::vector<std::vector<cv::Point>> cnts;
    cv::findContours(char_bin, cnts, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (cnts.empty()) return false;

    // Filter by area: keep contours >= 2% of max area
    double max_ca = 0;
    for (const auto& c : cnts)
        max_ca = std::max(max_ca, cv::contourArea(c));

    std::vector<std::vector<cv::Point>> kept;
    for (const auto& c : cnts)
        if (cv::contourArea(c) >= max_ca * 0.02) kept.push_back(c);

    if (kept.empty()) return false;

    // Merge bounding boxes
    cv::Rect char_bbox = cv::boundingRect(kept[0]);
    for (size_t i = 1; i < kept.size(); i++)
        char_bbox |= cv::boundingRect(kept[i]);

    // Add 10% padding
    int pad_x = static_cast<int>(char_bbox.width * 0.10);
    int pad_y = static_cast<int>(char_bbox.height * 0.10);
    char_bbox.x = std::max(0, char_bbox.x - pad_x);
    char_bbox.y = std::max(0, char_bbox.y - pad_y);
    char_bbox.width  = std::min(denoised.cols - char_bbox.x, char_bbox.width  + 2 * pad_x);
    char_bbox.height = std::min(denoised.rows - char_bbox.y, char_bbox.height + 2 * pad_y);

    if (char_bbox.area() <= 0) return false;

    // Extract character ROI and do final Otsu binarization
    cv::Mat char_roi = denoised(char_bbox).clone();
    cv::Mat final_bin;
    cv::threshold(char_roi, final_bin, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    double fg_ratio = static_cast<double>(cv::countNonZero(final_bin))
                    / (char_roi.rows * char_roi.cols);
    if (fg_ratio > 0.55) {
        cv::bitwise_not(final_bin, final_bin);
    }

    // Normalize to 64×64, preserving aspect ratio
    double char_aspect = static_cast<double>(char_roi.cols) / char_roi.rows;
    int fit_w = 64, fit_h = 64;
    if (char_aspect > 1.0) {
        fit_h = static_cast<int>(64.0 / char_aspect);
        if (fit_h < 8) fit_h = 8;
    } else {
        fit_w = static_cast<int>(64.0 * char_aspect);
        if (fit_w < 8) fit_w = 8;
    }

    cv::Mat resized;
    cv::resize(final_bin, resized, cv::Size(fit_w, fit_h), 0, 0, cv::INTER_AREA);
    cv::threshold(resized, resized, 128, 255, cv::THRESH_BINARY);

    // Center in 64×64 with black padding
    int left = (64 - fit_w) / 2;
    int top  = (64 - fit_h) / 2;
    cv::Mat norm(64, 64, CV_8UC1, cv::Scalar(0));
    resized.copyTo(norm(cv::Rect(left, top, fit_w, fit_h)));

    out_norm = norm;
    out_score = score_template(norm, 0);  // char_id unknown at extraction time
    return !out_score.rejected;
}

// ---------- Multi-strategy fallback (simplified) ----------

static std::vector<Candidate> extract_fallback(const cv::Mat& piece_bgr) {
    std::vector<Candidate> cands;
    cv::Mat gray, denoised;
    cv::cvtColor(piece_bgr, gray, cv::COLOR_BGR2GRAY);
    cv::bilateralFilter(gray, denoised, 5, 40, 40);

    // Determine threshold direction from border darkness
    cv::Mat border_sample;
    int bw = std::min(8, std::min(denoised.rows, denoised.cols) / 20);
    if (bw < 1) bw = 1;
    double border_mean = (
        cv::mean(denoised(cv::Rect(0, 0, denoised.cols, bw)))[0] +
        cv::mean(denoised(cv::Rect(0, denoised.rows - bw, denoised.cols, bw)))[0] +
        cv::mean(denoised(cv::Rect(0, bw, bw, denoised.rows - 2 * bw)))[0] +
        cv::mean(denoised(cv::Rect(denoised.cols - bw, bw, bw, denoised.rows - 2 * bw)))[0]
    ) / 4.0;
    int ttype = (border_mean < 100) ? cv::THRESH_BINARY : cv::THRESH_BINARY_INV;

    // Try 3 block sizes for adaptive threshold
    int block_sizes[] = {51, 31, 21};
    for (int bs : block_sizes) {
        cv::Mat binary;
        cv::adaptiveThreshold(denoised, binary, 255,
                              cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                              ttype, bs | 1, 8);

        cv::Mat k = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
        cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, k);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        if (contours.empty()) continue;

        std::sort(contours.begin(), contours.end(),
                  [](const auto& a, const auto& b) { return cv::contourArea(a) > cv::contourArea(b); });

        int try_n = std::min(2, static_cast<int>(contours.size()));
        for (int kk = 0; kk < try_n; kk++) {
            // Use the contour's bounding rect as the piece region
            cv::Rect r = cv::boundingRect(contours[kk]);
            int px = static_cast<int>(r.width * 0.15);
            int py = static_cast<int>(r.height * 0.15);
            r.x = std::max(0, r.x - px);
            r.y = std::max(0, r.y - py);
            r.width  = std::min(piece_bgr.cols - r.x, r.width  + 2 * px);
            r.height = std::min(piece_bgr.rows - r.y, r.height + 2 * py);
            r &= cv::Rect(0, 0, piece_bgr.cols, piece_bgr.rows);
            if (r.area() <= 0) continue;

            cv::Mat crop = piece_bgr(r).clone();

            // Try with and without deskew
            for (bool dsk : {true, false}) {
                cv::Mat norm;
                QualityScore sc;
                if (extract_otsu(crop, norm, sc, dsk) && !sc.rejected) {
                    cands.push_back({norm, sc, dsk});
                }
            }
        }
    }

    // Also try Otsu on the full piece
    {
        cv::Mat norm;
        QualityScore sc;
        if (extract_otsu(piece_bgr, norm, sc, true) && !sc.rejected)
            cands.push_back({norm, sc, true});
        if (extract_otsu(piece_bgr, norm, sc, false) && !sc.rejected)
            cands.push_back({norm, sc, false});
    }

    return cands;
}

// ---------- Main extraction: detect piece, extract character ----------

static bool process_single_image(const std::string& image_path, int char_id,
                                  const std::string& output_dir, bool verbose) {
    cv::Mat image = cv::imread(image_path, cv::IMREAD_COLOR);
    if (image.empty()) {
        std::cerr << "  [FAIL] Cannot load: " << image_path << "\n";
        return false;
    }

    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    // --- Stage 1: Locate the white piece on black background ---
    cv::Mat piece_mask;
    cv::threshold(gray, piece_mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    // Clean up mask
    {
        cv::Mat k_close = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 7));
        cv::morphologyEx(piece_mask, piece_mask, cv::MORPH_CLOSE, k_close);
        cv::Mat k_open = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
        cv::morphologyEx(piece_mask, piece_mask, cv::MORPH_OPEN, k_open);
    }

    std::vector<std::vector<cv::Point>> piece_cnts;
    cv::findContours(piece_mask, piece_cnts, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (piece_cnts.empty()) {
        std::cerr << "  [FAIL] No piece found (Otsu)\n";
        return false;
    }

    // Filter contours: expect piece ~117×93 px
    double total_area = gray.rows * gray.cols;
    std::vector<cv::Rect> piece_rois;
    for (const auto& c : piece_cnts) {
        double area = cv::contourArea(c);
        if (area < total_area * 0.003) continue;   // skip tiny
        if (area > total_area * 0.55) continue;     // skip whole-frame
        cv::Rect r = cv::boundingRect(c);
        // Validate size against expected range (from data analysis)
        if (r.width < 95 || r.width > 140) continue;
        if (r.height < 75 || r.height > 115) continue;
        double aspect = static_cast<double>(r.width) / r.height;
        if (aspect < 0.8 || aspect > 1.6) continue;
        piece_rois.push_back(r);
    }

    if (piece_rois.empty()) {
        // Relax size filter and try again
        for (const auto& c : piece_cnts) {
            double area = cv::contourArea(c);
            if (area < total_area * 0.002) continue;
            if (area > total_area * 0.60) continue;
            cv::Rect r = cv::boundingRect(c);
            double aspect = static_cast<double>(r.width) / r.height;
            if (aspect < 0.5 || aspect > 2.0) continue;
            piece_rois.push_back(r);
        }
    }

    if (piece_rois.empty()) {
        std::cerr << "  [FAIL] No valid piece region after size filter\n";
        return false;
    }

    // Pick the largest valid piece
    std::sort(piece_rois.begin(), piece_rois.end(),
              [](const cv::Rect& a, const cv::Rect& b) { return a.area() > b.area(); });
    cv::Rect best_roi = piece_rois[0];

    if (verbose) {
        std::cerr << "  Piece ROI: " << best_roi.width << "x" << best_roi.height
                  << " at (" << best_roi.x << "," << best_roi.y << ")\n";
    }

    // Add 15% padding around the piece
    int pad_x = static_cast<int>(best_roi.width * 0.15);
    int pad_y = static_cast<int>(best_roi.height * 0.15);
    cv::Rect crop_rect(
        std::max(0, best_roi.x - pad_x),
        std::max(0, best_roi.y - pad_y),
        std::min(image.cols - std::max(0, best_roi.x - pad_x), best_roi.width + 2 * pad_x),
        std::min(image.rows - std::max(0, best_roi.y - pad_y), best_roi.height + 2 * pad_y));
    crop_rect &= cv::Rect(0, 0, image.cols, image.rows);

    cv::Mat piece_crop = image(crop_rect).clone();

    // --- Stage 2: Extract character using Otsu (primary strategy) ---
    std::vector<Candidate> candidates;

    // Try Otsu with deskew first, then without
    for (bool dsk : {true, false}) {
        cv::Mat norm;
        QualityScore sc;
        if (extract_otsu(piece_crop, norm, sc, dsk) && !sc.rejected) {
            // Re-score with char_id for size consistency
            sc = score_template(norm, char_id);
            if (!sc.rejected)
                candidates.push_back({norm, sc, dsk});
        }
    }

    // --- Stage 3: Fallback to multi-strategy if Otsu failed ---
    if (candidates.empty()) {
        if (verbose) std::cerr << "  Otsu failed, trying fallback...\n";
        candidates = extract_fallback(piece_crop);
    }

    if (candidates.empty()) {
        std::cerr << "  [FAIL] All extraction strategies failed\n";
        return false;
    }

    // Pick best candidate
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.score.total > b.score.total; });

    Candidate& best = candidates[0];

    if (best.score.total < 0.6) {
        std::cerr << "  [WARN] Low score (" << best.score.total
                  << "), saving anyway\n";
    }

    // --- Stage 4: Save template ---
    auto it = CHAR_NAMES.find(char_id);
    if (it == CHAR_NAMES.end()) {
        std::cerr << "  [FAIL] Invalid char_id: " << char_id << "\n";
        return false;
    }

    std::ostringstream dir_ss;
    dir_ss << std::setw(2) << std::setfill('0') << char_id << "_" << it->second;
    std::string full_dir = output_dir + "/" + dir_ss.str();
    fs::create_directories(full_dir);

    // Find next sample number
    int max_sample = 0;
    if (fs::exists(full_dir)) {
        for (const auto& entry : fs::directory_iterator(full_dir)) {
            std::string nm = entry.path().stem().string();
            if (nm.rfind("sample_", 0) == 0) {
                try {
                    max_sample = std::max(max_sample, std::stoi(nm.substr(7)));
                } catch (...) {}
            }
        }
    }

    std::ostringstream fn;
    fn << full_dir << "/sample_" << std::setw(3) << std::setfill('0')
       << (max_sample + 1) << ".png";
    cv::imwrite(fn.str(), best.norm);

    auto& sc = best.score;
    std::cout << "  Saved: " << fn.str()
              << " | white=" << sc.white_pixels << "/4096"
              << " | score=" << std::fixed << std::setprecision(3) << sc.total
              << " | cnt=" << sc.centering << " cln=" << sc.cleanliness
              << " | holes=" << sc.hole_count
              << " | deskew=" << best.deskewed << "\n";

    return true;
}

// ---------- entry point ----------

int main(int argc, char** argv) {
    bool batch = false;
    bool verbose = false;
    std::string image_path;
    int char_id = 0;
    std::string output_dir = "templates/";

    // Parse args
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--batch") batch = true;
        else if (arg == "--verbose") verbose = true;
        else if (arg == "--no-gui") { /* compatibility, ignored */ }
        else if (image_path.empty()) image_path = arg;
        else if (char_id == 0) {
            try { char_id = std::stoi(arg); }
            catch (...) { output_dir = arg; }
        }
        else output_dir = arg;
    }

    // Ensure trailing slash on output_dir
    if (!output_dir.empty() && output_dir.back() != '/') output_dir += '/';

    if (batch) {
        // Batch mode: process all .jpg files in image_path directory
        std::string input_dir = image_path.empty() ? "raw_photos/" : image_path;
        if (!fs::exists(input_dir) || !fs::is_directory(input_dir)) {
            std::cerr << "Not a directory: " << input_dir << "\n";
            return 1;
        }

        // Collect all .jpg files
        std::vector<std::string> files;
        for (const auto& entry : fs::directory_iterator(input_dir)) {
            if (entry.path().extension() == ".jpg" || entry.path().extension() == ".JPG")
                files.push_back(entry.path().string());
        }
        std::sort(files.begin(), files.end());

        if (files.empty()) {
            std::cerr << "No .jpg files found in " << input_dir << "\n";
            return 1;
        }

        std::cerr << "Batch processing " << files.size() << " images...\n\n";

        // --- Pass 1: Extract all templates ---
        struct Result {
            std::string path;
            std::string saved_path;
            int char_id;
            int white_pixels;
            double total_score;
            bool ok;
        };
        std::vector<Result> results;

        int ok_count = 0, fail_count = 0, fallback_count = 0;
        for (const auto& f : files) {
            int cid = parse_char_id_from_filename(f);
            if (cid == 0) {
                std::cerr << "SKIP (cannot parse char_id): " << f << "\n";
                fail_count++;
                continue;
            }

            std::cerr << "[" << (ok_count + fail_count + 1) << "/" << files.size() << "] "
                      << fs::path(f).filename().string() << " (id=" << cid << ")\n";

            if (process_single_image(f, cid, output_dir, verbose)) {
                ok_count++;
            } else {
                fail_count++;
            }
        }

        // --- Pass 2: Learn per-character median white area ---
        // Collect all saved templates' white pixel counts per char_id
        std::map<int, std::vector<int>> per_char_white;
        for (const auto& entry : fs::recursive_directory_iterator(output_dir)) {
            if (entry.path().extension() != ".png") continue;
            // Parse char_id from parent directory name "01_司令"
            std::string parent = entry.path().parent_path().filename().string();
            std::regex id_re(R"((\d+)_)");
            std::smatch m;
            if (std::regex_search(parent, m, id_re)) {
                int cid = std::stoi(m[1].str());
                cv::Mat tmpl = cv::imread(entry.path().string(), cv::IMREAD_GRAYSCALE);
                if (!tmpl.empty()) {
                    per_char_white[cid].push_back(cv::countNonZero(tmpl));
                }
            }
        }

        std::cerr << "\n--- Per-character statistics ---\n";
        for (auto& [cid, areas] : per_char_white) {
            std::sort(areas.begin(), areas.end());
            double median = areas[areas.size() / 2];
            expected_white_area[cid] = median;
            double sum = std::accumulate(areas.begin(), areas.end(), 0.0);
            double mean = sum / areas.size();
            double std_dev = 0;
            for (int a : areas) std_dev += (a - mean) * (a - mean);
            std_dev = std::sqrt(std_dev / areas.size());
            double cv = (mean > 0) ? (std_dev / mean * 100) : 0;
            auto name_it = CHAR_NAMES.find(cid);
            std::cerr << "  " << (name_it != CHAR_NAMES.end() ? name_it->second : "??")
                      << " (id=" << cid << "): " << areas.size() << " templates"
                      << " | median=" << static_cast<int>(median)
                      << " | CV=" << std::fixed << std::setprecision(1) << cv << "%\n";
        }

        // --- Pass 3: Re-score and remove low-quality templates ---
        std::cerr << "\n--- Re-scoring with learned sizes ---\n";
        int kept = 0, removed = 0;
        for (auto& [cid, _] : per_char_white) {
            auto name_it = CHAR_NAMES.find(cid);
            std::ostringstream dss;
            dss << std::setw(2) << std::setfill('0') << cid << "_" << name_it->second;
            std::string dir_name = dss.str();
            std::string char_dir = output_dir + dir_name;

            if (!fs::exists(char_dir)) continue;

            // Collect and re-score all samples
            struct ScoredTpl {
                std::string path;
                double score;
            };
            std::vector<ScoredTpl> tpls;

            for (const auto& entry : fs::directory_iterator(char_dir)) {
                if (entry.path().extension() != ".png") continue;
                cv::Mat tmpl = cv::imread(entry.path().string(), cv::IMREAD_GRAYSCALE);
                if (tmpl.empty()) continue;
                auto sc = score_template(tmpl, cid);
                tpls.push_back({entry.path().string(), sc.total});
            }

            std::sort(tpls.begin(), tpls.end(),
                      [](const ScoredTpl& a, const ScoredTpl& b) { return a.score > b.score; });

            // Keep top 8 per character (or all if fewer)
            int keep_n = std::min(8, static_cast<int>(tpls.size()));
            for (int i = 0; i < static_cast<int>(tpls.size()); i++) {
                if (i < keep_n) {
                    kept++;
                    if (verbose) {
                        std::cerr << "  KEEP " << fs::path(tpls[i].path).filename().string()
                                  << " score=" << tpls[i].score << "\n";
                    }
                } else {
                    fs::remove(tpls[i].path);
                    removed++;
                }
            }
        }

        // --- Final summary ---
        std::cerr << "\n========================================\n";
        std::cerr << "  Batch Complete\n";
        std::cerr << "========================================\n";
        std::cerr << "Total images:     " << files.size() << "\n";
        std::cerr << "Extracted OK:     " << ok_count << "\n";
        std::cerr << "Failed:           " << fail_count << "\n";
        std::cerr << "Templates kept:   " << kept << "\n";
        std::cerr << "Low-score removed:" << removed << "\n";

    } else {
        // Single-image mode
        if (image_path.empty()) {
            std::cerr << "Usage: capture_template [--batch] [--verbose] <image_path> [char_id] [output_dir]\n"
                      << "  char_id auto-detected from filename if omitted.\n"
                      << "  --batch: process all .jpg files in <image_path> directory.\n";
            return 1;
        }

        if (char_id == 0)
            char_id = parse_char_id_from_filename(image_path);

        if (char_id < 1 || char_id > 12) {
            std::cerr << "Cannot determine char_id from filename: " << image_path << "\n"
                      << "  Expected format: 红-司令-01.jpg or 黑_军长_03.jpg\n";
            return 1;
        }

        std::cerr << "Processing: " << image_path << " (char_id=" << char_id << ")\n";
        bool ok = process_single_image(image_path, char_id, output_dir, verbose);
        return ok ? 0 : 1;
    }

    return 0;
}
