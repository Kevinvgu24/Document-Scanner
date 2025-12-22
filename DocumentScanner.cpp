#include "DocumentScanner.hpp"
#include <iostream>
#include <numeric> 
#include <fstream>
#include <windows.h>
#include <commdlg.h>

using namespace cv;
using namespace std;

DocumentScanner::DocumentScanner() {
    // Khởi tạo 4 điểm đích chuẩn (dùng cho Warp Perspective)
    dst_pts = { {0, 0}, {TARGET_WIDTH, 0}, {TARGET_WIDTH, TARGET_HEIGHT}, {0, TARGET_HEIGHT} };

    // Trên CPU, chúng ta không cần khởi tạo trước các bộ lọc (Filter) như trên GPU.
    // Chúng ta sẽ gọi trực tiếp các hàm xử lý ảnh khi cần.
}

vector<Point2f> DocumentScanner::orderPoints(const vector<Point>& pts) {
    vector<Point2f> rect(4);
    vector<int> sum_pts, diff_pts;
    for (const auto& p : pts) {
        sum_pts.push_back(p.x + p.y);
        diff_pts.push_back(p.y - p.x);
    }
    rect[0] = pts[distance(sum_pts.begin(), min_element(sum_pts.begin(), sum_pts.end()))];
    rect[2] = pts[distance(sum_pts.begin(), max_element(sum_pts.begin(), sum_pts.end()))];
    rect[1] = pts[distance(diff_pts.begin(), min_element(diff_pts.begin(), diff_pts.end()))];
    rect[3] = pts[distance(diff_pts.begin(), max_element(diff_pts.begin(), diff_pts.end()))];
    return rect;
}

void DocumentScanner::autoBrightness(Mat& img) {
    if (img.empty()) return;

    // Tính trung bình độ sáng
    Scalar mean_val = mean(img);
    double current_mean = mean_val.val[0];
    double target_mean = 200.0;
    double offset = target_mean - current_mean;

    // Cộng offset vào toàn bộ ảnh (tương đương cuda::add)
    // Dùng saturate_cast để đảm bảo giá trị nằm trong khoảng 0-255
    img.convertTo(img, -1, 1, offset);
}

void DocumentScanner::sharpenImage(Mat& img) {
    if (img.empty()) return;

    // Tạo kernel làm nét thủ công
    Mat kernel = (Mat_<float>(3, 3) << 0, -1, 0,
        -1, 5, -1,
        0, -1, 0);

    // Sử dụng filter2D trên CPU thay vì d_sharpen_filter->apply
    filter2D(img, img, img.depth(), kernel);
}

// Hàm mở hộp thoại chọn vị trí lưu file
string DocumentScanner::getSaveFilePath(const string& defaultName, const string& filter, const string& defaultExt) {
    OPENFILENAMEA ofn;
    char szFile[260] = { 0 };
    
    // Copy tên file mặc định
    strncpy_s(szFile, defaultName.c_str(), sizeof(szFile) - 1);

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrDefExt = defaultExt.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn) == TRUE) {
        return string(ofn.lpstrFile);
    }
    
    return ""; // Người dùng hủy
}

