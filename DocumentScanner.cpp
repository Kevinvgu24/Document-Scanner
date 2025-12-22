#include "DocumentScanner.hpp"
#include <iostream>

using namespace cv;
using namespace std;

//Define all the constructors and functions here (all using Nvdia CUDA GPU acceleration)  

DocumentScanner::DocumentScanner() {
    dst_pts = { {0, 0}, {TARGET_WIDTH, 0}, {TARGET_WIDTH, TARGET_HEIGHT}, {0, TARGET_HEIGHT} };

    // 1. Init Gaussian (GPU)
    d_gaussian_filter = cuda::createGaussianFilter(CV_8UC1, CV_8UC1, Size(5, 5), 0);

    // 2. Init Canny (GPU)
    d_canny_detector = cuda::createCannyEdgeDetector(50, 150);

    // 3. Init Dilate (GPU)
    Mat kernel_dilate = getStructuringElement(MORPH_RECT, Size(3, 3));
    d_dilate_filter = cuda::createMorphologyFilter(MORPH_DILATE, CV_8UC1, kernel_dilate);

    // 4. Init Sharpen (GPU)
    Mat kernel_sharpen = (Mat_<float>(3, 3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);
    d_sharpen_filter = cuda::createLinearFilter(CV_8UC1, CV_8UC1, kernel_sharpen);
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

void DocumentScanner::autoBrightnessGPU(cuda::GpuMat& d_img) {
    if (d_img.empty()) return;
    Scalar total_sum = cuda::sum(d_img);
    double pixel_count = (double)d_img.cols * d_img.rows;
    double current_mean = total_sum.val[0] / pixel_count;
    double target_mean = 200.0;
    double offset = target_mean - current_mean;
    // Phép cộng song song trên toàn bộ ma trận pixel
    cuda::add(d_img, Scalar(offset), d_img);
}

void DocumentScanner::sharpenImageGPU(cuda::GpuMat& d_img) {
    if (d_img.empty()) return;
    d_sharpen_filter->apply(d_img, d_img);
}

// --- HÀM TRỌNG TÂM: XỬ LÝ ẢNH TRÊN GPU ĐỂ TÌM GIẤY ---
Mat DocumentScanner::detectDocument(const Mat& src_frame, vector<Point>& detected_points) {
    if (src_frame.empty()) return Mat();

    // 1. Upload lên GPU (Tốn ít thời gian nhất có thể)
    d_src.upload(src_frame);

    // 2. Chuyển xám và Blur (GPU)
    cuda::cvtColor(d_src, d_gray, COLOR_BGR2GRAY);
    d_gaussian_filter->apply(d_gray, d_blurred);

    // 3. Canny Edge (GPU)
    d_canny_detector->detect(d_blurred, d_canny);

    // 4. Dilate (GPU) - Nối liền các nét đứt
    d_dilate_filter->apply(d_canny, d_dilated);

    // 5. Download về CPU để tìm Contours 
    // (OpenCV chưa hỗ trợ findContours tốt trên GPU, nên đoạn này phải về CPU)
    Mat imgThreshold;
    d_dilated.download(imgThreshold);

    // 6. Tìm Contour lớn nhất (Logic CPU cũ của bạn)
    vector<vector<Point>> contours;
    findContours(imgThreshold, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

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

    return imgThreshold; // Trả về để hiện thị ở cửa sổ Debug
}

// Hàm cắt ảnh (Chỉ gọi khi cần lưu, tận dụng GPU tối đa)

Mat DocumentScanner::getWarpedImage(const Mat& src_frame, const vector<Point>& points) {
    if (points.size() != 4) return Mat();

    d_src.upload(src_frame);

    // 1. Sắp xếp lại thứ tự điểm (TopLeft, TopRight, BottomRight, BottomLeft)
    vector<Point2f> ordered_pts = orderPoints(points);

    // 2. Tính chiều rộng và chiều cao thực tế của vùng chọn (dùng định lý Pytago/Euclidean distance)
    // Chiều rộng trên: khoảng cách giữa TL và TR
    float widthTop = (float)norm(ordered_pts[0] - ordered_pts[1]);
    // Chiều rộng dưới: khoảng cách giữa BL và BR
    float widthBottom = (float)norm(ordered_pts[3] - ordered_pts[2]);
    float maxWidth = max(widthTop, widthBottom);

    // Chiều cao trái: khoảng cách giữa TL và BL
    float heightLeft = (float)norm(ordered_pts[0] - ordered_pts[3]);
    // Chiều cao phải: khoảng cách giữa TR và BR
    float heightRight = (float)norm(ordered_pts[1] - ordered_pts[2]);
    float maxHeight = max(heightLeft, heightRight);

    // 3. Tự động xác định khung đích (Destination) dựa trên tỷ lệ thực tế
    // Nếu ảnh thực tế là Ngang (Rộng > Cao) -> Đích cũng phải là Ngang
    vector<Point2f> dynamic_dst_pts;

    // Tỉ lệ giấy A4 chuẩn là ~1.414
    // Chúng ta sẽ dùng kích thước cố định để ảnh nét, nhưng xoay chiều dựa trên hình dáng
    if (maxWidth > maxHeight) {
        // GIẤY NGANG (Landscape) -> Set đích thành 1130 x 800
        dynamic_dst_pts = {
            {0, 0},
            {TARGET_HEIGHT, 0},
            {TARGET_HEIGHT, TARGET_WIDTH},
            {0, TARGET_WIDTH}
        };
    }
    else {
        // GIẤY DỌC (Portrait) -> Set đích thành 800 x 1130
        dynamic_dst_pts = {
            {0, 0},
            {TARGET_WIDTH, 0},
            {TARGET_WIDTH, TARGET_HEIGHT},
            {0, TARGET_HEIGHT}
        };
    }

    // 4. Warp Perspective (Biến đổi hình học)
    Mat M = getPerspectiveTransform(ordered_pts, dynamic_dst_pts);

    // Kích thước ảnh đầu ra phải khớp với dynamic_dst_pts
    Size outputSize = (maxWidth > maxHeight) ? Size(TARGET_HEIGHT, TARGET_WIDTH) : Size(TARGET_WIDTH, TARGET_HEIGHT);

    cuda::warpPerspective(d_src, d_warped_rgb, M, outputSize);

    // 5. Hậu xử lý (Làm đẹp ảnh)
    cuda::cvtColor(d_warped_rgb, d_warped_gray, COLOR_BGR2GRAY);
    autoBrightnessGPU(d_warped_gray);
    sharpenImageGPU(d_warped_gray);

    Mat result;
    d_warped_gray.download(result);
    return result;
}

bool DocumentScanner::saveHighQualityDoc(const Mat& doc_img, const string& filenameBase) {
    if (doc_img.empty()) return false;
    string finalName = filenameBase + ".png";
    return imwrite(finalName, doc_img);
}