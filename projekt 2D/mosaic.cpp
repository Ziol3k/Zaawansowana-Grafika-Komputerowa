#include <opencv2/opencv.hpp>
#include <iostream>
#include <random>

using namespace cv;
using namespace std;

// clamp pomocniczy
inline int clampi(int v, int lo, int hi) {
    return max(lo, min(v, hi));
}

// rysowanie półprzezroczystego koła (lokalnie!)
void drawCircleAlpha(
    Mat &canvas,
    Point center,
    int radius,
    const Vec3f &color,
    float alpha
) {
    int x0 = clampi(center.x - radius, 0, canvas.cols - 1);
    int x1 = clampi(center.x + radius, 0, canvas.cols - 1);
    int y0 = clampi(center.y - radius, 0, canvas.rows - 1);
    int y1 = clampi(center.y + radius, 0, canvas.rows - 1);

    int r2 = radius * radius;

    for (int y = y0; y <= y1; y++) {
        Vec3f *row = canvas.ptr<Vec3f>(y);
        for (int x = x0; x <= x1; x++) {
            int dx = x - center.x;
            int dy = y - center.y;
            if (dx*dx + dy*dy <= r2) {
                row[x] = color * alpha + row[x] * (1.0f - alpha);
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        cout << "Usage: mosaic input.jpg output.png\n";
        return 0;
    }

    // ===== PARAMETRY (łatwe do opisu w projekcie) =====
    const int ITERATIONS = 200000;   // ilość prób (zwiększ dla lepszej jakości)
    const int MIN_R = 2;            // minimalny promień koła
    const int MAX_R = 15;           // maksymalny promień
    const float BASE_ALPHA = 0.35f; // bazowa przezroczystość
    const float EDGE_BOOST = 0.65f; // wzmocnienie na krawędziach

    // ===== Wczytanie obrazu =====
    Mat img = imread(argv[1]);
    if (img.empty()) {
        cerr << "Nie mozna wczytac obrazu\n";
        return 1;
    }

    // ===== Sobel =====
    Mat gray, gx, gy, grad;
    cvtColor(img, gray, COLOR_BGR2GRAY);
    Sobel(gray, gx, CV_32F, 1, 0, 3);
    Sobel(gray, gy, CV_32F, 0, 1, 3);
    magnitude(gx, gy, grad);
    normalize(grad, grad, 0.0f, 1.0f, NORM_MINMAX);

    // ===== Płótno (float, bo alpha blending) =====
    Mat canvas(img.rows, img.cols, CV_32FC3, Scalar(1,1,1));

    // ===== RNG =====
    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> rx(0, img.cols - 1);
    std::uniform_int_distribution<int> ry(0, img.rows - 1);
    std::uniform_real_distribution<float> rf(0.0f, 1.0f);

    // ===== GŁÓWNA PĘTLA =====
    for (int i = 0; i < ITERATIONS; i++) {

        int x = rx(rng);
        int y = ry(rng);

        float g = grad.at<float>(y, x);

        // probabilistyczne odrzucenie (nieregularność!)
        if (rf(rng) > g * EDGE_BOOST + 0.15f)
            continue;

        // rozmiar koła sterowany Sobelem
        int radius = int(MAX_R * (1.0f - g) + MIN_R * g);

        // obszar próbkowania koloru
        int x0 = clampi(x - radius, 0, img.cols - 1);
        int y0 = clampi(y - radius, 0, img.rows - 1);
        int x1 = clampi(x + radius, 0, img.cols - 1);
        int y1 = clampi(y + radius, 0, img.rows - 1);

        Rect sample(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
        Scalar m = mean(img(sample));
        Vec3f color(m[0]/255.0f, m[1]/255.0f, m[2]/255.0f);

        // alpha zależna od gradientu
        float alpha = BASE_ALPHA + g * 0.4f;

        drawCircleAlpha(canvas, Point(x,y), radius, color, alpha);
    }

    // ===== Zapis =====
    Mat out;
    canvas.convertTo(out, CV_8UC3, 255.0);
    imwrite(argv[2], out);

    cout << "Zapisano: " << argv[2] << endl;
    return 0;
}
