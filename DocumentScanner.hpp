#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

class DocumentScanner {
private:
    const float TARGET_WIDTH = 800.0f;
    const float TARGET_HEIGHT = 1130.0f;
    std::vector<cv::Point2f> dst_pts;

    // --- KHÔNG CẦN CÁC BIẾN GPU NỮA ---
    // Thay vào đó ta dùng biến cục bộ trong hàm để tiết kiệm RAM

    // Hàm phụ trợ
    std::vector<cv::Point2f> orderPoints(const std::vector<cv::Point>& pts);

    // Xử lý ảnh trên CPU
    void autoBrightness(cv::Mat& img);
    void sharpenImage(cv::Mat& img);

public:
    DocumentScanner();

    // Hàm tìm giấy (Trả về ảnh Threshold để debug và danh sách điểm)
    cv::Mat detectDocument(const cv::Mat& src_frame, std::vector<cv::Point>& detected_points);

    // Hàm cắt ảnh và xử lý hậu kỳ
    cv::Mat getWarpedImage(const cv::Mat& src_frame, const std::vector<cv::Point>& points);

    // Lưu file
    bool saveHighQualityDoc(const cv::Mat& doc_img, const std::string& filenameBase);

	// Lưu PDF
    bool saveDocToPDF(const cv::Mat& doc_img, const std::string& filename);

    // Hàm mở hộp thoại chọn vị trí lưu file
    static std::string getSaveFilePath(const std::string& defaultName, const std::string& filter, const std::string& defaultExt);
};