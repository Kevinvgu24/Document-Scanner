#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

class DocumentScanner {
private:
    // A4 paper size at high DPI (for print quality)
    const float TARGET_WIDTH = 2480.0f;
    const float TARGET_HEIGHT = 3508.0f;
    std::vector<cv::Point2f> dst_pts;

    // Helper: Sort points (Top-Left, Top-Right, Bottom-Right, Bottom-Left)
    std::vector<cv::Point2f> orderPoints(const std::vector<cv::Point>& pts);

    // Filter: The core algorithm to remove grid lines and enhance text
    void applyScannerFilter(cv::Mat& img);

public:
    DocumentScanner();

    // Tìm góc và khoanh vùng tài liệu
    cv::Mat detectDocument(const cv::Mat& src_frame, std::vector<cv::Point>& detected_points);

	// Bọc ảnh dựa trên các điểm đã phát hiện
    cv::Mat getWarpedImage(const cv::Mat& src_frame, const std::vector<cv::Point>& points);

    // Lưu ảnh png
    bool saveHighQualityDoc(const cv::Mat& doc_img, const std::string& filenameBase);

    // Lưu file pdf
    bool saveDocToPDF(const cv::Mat& doc_img, const std::string& filename);

	// Mở hộp thoại lưu file
    static std::string getSaveFilePath(const std::string& defaultName, const std::string& filter, const std::string& defaultExt);
};
