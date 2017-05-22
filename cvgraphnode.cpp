#include "cvgraphnode.h"            // Подключение файла cvgraphnode.h для определения методов классов
//#include "cvjsoncontroller.h"       // Подключение файла cvjsoncontroller.h для использования класса CVJsonController
//#include "cvfactorycontroller.h"    // Подключение файла cvfactorycontroller.h для использования класса CVFactoryController
#include "cvapplication.h"          // Подключение файла cvapplication.h для использования метода get_ip_address
#include <QTimer>                   // Подключение файла QTimer для использования одноименного класса
#include <QDateTime>                // Подключение файла QDateTime для использования одноименного класса
//#include <opencv2/opencv.hpp>       // Подключение файла opencv.hpp для использования классов Mat, VideoCapture и VideoWriter
//#include <utility>                  // Подключение файла utility для использования одноименного класса
#include <unistd.h>                 // Подключение файла unistd.h для использования метода usleep
//#include <time.h>                   // Подключение файла time.h для использования

QMap<QString, CVKernel::VideoData> CVKernel::video_data; // Определение словаря video_data

// Определение конструктора класса CVProcessData
// Входящие параметры:
//   video_name - идентификатор входного видеопотока
//   frame - кадр из видео подлежащий анализу
//   fnum - номер кадра
//   fps - количество кадров в секунду
//   draw_overlay - булевый флаг обозначающий необходимо ли отображать результат анализа на видео
//   pars - словарь входящих параметров процессорных узлов видеоаналитики
//   hist - словарь содержащий историю анализа видео
CVKernel::CVProcessData::CVProcessData(QString video_name, cv::Mat frame, int fnum, double fps, bool draw_overlay,
                                       QMap<QString, QSharedPointer<CVNodeParams>>& pars,
                                       QMap<QString, QSharedPointer<CVNodeHistory>>& hist)
    : video_name(video_name), frame_num(fnum), fps(fps) // Список инициализации конструктора
{
    params = pars;  // Присваивание словаря параметров переменной params
    history = hist; // Присваивание словаря истории переменной history
    video_data[video_name].frame = frame; // Инициализация кадра в словаре video_data по ключу video_name
    if (draw_overlay) {     // Если необходимо отображать данные анализа
        frame.copyTo(video_data[video_name].overlay); // То копируем исходный кадр в матрицу overlay словаря video_data
    }
}

