#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include "DocumentScanner.hpp"

using namespace cv;
using namespace std;

int main() {
    cout << "Khoi tao Scanner (CPU Mode + PDF Export)..." << endl;
    DocumentScanner scanner;
    cout << "He thong san sang! Nhan 's' de luu anh va PDF." << endl;

    VideoCapture cap(1, CAP_DSHOW);
    if (!cap.isOpened()) {
        cap.open(0, CAP_DSHOW);
        if (!cap.isOpened()) {
            cout << "Khong mo duoc Camera!" << endl;
            return -1;
        }
    }

    cap.set(CAP_PROP_FRAME_WIDTH, 1920);
    cap.set(CAP_PROP_FRAME_HEIGHT, 1080);

    Mat imgOriginal, imgContour, imgDebug;
    vector<Point> docPoints;

    double lastTime = (double)getTickCount();
    double fps = 0;
    int frameCounter = 0;

    while (true) {
        bool success = cap.read(imgOriginal);
        if (!success) break;

        imgContour = imgOriginal.clone();

        // Xử lý tìm giấy
        imgDebug = scanner.detectDocument(imgOriginal, docPoints);

        // Vẽ kết quả
        if (!docPoints.empty()) {
            vector<vector<Point>> conPoly{ docPoints };
            drawContours(imgContour, conPoly, 0, Scalar(0, 255, 0), 4);
            for (auto& pt : docPoints)
                circle(imgContour, pt, 8, Scalar(0, 0, 255), FILLED);
            putText(imgContour, "DOC FOUND", docPoints[0], FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
        }

        // Tính FPS
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
        // if (!imgDebug.empty()) imshow("Debug Threshold", imgDebug); // Bật nếu cần debug

        char key = (char)waitKey(1);
        if (key == 27) break; // ESC

        // --- NHẤN 's' ĐỂ LƯU ---
        if (key == 's' && !docPoints.empty()) {
            cout << ">>> Dang xu ly..." << endl;

            // 1. Cắt và xử lý ảnh
            Mat scannedDoc = scanner.getWarpedImage(imgOriginal, docPoints);

            // Hiện kết quả tạm thời
            imshow("Scanned Result", scannedDoc);

            // Tạo tên file mặc định theo thời gian
            string timeStr = to_string((long long)time(0));
            string defaultName = "Scan_" + timeStr;

            // 2. Mở hộp thoại chọn vị trí lưu ảnh PNG
            string pngPath = DocumentScanner::getSaveFilePath(
                defaultName + ".png",
                "PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0",
                "png"
            );

            if (!pngPath.empty()) {
                // Loại bỏ phần mở rộng nếu có để tránh trùng lặp
                string basePath = pngPath;
                size_t lastDot = basePath.find_last_of('.');
                if (lastDot != string::npos) {
                    basePath = basePath.substr(0, lastDot);
                }

                if (scanner.saveHighQualityDoc(scannedDoc, basePath)) {
                    cout << "1. Da luu anh: " << basePath << ".png" << endl;
                } else {
                    cout << "Loi: Khong the luu anh PNG!" << endl;
                }

                // 3. Hỏi có muốn lưu PDF không
                cout << "Ban co muon luu thanh PDF khong? (Nhan 'p' de luu PDF)" << endl;
                char pdfChoice = (char)waitKey(0);
                
                if (pdfChoice == 'p' || pdfChoice == 'P') {
                    string pdfPath = DocumentScanner::getSaveFilePath(
                        defaultName + ".pdf",
                        "PDF Files (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0",
                        "pdf"
                    );

                    if (!pdfPath.empty()) {
                        if (scanner.saveDocToPDF(scannedDoc, pdfPath)) {
                            cout << "2. Da luu PDF: " << pdfPath << endl;
                        } else {
                            cout << "Loi: Khong the luu file PDF!" << endl;
                        }
                    } else {
                        cout << "Da huy luu PDF." << endl;
                    }
                }
            } else {
                cout << "Da huy luu file." << endl;
            }

            cout << ">>> Hoan tat!" << endl;
        }
    }

    return 0;
}