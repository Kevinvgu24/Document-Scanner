#include "DocumentScanner.hpp"
#include <iostream>
#include <numeric> 
#include <fstream>
#include <windows.h>
#include <commdlg.h>

using namespace cv;
using namespace std;

DocumentScanner::DocumentScanner() {
    // Khởi tạo điểm đích mặc định (Khổ A4)
    dst_pts = { {0, 0}, {TARGET_WIDTH, 0}, {TARGET_WIDTH, TARGET_HEIGHT}, {0, TARGET_HEIGHT} };
}

vector<Point2f> DocumentScanner::orderPoints(const vector<Point>& pts) {
    vector<Point2f> rect(4);
    vector<int> sum_pts, diff_pts;
    for (const auto& p : pts) {
        sum_pts.push_back(p.x + p.y);
        diff_pts.push_back(p.y - p.x);
    }
    // Top-left: tổng nhỏ nhất
    rect[0] = pts[distance(sum_pts.begin(), min_element(sum_pts.begin(), sum_pts.end()))];
    // Bottom-right: tổng lớn nhất
    rect[2] = pts[distance(sum_pts.begin(), max_element(sum_pts.begin(), sum_pts.end()))];
    // Top-right: hiệu nhỏ nhất
    rect[1] = pts[distance(diff_pts.begin(), min_element(diff_pts.begin(), diff_pts.end()))];
    // Bottom-left: hiệu lớn nhất
    rect[3] = pts[distance(diff_pts.begin(), max_element(diff_pts.begin(), diff_pts.end()))];
    return rect;
}

//Bộ lọc (filter) cho tài liệu scan
void DocumentScanner::applyScannerFilter(Mat& img) {
    if (img.empty()) return;

    //Dùng Grayscale 
    if (img.channels() == 3) {
        cvtColor(img, img, COLOR_BGR2GRAY);
    }

    //Tăng độ phân giải (Super-Resolution) để làm mịn nét
    resize(img, img, Size(), 2.0, 2.0, INTER_CUBIC);

    //Xóa bóng đổ (Shadow Removal) - Làm trắng nền
    Mat bg;
    GaussianBlur(img, bg, Size(51, 51), 0);
    divide(img, bg, img, 255.0);

    //Làm đậm nét 
    Mat element = getStructuringElement(MORPH_RECT, Size(2, 2));
    erode(img, img, element);

    //Cắt ngưỡng (Threshold)
    adaptiveThreshold(img, img, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 11, 3);

    //Thu nhỏ về kích thước gốc (Khử răng cưa)
    resize(img, img, Size(), 0.5, 0.5, INTER_AREA);
}

Mat DocumentScanner::detectDocument(const Mat& src_frame, vector<Point>& detected_points) {
    if (src_frame.empty()) return Mat();

    Mat imgGray, imgBlur, imgCanny, imgDilate;
    cvtColor(src_frame, imgGray, COLOR_BGR2GRAY);
    GaussianBlur(imgGray, imgBlur, Size(5, 5), 0);
    Canny(imgBlur, imgCanny, 50, 150);

    Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
    dilate(imgCanny, imgDilate, kernel);

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
    return imgDilate;
}

