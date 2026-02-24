#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include "DocumentScanner.hpp"

using namespace cv;
using namespace std;

int Document_Scanner_Cpu() {
    cout << "Initializing Scanner (Optimized for Canon 2000D)..." << endl;
    DocumentScanner scanner;
    cout << "System ready. Press 's' to scan and save." << endl;

    VideoCapture cap(0, CAP_DSHOW);
    if (!cap.isOpened()) {
        cap.open(1, CAP_DSHOW);
        if (!cap.isOpened()) {
            cout << "Error: Cannot open Camera!" << endl;
            return -1;
        }
    }

    // Set resolution to Full HD (1920x1080) for Canon 2000D
    cap.set(CAP_PROP_FRAME_WIDTH, 1920);
    cap.set(CAP_PROP_FRAME_HEIGHT, 1080);

    // Print actual resolution
    cout << "Camera Resolution: " << cap.get(CAP_PROP_FRAME_WIDTH)
        << "x" << cap.get(CAP_PROP_FRAME_HEIGHT) << endl;

    Mat imgOriginal, imgContour, imgDebug;
    vector<Point> docPoints;

    double lastTime = (double)getTickCount();
    double fps = 0;
    int frameCounter = 0;

    while (true) {
        bool success = cap.read(imgOriginal);
        if (!success) break;

        imgContour = imgOriginal.clone();

		// Phát hiện tài liệu trong khung hình
        imgDebug = scanner.detectDocument(imgOriginal, docPoints);

		// Phể hiện kết quả lên khung hình chính
        if (!docPoints.empty()) {
            vector<vector<Point>> conPoly{ docPoints };
            drawContours(imgContour, conPoly, 0, Scalar(0, 255, 0), 4);
            for (auto& pt : docPoints)
                circle(imgContour, pt, 8, Scalar(0, 0, 255), FILLED);
            putText(imgContour, "DOC FOUND", docPoints[0], FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
        }

		// 3. Tính toán FPS (do giới hạn phần cứng của máy ảnh canon 2000D 
        // chỉ có thể quay ở định dạng full HD với 30 FPS nên số FPS sẽ luôn dưới 30 dù máy tính có thể sử lý hơn thế)
        frameCounter++;
        double currentTime = (double)getTickCount();
        if (frameCounter >= 10) {
            fps = frameCounter / ((currentTime - lastTime) / getTickFrequency());
            frameCounter = 0;
            lastTime = currentTime;
        }

        rectangle(imgContour, Point(10, 10), Point(200, 60), Scalar(0, 0, 0), FILLED);
        putText(imgContour, "FPS: " + to_string((int)fps), Point(20, 45), FONT_HERSHEY_DUPLEX, 1, Scalar(0, 255, 255), 2);

        imshow("Scanner Preview", imgContour);

        char key = (char)waitKey(1);
        if (key == 27) break; // ESC to exit

        // 4. Lưu quá trình
        if (key == 's' && !docPoints.empty()) {
            cout << ">>> Processing image..." << endl;
            // The scanner class handles channel splitting internally.
            Mat scannedDoc = scanner.getWarpedImage(imgOriginal, docPoints);

            imshow("Scanned Result (Preview)", scannedDoc);
            cout << "Review the result window. Saving..." << endl;

            string timeStr = to_string((long long)time(0));
            string defaultName = "Scan_" + timeStr;

            // Lưu png
            string pngPath = DocumentScanner::getSaveFilePath(
                defaultName + ".png",
                "PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0",
                "png"
            );

            if (!pngPath.empty()) {
                string basePath = pngPath;
                size_t lastDot = basePath.find_last_of('.');
                if (lastDot != string::npos) {
                    basePath = basePath.substr(0, lastDot);
                }

                if (scanner.saveHighQualityDoc(scannedDoc, basePath)) {
                    cout << " [OK] Image Saved: " << basePath << ".png" << endl;
                }
                else {
                    cout << " [ERR] Failed to save PNG!" << endl;
                }

                // Lưu pdf
                cout << "Do you want to export as PDF? (Press 'p' to confirm)" << endl;
                char pdfChoice = (char)waitKey(0);

                if (pdfChoice == 'p' || pdfChoice == 'P') {
                    string pdfPath = DocumentScanner::getSaveFilePath(
                        defaultName + ".pdf",
                        "PDF Files (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0",
                        "pdf"
                    );

                    if (!pdfPath.empty()) {
                        if (scanner.saveDocToPDF(scannedDoc, pdfPath)) {
                            cout << " [OK] PDF Saved: " << pdfPath << endl;
                        }
                        else {
                            cout << " [ERR] Failed to save PDF!" << endl;
                        }
                    }
                    else {
                        cout << " PDF export cancelled." << endl;
                    }
                }
            }
            else {
                cout << " Save cancelled." << endl;
            }

            cout << ">>> Ready for next scan." << endl;
        }
    }

    return 0;
}

int main() {
    return Document_Scanner_Cpu();
}
