#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include "DocumentScanner.hpp" 

using namespace cv;
using namespace std;

int main() {
	// 1. Initialize Document Scanner with CUDA
    cout << "Gernating CUDA..." << endl;
    DocumentScanner scanner;
    cout << "CUDA ready!" << endl;

    // 2. Open Camera
	VideoCapture cap(1, CAP_DSHOW); // (Use 0 for using camera on latop or 1 or 2 for external carema)
    if (!cap.isOpened()) {
        cout << "Can not open the Camera!" << endl;
        return -1;
    }

    cap.set(CAP_PROP_FRAME_WIDTH, 1920);
    cap.set(CAP_PROP_FRAME_HEIGHT, 1080);

    double width = cap.get(CAP_PROP_FRAME_WIDTH);
    double height = cap.get(CAP_PROP_FRAME_HEIGHT);

    cout << "Do phan giai thuc te: " << width << "x" << height << endl;

    Mat imgOriginal, imgContour, imgDebug;
    vector<Point> docPoints;

    // Set FPS
    double lastTime = (double)getTickCount();
    double fps = 0;
    int frameCounter = 0;

    while (true) {
        bool success = cap.read(imgOriginal);
        if (!success) break;

        imgContour = imgOriginal.clone();

        
        // Hàm này thực hiện Canny, Dilate trên GPU và trả về points + ảnh debug
        imgDebug = scanner.detectDocument(imgOriginal, docPoints);

        // --- BƯỚC 2: VẼ KẾT QUẢ ---
        if (!docPoints.empty()) {
            vector<vector<Point>> conPoly{ docPoints };
            drawContours(imgContour, conPoly, 0, Scalar(0, 255, 0), 4);

            for (auto& pt : docPoints)
                circle(imgContour, pt, 8, Scalar(0, 0, 255), FILLED);

            putText(imgContour, "DOC FOUND", docPoints[0], FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
        }

        // --- BƯỚC 3: Calculate FPS ---
        frameCounter++;
        double currentTime = (double)getTickCount();
        if (frameCounter >= 10) {
            fps = frameCounter / ((currentTime - lastTime) / getTickFrequency());
            frameCounter = 0;
            lastTime = currentTime;
        }

        // Hiển thị FPS
        rectangle(imgContour, Point(10, 10), Point(200, 60), Scalar(0, 0, 0), FILLED);
        putText(imgContour, "CUDA FPS: " + to_string((int)fps), Point(20, 45), FONT_HERSHEY_DUPLEX, 1, Scalar(0, 255, 255), 2);

        // --- DISPLAY ---
        imshow("Document Scanner (GPU Powered)", imgContour);
        if (!imgDebug.empty()) imshow("GPU Threshold Debug", imgDebug);

        // --- INSTRUCTORS ---
        char key = (char)waitKey(1);
        if (key == 27) break; // ESC to exit

        // Nhấn 's' để scan và lưu ảnh
        if (key == 's' && !docPoints.empty()) {
            cout << "Dang xu ly anh chat luong cao..." << endl;
            // Gọi hàm Warp trên GPU
            Mat scannedDoc = scanner.getWarpedImage(imgOriginal, docPoints);

            // Hiển thị kết quả cắt
            imshow("Scanned Result", scannedDoc);

            // Lưu
            scanner.saveHighQualityDoc(scannedDoc, "Scanned_Document_" + to_string((int)currentTime));
            cout << "Da luu thanh cong!" << endl;
        }
    }
    cout << "\n=== DOCUMENT SCANNER (CUDA ACTIVATED) ===" << endl;
    cout << "Huong dan:" << endl;
    cout << " - Camera dang tu dong lay net..." << endl;
    cout << " - Nhan 'S' de luu anh." << endl;
    cout << " - Nhan 'ESC' de thoat." << endl;
    return 0;
}