// Определение конструктора класса CVIONode
// Входящие параметры:
//   device_id - номер USB устройства видеокамеры
//   output_path - путь до видео с отображением результатов анализа (может принимать значения "file:путь_до_видео", "udp:путь_до_видео_без_ip_адреса", "rtsp:путь_до_видео_без_ip_адреса", "rtp:путь_до_видео_без_ip_адреса"
//   width - ширина кадра
//   height - высота кадра
//   framespersecond - количество кадров в секунду
//   pars - словарь входящих параметров процессорных узлов видеоаналитики
CVKernel::CVIONode::CVIONode(int device_id, QString input_path, QString output_path,
                             int width, int height, double framespersecond,
                             QMap<QString, QSharedPointer<CVNodeParams>>& pars)
    : QObject(NULL),        // Создание родительского класса QObject не имеющего предка в объектной иерархии
      frame_number(1),                                              // Присвоение 1 номеру кадра
      frame_width(width),                                           // Инициализация ширины кадра
      frame_height(height),                                         // Инициализация высоты кадра
      params(pars)                                                  // Инициализация параметров процессорных узлов
{
    if (input_path.isEmpty()) { // Если путь до видео инициализирован (используется видеофайл или ip-камера)
        video_path = QString("video") + QString::number(device_id);    // Инициализация идентификатора потока
        in_stream.open(device_id);  // Открытие входящего видеопотока
    } else {    // Иначе если используется USB камера
        video_path = input_path;    // Инициализация идентификатора потока
        in_stream.open(input_path.toStdString()); // Открытие входящего видеопотока
    }

    if (not in_stream.isOpened())       // Если видеопоток не удалось открыть
    {
        qDebug() << "Video stream is not opened";   // То вывод в интерфейс командной строки сообщения о неудаче открытия потока
        return;     // Выход из функции
    }

    if (in_stream.get(CV_CAP_PROP_FPS) > 1.0)   // Если удалось прочитать характеристику количества кадров в секунду из видеопотока
        stopwatch.fps = in_stream.get(CV_CAP_PROP_FPS); // То инициализируем переменную fps структуры таймера значением из видеопотока
    else    // Иначе
        stopwatch.fps = framespersecond;    // Присваиваем количество кадров переданное в конструктор

    qDebug() << "Original: resolution: [" << (int) in_stream.get(CV_CAP_PROP_FRAME_WIDTH) << ", "    // Отображаем первоначальные характеристики входящего видеопотока
             << (int) in_stream.get(CV_CAP_PROP_FRAME_HEIGHT) << "] fps = " << in_stream.get(CV_CAP_PROP_FPS); // в интерфейс командной строки

    in_stream.set(CV_CAP_PROP_FRAME_WIDTH, width);   // Присваиваем
    in_stream.set(CV_CAP_PROP_FRAME_HEIGHT, height); // новые характеристики
    in_stream.set(CV_CAP_PROP_FPS, stopwatch.fps);   // входящего видеопотока

    frame_width = (int) in_stream.get(CV_CAP_PROP_FRAME_WIDTH);   // Получаем конечные характеристики
    frame_height = (int) in_stream.get(CV_CAP_PROP_FRAME_HEIGHT); // высоты и ширины кадра

    qDebug() << "Calculate: resolution: [" << frame_width << ", " << frame_height << "] fps = " << in_stream.get(CV_CAP_PROP_FPS); // Отображаем конечные характеристики видеопотока в интерфейс командной строки

    overlay_path = generate_overlay_path(output_path).isEmpty() ? "" : output_path;   // Присваиваем output_path переменной overlay_path, если путь корректный или не пустой
    state = std::make_unique<CVIONodeClosed>(*this);  // Присваиваем состояние "Не активен" для созданного узла ввода-вывода
}

// Определение метода деструктора класса CVIONode
CVKernel::CVIONode::~CVIONode()
{
    if (in_stream.isOpened()) { // Если входящий поток открыт
        in_stream.release();    // Закрыть входящий поток
        qDebug() << "in_stream.release();"; // Отобразить сообщение о закрытии потока в интерфейс командной строки
    }
    if (out_stream.isOpened()) { // Если исходящий поток открыт
        out_stream.release();    // Закрыть исходящий поток
        qDebug() << "out_stream.release();"; // Отобразить сообщение о закрытии потока в интерфейс командной строки
    }
}

// Определение метода generate_overlay_path класса CVIONode
// Входящий параметр output_path - шаблон пути до оверлея
// Выходное значение - путь до оверлея
QString CVKernel::CVIONode::generate_overlay_path(QString output_path)
{
    QString path;
    int colon_id = output_path.indexOf(":");  // Ищем позицию первого найденного символа ":" в пути
    if (not output_path.isEmpty() and colon_id > 0)  // Если путь не является пустой строкой и найден символ ':'
    {
        QString protocol = output_path.left(colon_id); // То определяем протокол по подстроке слева от символа ":"
        if (protocol == "file") // Если сохранение в файловую систему
        {
            path = path.mid(colon_id + 1);  // То итоговый путь объявлен справа от символа ":"
            int last_dot_id = output_path.lastIndexOf('.'); // Находим позицию последнего символа '.'
            QString format = "dd.MMM.yy_hh:mm:ss";          // Определяем формат строчного отображения даты и времени
            path.insert(last_dot_id, QDateTime::currentDateTime().toString(format)); // Вставляем перед типом выходного видео строку со временем создания файла
            qDebug() << "write to file:" << overlay_path;   // Отображаем на консоль информацию о пути до выходного файла
        }
        else // Если выкладываем видео в сеть
        {
            path = protocol + "://" + get_ip_address() + ":" + output_path.mid(colon_id + 1); // То определяем выходной поток вставляя после символа ':' ip-адрес компьютера в сети, на котором запущено ядро
        }
    }
    else // Иначе
    {
        path = ""; // инициализируем путь до выходного файла пустой строкой
    }
    return path; // возврат строки пути до оверлея
}

