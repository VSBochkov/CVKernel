import json                 # Подключаем модуль работы с форматом JSON
import multiprocessing      # Подключаем модуль работы с процессами
import time                 # Подключаем модуль работы со временем

import cv2 as cv            # Покдлючаем библиотеку OpenCV

from cv_kernel import cv_connector              # Подключаем класс коннектора
from cv_kernel.cv_network_controller import *   # Подключаем сетевого менеджера


class cv_supervisor(cv_connector):      # Определение класса супервайзера - наследника класса коннектора
    client_status = 0                   # Определение
    supervisor_status = 1               # константных значений
    supervisor_startup = 2              # входящих
    supervision_info = 3                # пакетов
    stop_display = 0x7f                 # Определение константы закрытия процесса отображения оверлея

    def __init__(self, network_controller, cvkernel_json_path, cvsupervisor_json_path,  # Определение
                 run_state_handler, ready_state_handler, closed_state_handler):         # конструктора класса
        self.cv_supervision_params = json.load(open(cvsupervisor_json_path, 'r'))       # Загрузка параметров супервидения
        self.display_processes = {}     # Создание словаря процессов отображения оверлея
        self.supervision_tcp = cv_network_controller.create_tcp_sock(int(self.cv_supervision_params['port']))   # Создание TCP сервера принятия супервизионных данных от ядра
        self.supervision_mutex = multiprocessing.RLock()    # Определение мьютекса процесса супервидения
        self.supervision_cv = multiprocessing.Condition(self.supervision_mutex)     # Определение условной блокировки процесса супервидения
        self.supervision = multiprocessing.Process(target=self.__supervision_process)   # Определение процесса супервидения
        self.supervision.start()    # Запуск процесса
        cv_connector.__init__(                                                      # Вызов
            self, network_controller, cvkernel_json_path, run_state_handler,        # конструктора
            ready_state_handler, closed_state_handler, {'type': 'supervisor'},      # базового
            self.cv_supervision_params                                              # класса
        )

    def __supervision_process(self):    # Определение метода процесса супервайзера
        kernel_supervision_tcp_sock = cv_network_controller.get_tcp_client(self.supervision_tcp)    # Получение соединения от ядра

        while True:     # Организация бесконечного цикла
            packet = cv_network_controller.receive_packet(kernel_supervision_tcp_sock)  # Прием пакета от ядра
            if len(packet.keys()) == 0:     # Если это пустой JSON пакет
                break       # Закрытие процесса супервайзера

            if packet['type'] == cv_supervisor.client_status:   # Если его тип - изменение статуса клиента
                self.__client_status_changed(packet)            # то вызываем метод обработчик статуса клиента
            elif packet['type'] == cv_supervisor.supervisor_startup:    # Если его тип - синхронизация
                self.__on_startup(packet)       # Запускаем метод обработчик синхронизации
            elif packet['type'] == cv_supervisor.supervision_info:      # Если тип - информация работы ядра
                cv_supervisor.__display_supervision_info(packet)        # Отображаем информацию
        kernel_supervision_tcp_sock.close()     # Закрываем соединение супервайзера
        print 'exit from __supervision_process' # Выходим из процесса супервайзера

    def __display_overlay(self, client_id):     # Определение процесса отрисовки оверлея
        overlay_path = self.display_processes[client_id]['overlay']     # Извлекаем сетевой путь нахождения оверлея

        print 'Open overlay stream at: {}'.format(overlay_path)         # Вывод сообщения о отображении по пути на консоль
        cap = cv.VideoCapture(overlay_path)     # Определение входящего видеопотока по пути
        while not cap.isOpened():               # Пока видеопоток не откроется
            print 'capture is not opened'       # Отображаем информацию о безуспешном взятии потока
            cap = cv.VideoCapture(overlay_path) # Пытаемся его заново открыть

        print 'overlay stream is open'          # Выводим в консоль сообщение успешного открытия видеопотока
        fps = cap.get(cv.CAP_PROP_FPS)          # Извлекаем характеристику FPS
        while fps > 100:                        # Пока она больше 100
            fps /= 10.                          # Делим на 10
        size = (int(cap.get(cv.CAP_PROP_FRAME_WIDTH)), int(cap.get(cv.CAP_PROP_FRAME_HEIGHT)))  # Извлекаем размер кадра
        writer = cv.VideoWriter('out' + time.strftime("%dd%mm%Yy_%H:%M:%S_") + str(client_id) + '.avi', # Создаем
                                1482049860, fps, size)  # видеопоток сохранения оверлея на диск
        while True:     # Организуем бесконечный цикл
            ret, frame = cap.read()    # Берем очередной кадр из входящего потока
            if not ret:                # Если кадр не взят
                break   # Выход из цикла
            writer.write(frame)        # Записываем кадр исходящий видеопоток
            cv.imshow(overlay_path, frame)  # Отображаем кадр на экран
            cv.waitKey(int(1000 / fps))     # Задержка на время жизни кадра

        cap.release()           # Закрываем входящий видеопоток
        writer.release()        # Закрываем исходящий видеопоток
        cv.destroyWindow(overlay_path)  # Закрываем окно отображения оверлея
        print 'exit from __display_overlay of #{}'.format(client_id)    # Выводим на экран сообщение о выходе из процесса отображения оверлея

    def __client_status_changed(self, packet):      # Определение метода обработчика клиентского статуса
        if packet['state'] == cv_connector.state_closed:        # Если состояние - "Не активен"
            print 'client #{} is closed'.format(packet['id'])   # Вывод сообщения на консоль
        elif packet['state'] == cv_connector.state_ready:       # Если состояние - "Остановлен"
            print 'client #{} is ready'.format(packet['id'])    # Вывод сообщения на консоль
            if packet['overlay_path'] == '':                    # Если путь до оверлея не задан
                return                                          # То выходим из функции
            print 'overlay_path is {}'.format(packet['overlay_path'])   # Выводим на экран сообщении о нахождении оверлея
            if packet['overlay_path'][0:3] != 'udp' and packet['overlay_path'][0:3] != 'rtp' and packet['overlay_path'][0:4] != 'rtsp':     # Если путь не сетевой
                return  # То выходим из функции
            if packet['id'] in self.display_processes.keys():   # Если номер коннектора уже находится в словаре запущенных процессов отображения оверлея
                self.display_processes[packet['id']]['process'].terminate()     # Завершаем текущий процесс отображения оверлея
                self.display_processes.pop(packet['id'])    # Извлекаем из словаря пару с номером коннектора
                print 'overlay displayer is closed'         # Вывод сообщения о закрытии процесса отображения на экран
            else:                                           # Если не найдено коннектора в словаре
                self.display_processes[packet['id']] = {'overlay': packet['overlay_path']}  # То создаем запись в словаре
                process = multiprocessing.Process(target=self.__display_overlay, args=(packet['id'],))  # и инициализируем
                self.display_processes[packet['id']]['process'] = process       # процесс отображения оверлея
                print 'overlay displayer is created'        # Выводим сообщение о созданном процессе на консоль
        elif packet['state'] == cv_connector.state_run:     # Если состояние - "Активен"
            print 'client #{} is run'.format(packet['id'])  # Вывод сообщения на консоль
            if packet['overlay_path'] == '':                # Если путь до оверлея не задан
                return                                      # То выходим из функции
            print 'overlay_path is {}'.format(packet['overlay_path'])   # Вывод сообщения о пути до оверлея на консоль
            if packet['overlay_path'][0:3] != 'udp' and packet['overlay_path'][0:3] != 'rtp' and packet['overlay_path'][0:4] != 'rtsp':     # Если путь не сетевой
                return                                      # То выходим из функции
            if packet['id'] in self.display_processes.keys():   # Если номер коннектора уже находится в словаре запущенных процессов отображения оверлея
                self.display_processes[packet['id']]['process'].start()     # То запускаем процесс
            else:                                               # Иначе
                self.display_processes[packet['id']] = {'overlay': packet['overlay_path']} # То создаем запись в словаре
                process = multiprocessing.Process(target=self.__display_overlay, args=(packet['id'],))  # и инициализируем
                self.display_processes[packet['id']]['process'] = process   # процесс отображения оверлея
                process.start()     # запускаем процесс отображения оверлея
            print 'overlay displayer is run'    # Выводим сообщение о запуске процесса отображения оверлея на консоль

    def __on_startup(self, packet):                 # Определение метода обработчика синхронизации супервайзера
        for client in packet['clients']:            # Для каждого клиента из пакета
            self.__client_status_changed(client)    # Вызываем метод обработки состояния клиента

    @staticmethod                                   # Статический метод
    def __display_supervision_info(packet):         # Определение метода отображения информации о работе ядра
        print '{}'.format(json.dumps(packet, indent=4))     # Печатаем пакет на консоль в удобочитаемом формате


