import json             # Подключаем модуль работы с JSON форматом
import multiprocessing  # Подключаем модуль работы с процессами
import Queue            # Подключаем модуль работы с очередями

from cv_kernel.cv_network_controller import *   # Подключаем модуль сетевого менеджера


class cv_connector(object):     # Определение класса коннектора
    state_closed = 0            # Определение
    state_ready = 1             # констант значений
    state_run = 2               # состояний коннектора
    com_run = 0                 # Определение
    com_stop = 1                # констант значений
    com_close = 2               # команд коннектора

    def __init__(self, network_controller, cvkernel_json_path, run_state_handler,                   # Определение
                 ready_state_handler, closed_state_handler, connector_type, connector_settings):    # конструктора
        self.cv_kernel_settings = json.load(open(cvkernel_json_path, 'r'))  # Загрузка сетевых настроек ядра видеоаналитики
        self.state_changed_handler = {                          # Определение
            cv_connector.state_closed: closed_state_handler,    # словаря
            cv_connector.state_ready: ready_state_handler,      # обработчиков
            cv_connector.state_run: run_state_handler           # состояний
        }                                                       # коннектора
        self.state_tcp = None                                   # Определение TCP сокета передачи состояния коннектора
        self.connector_type = connector_type                    # Инициализация значения типа коннектора (клиент ядра или супервайзер)
        self.connector_settings = connector_settings            # Инициализация сетевых настроек коннектора
        self.network_controller = network_controller            # Инициализация сетевого менеджера
        self.state_mutex = multiprocessing.RLock()              # Создание мьютекса процесса передачи состояния
        self.state_cv = multiprocessing.Condition(self.state_mutex)     # Создание условной блокировки
        self.tcp_queue = multiprocessing.Queue()                # Создание очереди процесса передачи состояния
        self.rest = multiprocessing.Process(target=self.__state_machine)    # Создание процесса передачи состояния
        self.rest.start()       # Активация процесса
        self.network_controller.set_cv_iot_mac_found_handler(self.connect_to_cvkernel)  # Активация платформенного обработчика найденного в сети целевого ядра
        self.network_controller.add_cvkernel_mac_handler(self.cv_kernel_settings['mac_address'])    # Добавление МАС адреса ядра видеоаналитики для поиска в сети

    def run(self):      # Определение метода перевода состояния коннектора в "Активен"
        rest = {'command': cv_connector.com_run}   # Инициализация пакета
        self.tcp_queue.put(rest)    # Помещение пакета в очередь

    def stop(self):     # Определение метода перевода состояния коннектора в "Остановлен"
        rest = {'command': cv_connector.com_stop}   # Инициализация пакета
        self.tcp_queue.put(rest)    # Помещение пакета в очередь

    def close(self):    # Определение метода перевода состояния коннектора в "Не активен"
        rest = {'command': cv_connector.com_close}  # Инициализация пакета
        self.tcp_queue.put(rest)    # Помещение пакета в очередь

    def connect_to_cvkernel(self, mac, tcp_address):    # Определение метода-обработчика нахождения ядра видеоаналитики в сети
        self.tcp_queue.put(tcp_address)                 # Помещение пакета с адресом
        self.tcp_queue.put(self.connector_type)         # Помещение пакета с типом коннектора
        self.tcp_queue.put(self.connector_settings)     # Помещение пакета с настройками коннектора
        self.start_jobs()                               # Вызов метода запуска процессов

    def start_jobs(self):                               # Определение метода запуска процессов (Виртуальный)
        with self.state_mutex:                          # Захват мьютекса
            self.state_cv.notify_all()                  # Разблокировка процесса передачи состояния

    def __state_machine(self):                          # Определение метода-процесса передачи состояния
        with self.state_mutex:                          # Захват мьютекса
            self.state_cv.wait()                        # Ожидание запуска процесса

        kernel_address = self.tcp_queue.get()           # Получение адреса ядра
        self.state_tcp = cv_network_controller.connect_to_tcp_host(     # Присоединение к порту
            kernel_address, self.cv_kernel_settings['tcp_server_port']  # передачи состояния
        )                                                               # ядра через TCP
        print '__state_machine: cvkernel connected'     # Вывод успешного присоединения к ядру видеоаналитики в консоль

        connection_state = cv_connector.state_closed        # Начальная инициализация текущего
        prev_connection_state = cv_connector.state_closed   # и предыдущего состояний коннектора

        def closed_by_originator():     # Определение метода выяснения был ли коннектор закрыт ядром
            return connection_state == cv_connector.state_closed and prev_connection_state == cv_connector.state_ready    # Возвращение истинности Условия предыдущего состояния "Остановлен" и текущего "Не активен"

        while connection_state != cv_connector.state_run:   # Организуем цикл до перевода состояния коннектора в значение "Активен"
            packet = self.tcp_queue.get()   # Получаем пакет из очереди
            cv_network_controller.send_packet(self.state_tcp, packet)   # Отсылаем его по TCP
            response = cv_network_controller.receive_packet(self.state_tcp)     # Принимаем ответ от ядра
            if response == cv_network_controller.connection_is_broken:  # Если было разорвано соединение
                print '__state_machine: cvkernel disconnected, try to connect again'    # Выводим информацию в консоль
                self.state_tcp = cv_network_controller.connect_to_tcp_host(         # Присоединяемся заново
                    kernel_address, self.cv_kernel_settings['tcp_server_port']      # к ядру
                )
                print '__state_machine: cvkernel connected'     # Выводим сообщение о присоединении к ядру
            elif response == cv_network_controller.invalid_data:    # Если были приняты испорченные данные
                print '__state_machine: Bad response from kernel'   # Выводим сообщение об этом в консоль
            elif response == cv_network_controller.no_data:         # Если не приняты данные от ядра
                pass                                                # Не делаем ничего
            else:                                                   # Если принят ответ от ядра
                try:                                                # Блок перехвата исключений
                    connection_state = response['state']            # Извлекаем состояние из ответа
                except KeyError:                                    # Если в пакете не найден элемент состояния коннектора
                    print '__state_machine: Bad response from kernel: {}'.format(response)  # Выводим сообщение о неправильном ответе от ядра
                else:                                               # Если состояние получено
                    self.state_changed_handler[connection_state]()  # Вызываем обработчик состояния
                    prev_connection_state = connection_state        # Инициализируем предыдущее состояние

        rest_run = True     # Инициализируем флаг активности передачи данных
        while rest_run:     # Выполняем цикл пока не деактивирована передача данных
            try:            # Блок перехвата исключений
                packet = self.tcp_queue.get(timeout=5)  # Извлекаем из очереди пакет
            except Queue.Empty:    # Если пакет не был извлечен
                pass               # То не делаем ничего
            else:                  # Если извлекли пакет
                print '__state_machine: got packet {}'.format(packet)       # То распечатываем его в консоль
                cv_network_controller.send_packet(self.state_tcp, packet)   # Отправляем пакет ядру видеоаналитики
            finally:    # В любом случае
                response = cv_network_controller.async_receive_packet(self.state_tcp)   # Принимаем ответ от ядра
                if response == cv_network_controller.connection_is_broken:  # Если соединение было разорвано
                    print '__state_machine: cvkernel disconnected, try to connect again'    # Выводим сообщение об этом в консоль
                    self.state_tcp = cv_network_controller.connect_to_tcp_host(     # Подключаемся
                        kernel_address, self.cv_kernel_settings['tcp_server_port']  # заново к ядру
                    )
                    print '__state_machine: cvkernel connected'         # Выводим сообщение об подключении к ядру в консоль
                elif response == cv_network_controller.invalid_data:    # Если был принят битый пакет
                    print '__state_machine: Bad response from kernel'   # Выводим сообщение об этом в консоль
                elif response == cv_network_controller.no_data:         # Если не был принят пакет
                    pass                                                # То не делаем ничего
                else:                                                   # Если принят пакет
                    try:                                                # Блок перехвата исключений
                        connection_state = response['state']            # Извлекаем значение состояния из ответа
                    except KeyError:                                    # Если в пакете не найдено значение состояния
                        print '__state_machine: Bad response from kernel: {}'.format(response)  # Выводим сообщение об этом в консоль
                    else:                                               # Если извлечение прошло успешно
                        self.state_changed_handler[connection_state]()  # Вызываем обработчик нового состояния
                        if closed_by_originator():                      # Если ядро деактивировало коннектор
                            self.state_tcp.close()                      # Закрываем сокет
                            rest_run = 0                                # Выходим из цикла
                        prev_connection_state = connection_state        # Инициализируем предыдущее состояние коннектора

        print 'exit from __state_machine'   # Вывод сообщения о завершении процесса в консоль