// Определение метода start класса CVIONode
void CVKernel::CVIONode::start()
{
    guard_lock.lock();   // Блокировка мьютекса
        state.reset(new CVIONodeRun(*this)); // Установка статуса "в процессе" для узла ввода-вывода
        state_changed_cv.wakeOne();          // Сигнал на пробуждение условной блокировки
    guard_lock.unlock(); // Освобождение мьютекса
}

// Определение метода stop класса CVIONode
void CVKernel::CVIONode::stop()
{
    guard_lock.lock();   // Блокировка мьютекса
        state.reset(new CVIONodeStopped(*this)); // Установка статуса "остановлен" для узла ввода-вывода
    guard_lock.unlock(); // Освобождение мьютекса
}

// Определение метода close класса CVIONode
void CVKernel::CVIONode::close()
{
    guard_lock.lock();   // Блокировка мьютекса
        state.reset(new CVIONodeClosed(*this)); // Установка статуса "не активен" для узла ввода-вывода
        state_changed_cv.wakeOne();             // Сигнал на пробуждение условной блокировки
    guard_lock.unlock(); // Освобождение мьютекса
}

// Определение метода process класса CVIONode
void CVKernel::CVIONode::process()
{
    guard_lock.lock();   // Блокировка мьютекса
        state_changed_cv.wait(&guard_lock); // Блокировка потока пока не будет сигнала на пробуждение
    guard_lock.unlock(); // Освобождение мьютекса

    history = CVKernel::CVFactoryController::get_instance().create_history(params.keys()); // Создание словаря истории видеоаналитики

    while(state.get() != nullptr)  // Пока память состояния узла ввода-вывода не освобождена
        state->process();          // Вызов метода process объекта состояния узла ввода-вывода (Паттерн ООП "Стратегия")
}

// Определение метода process класса CVIONodeRun - наследника класса CVIONodeState (Паттерн ООП "Стратегия")
void CVKernel::CVIONodeRun::process()
{
    io_node.on_run(); // Вызов метода on_run() у объекта-собственника состояния
}

// Определение метода process класса CVIONodeStopped - наследника класса CVIONodeState (Паттерн ООП "Стратегия")
void CVKernel::CVIONodeStopped::process()
{
    io_node.on_stopped();  // Вызов метода on_stopped() у объекта-собственника состояния
}

// Определение метода process класса CVIONodeClosed - наследника класса CVIONodeState (Паттерн ООП "Стратегия")
void CVKernel::CVIONodeClosed::process()
{
    io_node.on_closed();  // Вызов метода on_closed() у объекта-собственника состояния
}

// Определение метода on_run класса CVIONode
void CVKernel::CVIONode::on_run()
{
    cv::Mat frame;      // Создаем матрицу кадра
    stopwatch.start();  // Стартуем секундомер
    if (not in_stream.read(frame)) // Если не удалось прочитать кадр из входящего видеопотока
    {
        state.reset(new CVIONodeClosed(*this));  // Меняем состояние узла ввода-вывода на "Не активен"
        return;     // Выход из функции
    }

    if ((not overlay_path.isEmpty()) and (not out_stream.isOpened())) // Если путь до исходящего видео не пуст и видео не открыто
    {
        out_stream.open(generate_overlay_path(overlay_path).toStdString(), 1482049860, stopwatch.fps, frame.size(), true); // То открываем исходящий видеопоток
    }
    QSharedPointer<CVProcessData> process_data(    // Создаем указатель на объект данных видеопроцессинга на кадре
        new CVProcessData(                         // Выделяем память в куче для структуры CVProcessData
            video_path, frame, frame_number,       // Передаем
            get_fps(), out_stream.isOpened(),      // параметры
            params, history                        // конструктора
        )
    );
    emit next_node(process_data);                  // Генерируем сигнал next_node для передачи управления корневым процессорным ядрам леса видеоаналитики
    usleep(stopwatch.time());                      // Останавливаем поток на остаточное время жизни кадра выраженное в микросекундах
    out_stream << video_data[video_path].overlay;  // Записываем кадр с отображенными результатами анализа видео в исходящий поток
    emit send_metadata(CVJsonController::pack_to_json<CVProcessData>(*process_data.data())); // Генерируем сигнал отправки метаданных, вычисленных на кадре
    frame_number++;                                // Инкрементируем номер кадра
}