// --- LOGIC XỬ LÝ CHÍNH TRÊN CPU ---
Mat DocumentScanner::detectDocument(const Mat& src_frame, vector<Point>& detected_points) {
    if (src_frame.empty()) return Mat();

    Mat imgGray, imgBlur, imgCanny, imgDilate;

    // 1. Chuyển xám
    cvtColor(src_frame, imgGray, COLOR_BGR2GRAY);

    // 2. Gaussian Blur (Giảm nhiễu)
    GaussianBlur(imgGray, imgBlur, Size(5, 5), 0);

    // 3. Canny Edge Detection
    Canny(imgBlur, imgCanny, 50, 150);

    // 4. Dilate (Phình to nét đứt)
    Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
    dilate(imgCanny, imgDilate, kernel);

    // 5. Tìm Contours
    vector<vector<Point>> contours;
    findContours(imgDilate, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    double maxArea = 0;
    detected_points.clear();

    for (const auto& c : contours) {
        double area = contourArea(c);
        if (area > 5000) {
            double peri = arcLength(c, true);
            vector<Point> approx;
            approxPolyDP(c, approx, 0.02 * peri, true);

            if (area > maxArea && approx.size() == 4) {
                detected_points = approx;
                maxArea = area;
            }
        }
    }

    return imgDilate; // Trả về ảnh nhị phân để hiển thị debug
}

Mat DocumentScanner::getWarpedImage(const Mat& src_frame, const vector<Point>& points) {
    if (points.size() != 4) return Mat();

    // 1. Sắp xếp lại thứ tự điểm
    vector<Point2f> ordered_pts = orderPoints(points);

    // 2. Tính kích thước thực tế (Logic giữ nguyên như cũ)
    float widthTop = (float)norm(ordered_pts[0] - ordered_pts[1]);
    float widthBottom = (float)norm(ordered_pts[3] - ordered_pts[2]);
    float maxWidth = max(widthTop, widthBottom);

    float heightLeft = (float)norm(ordered_pts[0] - ordered_pts[3]);
    float heightRight = (float)norm(ordered_pts[1] - ordered_pts[2]);
    float maxHeight = max(heightLeft, heightRight);

    // 3. Xác định khung đích
    vector<Point2f> dynamic_dst_pts;
    if (maxWidth > maxHeight) {
        // Giấy ngang
        dynamic_dst_pts = {
            {0, 0}, {TARGET_HEIGHT, 0}, {TARGET_HEIGHT, TARGET_WIDTH}, {0, TARGET_WIDTH}
        };
    }
    else {
        // Giấy dọc
        dynamic_dst_pts = {
            {0, 0}, {TARGET_WIDTH, 0}, {TARGET_WIDTH, TARGET_HEIGHT}, {0, TARGET_HEIGHT}
        };
    }

    // 4. Warp Perspective (Trên CPU)
    Mat M = getPerspectiveTransform(ordered_pts, dynamic_dst_pts);
    Size outputSize = (maxWidth > maxHeight) ? Size(TARGET_HEIGHT, TARGET_WIDTH) : Size(TARGET_WIDTH, TARGET_HEIGHT);

    Mat imgWarped;
    warpPerspective(src_frame, imgWarped, M, outputSize);

    // 5. Hậu xử lý (Làm đẹp ảnh)
    Mat imgProcessed;
    cvtColor(imgWarped, imgProcessed, COLOR_BGR2GRAY);

    autoBrightness(imgProcessed);
    sharpenImage(imgProcessed);

    return imgProcessed;
}

bool DocumentScanner::saveHighQualityDoc(const Mat& doc_img, const string& filenameBase) {
    if (doc_img.empty()) return false;
    string finalName = filenameBase + ".png";
    return imwrite(finalName, doc_img);
}

bool DocumentScanner::saveDocToPDF(const Mat& doc_img, const string& filename) {
    if (doc_img.empty()) return false;

    // 1. Encode ảnh sang JPEG buffer (PDF thích JPEG)
    vector<uchar> buf;
    vector<int> params = { IMWRITE_JPEG_QUALITY, 90 }; // Chất lượng ảnh 90%
    imencode(".jpg", doc_img, buf, params);

    // 2. Mở file để ghi (Binary mode)
    ofstream pdf(filename, ios::binary);
    if (!pdf.is_open()) return false;

    int width = doc_img.cols;
    int height = doc_img.rows;

    // 3. Header PDF
    pdf << "%PDF-1.4\n";

    // Các biến đếm offset cho file PDF (Cross-reference table)
    vector<streampos> offsets;
    
    // Object 1: Catalog
    offsets.push_back(pdf.tellp());
    pdf << "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";

    // Object 2: Page Tree
    offsets.push_back(pdf.tellp());
    pdf << "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n";

    // Object 3: Page (Thiết lập kích thước trang = kích thước ảnh)
    offsets.push_back(pdf.tellp());
    pdf << "3 0 obj\n<< /Type /Page /Parent 2 0 R /Resources << /XObject << /Im1 4 0 R >> >> /MediaBox [0 0 "
        << width << " " << height << "] /Contents 5 0 R >>\nendobj\n";

    // Object 4: Image XObject (Nhúng dữ liệu JPEG)
    offsets.push_back(pdf.tellp());
    pdf << "4 0 obj\n<< /Type /XObject /Subtype /Image /Width " << width
        << " /Height " << height << " /ColorSpace /DeviceGray /BitsPerComponent 8 "
        << "/Filter /DCTDecode /Length " << buf.size() << " >>\nstream\n";

    // Ghi dữ liệu binary của ảnh
    pdf.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    pdf << "\nendstream\nendobj\n";

    // Object 5: Content Stream (Vẽ ảnh lên trang PDF)
    // Lệnh: q (save state) -> width 0 0 height 0 0 cm (scale to fit) -> /Im1 Do (draw image) -> Q (restore state)
    string streamData = "q " + to_string(width) + " 0 0 " + to_string(height) + " 0 0 cm /Im1 Do Q";
    offsets.push_back(pdf.tellp());
    pdf << "5 0 obj\n<< /Length " << streamData.length() << " >>\nstream\n" << streamData << "\nendstream\nendobj\n";

    // 4. Footer (XREF & Trailer)
    streampos xref_offset = pdf.tellp();
    pdf << "xref\n0 " << (offsets.size() + 1) << "\n0000000000 65535 f \n";
    for (streampos o : offsets) {
        char buffer[21];
        snprintf(buffer, sizeof(buffer), "%010lld 00000 n \n", static_cast<long long>(o));
        pdf << buffer;
    }

    pdf << "trailer\n<< /Size " << (offsets.size() + 1) << " /Root 1 0 R >>\n";
    pdf << "startxref\n" << static_cast<long long>(xref_offset) << "\n%%EOF\n";

    pdf.close();
    return true;
}