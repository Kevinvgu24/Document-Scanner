//Run at release mode and x64

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/cudaarithm.hpp> // Thư viện toán học CUDA cơ bản

int main() {
    std::cout << "Dang kiem tra ket noi voi RTX 4060..." << std::endl;

    try {
        // 1. Kiểm tra số lượng thiết bị CUDA
        int device_count = cv::cuda::getCudaEnabledDeviceCount();

        if (device_count > 0) {
            std::cout << "=== THANH CONG! ===" << std::endl;
            std::cout << "So luong GPU tim thay: " << device_count << std::endl;

            // 2. Lấy thông tin chi tiết
            cv::cuda::printCudaDeviceInfo(0);

            std::cout << "\nXin chuc mung! OpenCV da san sang chay tren GPU." << std::endl;
        }
        else {
            std::cout << "=== THAT BAI ===" << std::endl;
            std::cout << "Khong tim thay thiet bi CUDA. Kiem tra lai Driver hoac qua trinh Build." << std::endl;
        }
    }
    catch (const cv::Exception& e) {
        std::cout << "Loi (Exception): " << e.what() << std::endl;
    }

    std::cout << "\nNhan Enter de thoat...";
    std::cin.get();
    return 0;
}