// Определение метода on_stopped класса CVIONode
void CVKernel::CVIONode::on_stopped()
{
    for (auto hist_item : history)  // Организуем цикл по всем парам словаря history
    {
        hist_item->clear_history(); // Очищаем историю видеоаналитики
    }

    if ((not overlay_path.isEmpty()) and (out_stream.isOpened()))  // Если выходной видеопоток открыт
    {
        out_stream.release(); // То закрываем его
    }

    guard_lock.lock();    // Захват мьютекса
        state_changed_cv.wait(&guard_lock); // Блокировка потока пока не будет подан сигнал на пробуждение
    guard_lock.unlock();  // Освобождение мьютекса
}

// Определение метода on_closed класса CVIONode
void CVKernel::CVIONode::on_closed()
{
    guard_lock.lock();    // Захват мьютекса
        emit close_udp(); // Генерация сигнала на закрытие UDP канала передачи метаданных
        state_changed_cv.wait(&guard_lock); // Блокировка потока пока не будет подан сигнал на пробуждение
    guard_lock.unlock();  // Освобождение мьютекса
    state.reset(nullptr); // Освобождение памяти под указатель статуса
    emit node_closed();   // Генерация сигнала о закрытии узла ввода-вывода
}

// Определение метода udp_closed класса CVIONode
void CVKernel::CVIONode::udp_closed() {
    guard_lock.lock();    // Захват мьютекса
        state_changed_cv.wakeOne();  // Генерация сигнала на пробуждение условной блокировки
    guard_lock.unlock();  // Освобождение мьютекса
}

// Определение конструктора класса CVProcessingNode
CVKernel::CVProcessingNode::CVProcessingNode() :
    QObject(NULL)    // Создание независимого объекта в иерархии Qt
{
    frame_counter = 0;   // Инициализация счетчика обработанных кадров нулем
    average_time = 0.;   // Инициализация среднего времени обработки кадра нулем
    for (int i = 0; i < window_size; i++) // Организация цикла по массиву timings
    {
        timings[i] = 0.; // Инициализация каждого члена массива нулем
    }
}

// Определение метода calc_average_time класса CVProcessingNode
// Возвращаемое значение - среднее время работы процессорного узла над кадром
double CVKernel::CVProcessingNode::calc_average_time()
{
    double acc_time = 0.;   // Инициализация аккумуляторного значения времени нулем
    int size = std::min(frame_counter, window_size); // Присваивание переменной size значения минимума между номером кадра и размером окна усреднения
    for(int i = 0; i < size; ++i) // Организация цикла для size-элементов массива timings
    {
        acc_time += timings[i];  // Суммируем все время
    }
    return acc_time / size; // Возвращаем значение суммы времени деленное на количество итераций цикла
}

// Определение метода process класса CVProcessingNode
// Входящий параметр process_data - разделяемый указатель на структуру данных видеопроцессинга
void CVKernel::CVProcessingNode::process(QSharedPointer<CVProcessData> process_data)
{
    clock_t start = clock();  // Стартуем таймер
    QSharedPointer<CVNodeData> node_data = compute(process_data); // Производим итерацию вычислений над кадром
    process_data->data[QString(this->metaObject()->className())] = node_data; // Складываем данные вычислений в словарь data структуры process_data
    timings[frame_counter++ % window_size] = (double)(clock() - start) / CLOCKS_PER_SEC; // Вычисляем время потраченное на вычисления
    average_time = calc_average_time(); // Вычисляем среднее время обработки последних кадров
    emit next_node(process_data);  // Генерируем сигнал передачи видеоаналитики следующему процессорному узлу в иерархии
}

// Определение метода reset_average_time класса CVProcessingNode
void CVKernel::CVProcessingNode::reset_average_time()
{
    average_time = 0.;  // Сброс значения среднего времени выполнения итерации
}