//Hàm cắt ảnh và xử lý vị trí 
Mat DocumentScanner::getWarpedImage(const Mat& src_frame, const vector<Point>& points) {
    if (points.size() != 4) return Mat();

    vector<Point2f> ordered_pts = orderPoints(points);

    //Tính kích thước khung hình
    float widthTop = (float)norm(ordered_pts[0] - ordered_pts[1]);
    float widthBottom = (float)norm(ordered_pts[3] - ordered_pts[2]);
    float maxWidth = max(widthTop, widthBottom);

    float heightLeft = (float)norm(ordered_pts[0] - ordered_pts[3]);
    float heightRight = (float)norm(ordered_pts[1] - ordered_pts[2]);
    float maxHeight = max(heightLeft, heightRight);

    vector<Point2f> dynamic_dst_pts;
    Size outputSize;

	// Hàm xử lý tự động xoay ảnh dựa trên tỷ lệ khung hình
    if (maxWidth > maxHeight) {
        // Nếu phát hiện hình nằm ngang ==> Warp ra ngang
        dynamic_dst_pts = { {0, 0}, {TARGET_HEIGHT, 0}, {TARGET_HEIGHT, TARGET_WIDTH}, {0, TARGET_WIDTH} };
        outputSize = Size((int)TARGET_HEIGHT, (int)TARGET_WIDTH);
    }
    else {
        // Nếu phát hiện hình nằm dọc ==> Warp ra dọc
        dynamic_dst_pts = { {0, 0}, {TARGET_WIDTH, 0}, {TARGET_WIDTH, TARGET_HEIGHT}, {0, TARGET_HEIGHT} };
        outputSize = Size((int)TARGET_WIDTH, (int)TARGET_HEIGHT);
    }

    Mat M = getPerspectiveTransform(ordered_pts, dynamic_dst_pts);
    Mat imgWarped;

    // Dùng INTER_LANCZOS4 (openCV) để ảnh nét nhất khi zoom
    warpPerspective(src_frame, imgWarped, M, outputSize, INTER_LANCZOS4);

    if (imgWarped.cols > imgWarped.rows) {
        rotate(imgWarped, imgWarped, ROTATE_90_CLOCKWISE);
    }

    // Áp dụng bộ lọc làm nét
    applyScannerFilter(imgWarped);
    return imgWarped;
}

//Hàm lưu file PNG chất lượng cao
bool DocumentScanner::saveHighQualityDoc(const Mat& doc_img, const string& filenameBase) {
    if (doc_img.empty()) return false;
    string finalName = filenameBase + ".png";
    vector<int> compression_params;
    compression_params.push_back(IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(9);
    return imwrite(finalName, doc_img, compression_params);
}

bool DocumentScanner::saveDocToPDF(const Mat& doc_img, const string& filename) {
    if (doc_img.empty()) return false;

    vector<uchar> buf;
    vector<int> params = { IMWRITE_JPEG_QUALITY, 90 };
    imencode(".jpg", doc_img, buf, params);

    ofstream pdf(filename, ios::binary);
    if (!pdf.is_open()) return false;

    int width = doc_img.cols;
    int height = doc_img.rows;

    pdf << "%PDF-1.4\n";
    vector<streampos> offsets;

    offsets.push_back(pdf.tellp());
    pdf << "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";

    offsets.push_back(pdf.tellp());
    pdf << "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n";

    offsets.push_back(pdf.tellp());
    pdf << "3 0 obj\n<< /Type /Page /Parent 2 0 R /Resources << /XObject << /Im1 4 0 R >> >> /MediaBox [0 0 "
        << width << " " << height << "] /Contents 5 0 R >>\nendobj\n";

    offsets.push_back(pdf.tellp());
    pdf << "4 0 obj\n<< /Type /XObject /Subtype /Image /Width " << width
        << " /Height " << height << " /ColorSpace /DeviceGray /BitsPerComponent 8 "
        << "/Filter /DCTDecode /Length " << buf.size() << " >>\nstream\n";
    pdf.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    pdf << "\nendstream\nendobj\n";

    string streamData = "q " + to_string(width) + " 0 0 " + to_string(height) + " 0 0 cm /Im1 Do Q";
    offsets.push_back(pdf.tellp());
    pdf << "5 0 obj\n<< /Length " << streamData.length() << " >>\nstream\n" << streamData << "\nendstream\nendobj\n";

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

string DocumentScanner::getSaveFilePath(const string& defaultName, const string& filter, const string& defaultExt) {
    OPENFILENAMEA ofn;
    char szFile[260] = { 0 };
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
    if (GetSaveFileNameA(&ofn) == TRUE) return string(ofn.lpstrFile);
    return "";
}
