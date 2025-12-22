
//This program run on laptop with GPU (4060 with 8G vRam) support to scan documents using OpenCV's CUDA module.
//
#pragma once
#include <opencv2/opencv.hpp>       // OpenCV lib
#include <opencv2/cudaimgproc.hpp> 
#include <opencv2/cudafilters.hpp> 
#include <opencv2/cudawarping.hpp> 
#include <opencv2/cudaarithm.hpp>  

class DocumentScanner {
private:
    const float TARGET_WIDTH = 800.0f;
    const float TARGET_HEIGHT = 1130.0f;
    std::vector<cv::Point2f> dst_pts;

    // GPU
    cv::cuda::GpuMat d_src, d_gray, d_blurred, d_canny, d_dilated;
    cv::cuda::GpuMat d_warped_rgb, d_warped_gray;

	// Constructors for CUDA filters
    cv::Ptr<cv::cuda::Filter> d_gaussian_filter;
    cv::Ptr<cv::cuda::CannyEdgeDetector> d_canny_detector;
    cv::Ptr<cv::cuda::Filter> d_dilate_filter; // Bộ lọc phình to nét
    cv::Ptr<cv::cuda::Filter> d_sharpen_filter;

	// Support functions
    std::vector<cv::Point2f> orderPoints(const std::vector<cv::Point>& pts);
    void autoBrightnessGPU(cv::cuda::GpuMat& d_img);
    void sharpenImageGPU(cv::cuda::GpuMat& d_img);

public:
    DocumentScanner();

	// Warped_points
	// Return warped image and detected points
    cv::Mat detectDocument(const cv::Mat& src_frame, std::vector<cv::Point>& detected_points);

	// Take the points and return the warped image
    cv::Mat getWarpedImage(const cv::Mat& src_frame, const std::vector<cv::Point>& points);

    // Save file
    bool saveHighQualityDoc(const cv::Mat& doc_img, const std::string& filenameBase);
};