class Test:
    meta_type = 0xff

    def __init__(self):
        from cv_kernel import cv_network_controller, cv_supervisor

        self.test_queue = multiprocessing.Queue()
        self.test_proc = multiprocessing.Process(target=self.__test)
        self.prev_state = multiprocessing.Value('i', cv_connector.state_closed)

        cv_kernel_path = os.environ.get('CVKERNEL_PATH')
        self.network_controller = cv_network_controller(None)
        self.supervisor = cv_supervisor(
            network_controller=self.network_controller,
            cvkernel_json_path=cv_kernel_path + '/cv_kernel_settings.json',
            cvsupervisor_json_path=cv_kernel_path + '/supervision.json',
            run_state_handler=self.test_on_run,
            ready_state_handler=self.test_on_ready,
            closed_state_handler=self.test_on_closed
        )

        self.test_proc.run()

    def test_on_run(self):
        self.test_queue.put({'type': cv_connector.state_run})

    def test_on_ready(self):
        self.test_queue.put({'type': cv_connector.state_ready})

    def test_on_closed(self):
        self.test_queue.put({'type': cv_connector.state_closed})
        if self.prev_state == cv_connector.state_ready:
            self.test_proc.join()

    def meta_handler(self, packet):
        self.test_queue.put({'type': Test.meta_type, 'meta': packet})

    def __test(self):
        running = True
        while running:
            packet = self.test_queue.get()
            if packet['type'] == cv_connector.state_closed:
                if self.prev_state == cv_connector.state_ready:
                    print 'closed by originator'
                    running = False
            elif packet['type'] == cv_connector.state_ready:
                if self.prev_state == cv_connector.state_closed:
                    print 'supervision is ready'
                    self.supervisor.run()
            elif packet['type'] == cv_connector.state_run:
                print 'supervision is run'

            self.prev_state = packet['type']
        self.network_controller.stop()
        print 'exit from __test.'


def test():
    print '====cv_supervisor_test===='
    Test()

if __name__ == '__main__':
    test()