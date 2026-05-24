#include <opencv2/opencv.hpp>   // Подключение заголовочного файла библиотеки OpenCV
#include <iostream>             // Подключение стандартной библиотеки для работы с потоками ввода-вывода
#include <vector>               // Подключение стандартного класса динамического массива
#include <string>               // Подключение стандартного класса для работы со строками
#include <algorithm>            // Подключение стандартных алгоритмов

using namespace cv;
using namespace std;

// Структура для хранения кандидата на выделение
struct Candidate {
    Rect rect;          // Прямоугольные координаты области на картинке
    double score;       // Оценка, показывающая насколько эта область похожа на настоящую рамку
};

// Функция измерения цвета вокруг рамки
// Возвращает средний цвет границы этого прямоугольника
// Измерение цвета вокруг рамки позволяет убедиться, что область окружена красным фоном
Scalar getBorderAverageColor(const Mat& img, Rect rect) {
    double r_sum = 0, g_sum = 0, b_sum = 0;
    int count = 0;

    // Расширяем рамку на 3 пикселя наружу, чтобы точно попасть на красный фон вокруг черного прямоугольника
    int x1 = std::max(0, rect.x - 3);
    int y1 = std::max(0, rect.y - 3);
    int x2 = std::min(img.cols - 1, rect.x + rect.width + 3);
    int y2 = std::min(img.rows - 1, rect.y + rect.height + 3);

    // Проходим по верхней и нижней горизонтальным линиям границы и считываем цвет пикселей
    for (int x = x1; x <= x2; x++) {
        Vec3b p1 = img.at<Vec3b>(y1, x);
        Vec3b p2 = img.at<Vec3b>(y2, x);
        b_sum += p1[0] + p2[0];
        g_sum += p1[1] + p2[1];
        r_sum += p1[2] + p2[2];
        count += 2;
    }

    // Проходим по левой и правой вертикальным линиям границы и считываем цвет пикселей
    for (int y = y1 + 1; y < y2; y++) {
        Vec3b p1 = img.at<Vec3b>(y, x1);
        Vec3b p2 = img.at<Vec3b>(y, x2);
        b_sum += p1[0] + p2[0];
        g_sum += p1[1] + p2[1];
        r_sum += p1[2] + p2[2];
        count += 2;
    }

    // Возвращает средний цвет
    if (count == 0) return Scalar(0,0,0);
    return Scalar(b_sum / count, g_sum / count, r_sum / count);
}

// Функция для сортировки кандидатов по убыванию их оценки
// Возвращает true, если оценка кандидата 1 выше, чем у кандидата 2
bool compareCandidates(const Candidate& a, const Candidate& b) {
    return a.score > b.score;
}

// Главный цикл
int main() {
    cout << "=== RUNNING NEW CODE ===" << endl;   // Выводит в консоль проверочную строку

    // Список изображений для обработки
    vector<string> filenames = {
        "test_marker.jpg",
        "test0.png",
        "test1.png",
        "test2.png",
        "test3.png",
        "test4.png"
    };

    // Начало цикла for, который будет поочередно обрабатывать файлы из списка
    for (const string& filename : filenames) {
        // 1. Загрузка очередного изображения
        Mat src = imread(filename, IMREAD_COLOR);   // Загрузка изображения в цветном формате BGR
        if (src.empty()) {
            cout << "Предупреждение: не удалось загрузить файл " << filename << endl;
            continue;
        }

        // 2. Разделение трехканального цветного изображения на независимые каналы
        vector<Mat> channels;
        split(src, channels);
        Mat red_channel = channels[2]; // На красном фоне черные прямоугольники имеют максимальный контраст именно в красном цвете

        // 3. Размытие красного канала для удаления шумов
        Mat blurred;
        GaussianBlur(red_channel, blurred, Size(11, 11), 0);

        // 4. Бинаризация по методу Оцу с инверсией цвета
        // Для алгоритма поиска контуров объекты обязательно должны быть белыми на черном фоне
        Mat bin;
        threshold(blurred, bin, 0, 255, THRESH_BINARY_INV | THRESH_OTSU);

        // 5. Поиск внешних контуров
        // Находит границы всех белых объектов на бинарном изображении
        vector<vector<Point>> contours;
        findContours(bin, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        // 6. Подготовка результирующего кадра
        // Создает точную копию оригинального цветного изображения, на котором будем рисовать рамки
        Mat result = src.clone();
        double total_img_area = src.cols * src.rows;
        vector<Candidate> candidates;

        // 7. Цикл анализа каждого найденного контура
        for (size_t i = 0; i < contours.size(); i++) {
            // Фильтрация по площади
            // Если контур слишком маленький или слишком большой, он отбрасывается как шум
            double area = contourArea(contours[i]);
            if (area < 500 || area > total_img_area * 0.08) {
                continue;
            }

            // Находим минимальный ориентированный (повернутый) прямоугольник вокруг контура
            RotatedRect rotRect = minAreaRect(contours[i]);
            double w = rotRect.size.width;
            double h = rotRect.size.height;

            // Определяем истинные (повернутые) ширину и высоту прямоугольника
            double real_width = std::max(w, h);
            double real_height = std::min(w, h);

            if (real_height == 0) continue;

            // Истинное отношение сторон (не зависит от наклона камеры)
            double true_aspect_ratio = real_width / real_height;

            // Жесткая геометрическая фильтрация пропорций:
            // Проверяет, укладывается ли истинное отношение сторон контура в допустимые геометрические пропорции ярлыков
            bool is_valid_shape = false;
            if (true_aspect_ratio >= 2.2 && true_aspect_ratio <= 5.0) {
                is_valid_shape = true; // Подходит под пропорции кода товара
            } else if (true_aspect_ratio >= 0.7 && true_aspect_ratio <= 1.8) {
                is_valid_shape = true; // Подходит под пропорции ряда/места
            }

            if (!is_valid_shape) {
                continue; // Отсекаем длинные полосы и любые нетипичные объекты
            }

            // Получаем стандартную выровненную рамку для отрисовки
            Rect rect = boundingRect(contours[i]);

            // Измерение цвета на границе рамки
            Scalar border_color = getBorderAverageColor(src, rect);
            double avg_g = border_color[1]; // Зеленый
            double avg_r = border_color[2]; // Красный

            // Красный цвет стикера очень насыщенный (R должен быть минимум в 1.35 раза больше G)
            if (avg_r < avg_g * 1.35) {
                continue;
            }

            // Коэффициент заполнения относительно прямоугольника
            // Для настоящих рамок он будет близок к 1.0 (ставим порог 0.65, что уберет ошибки)
            double rect_area = real_width * real_height;
            double solidity = area / rect_area;
            if (solidity < 0.65) {
                continue;
            }

            // Оценка кандидата и математический подсчет баллов
            double diff_code = abs(true_aspect_ratio - 4.0);
            double diff_item = abs(true_aspect_ratio - 1.1);
            double best_diff = std::min(diff_code, diff_item);
            double shape_score = 1.0 / (1.0 + best_diff);

            // Разница каналов на границе
            double color_diff = avg_r - avg_g;

            // Рассчитываем финальный балл кандидата
            // Перемножаем корень площади (даем приоритет крупным рамкам над мелким текстом), контрастность красной границы, заполненность и оценку формы
            double score = sqrt(area) * color_diff * solidity * shape_score;

            // Записывает успешного кандидата и его балл в структуру и добавляет в общий массив
            Candidate c;
            c.rect = rect;
            c.score = score;
            candidates.push_back(c);
        }

        // Сортируем всех найденных кандидатов по убыванию оценки
        sort(candidates.begin(), candidates.end(), compareCandidates);

        // Отрисовываем ровно 3 лучшие области (если найдено 3 или больше)
        int draw_count = std::min(3, (int)candidates.size());
        for (int k = 0; k < draw_count; k++) {
            rectangle(result, candidates[k].rect, Scalar(0, 255, 0), 3);
        }

        // Настройка и отображение окна результатов
        namedWindow(filename, WINDOW_NORMAL);
        resizeWindow(filename, 500, 700);
        imshow(filename, result);
    }

    waitKey(0);
    return 0;
}