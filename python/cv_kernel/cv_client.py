import json                 # Подключаем модуль работы с JSON форматом
import multiprocessing      # Подключаем модуль работы с процессами

from cv_kernel import cv_connector              # Покдлючаем модуль коннектора платформы видеоаналитики
from cv_kernel.cv_network_controller import *   # Подключаем модуль сетевого менеджера


class cv_client(cv_connector):  # Определение класса клиента ядра видеоаналитики - наследника класса коннектора
    def __init__(self, network_controller, cvkernel_json_path, run_state_handler, ready_state_handler,  # Определение
                 closed_state_handler, meta_handler, cvproc_json_path):                                 # конструктора класса
        self.cv_process_description = json.load(open(cvproc_json_path, 'r'))    # Считывание JSON спецификации видеоаналитики
        self.meta_handler = meta_handler            # Инициализация метода обработчика принятия метаданных
        self.meta_mutex = multiprocessing.RLock()   # Инициализация мьютекса процесса принятия метаданных
        self.meta_cv = multiprocessing.Condition(self.meta_mutex)       # Определение условной блокировки процесса принятия метаданных
        self.meta = multiprocessing.Process(target=self.__meta_receiver)    # Определение процесса принятия метаданных
        self.meta_udp = cv_network_controller.create_udp_sock(int(self.cv_process_description['meta_udp_port']))    # Определение UDP порта принятия метаданных
        self.meta.start()   # Активация процесса принятия метаданных
        cv_connector.__init__(self, network_controller, cvkernel_json_path, run_state_handler,  # Вызов
                              ready_state_handler, closed_state_handler,                        # конструктора
                              {'type': 'client'}, self.cv_process_description)                  # базового класса

    def destruct(self):     # Определение деструктора класса
        self.meta.join()    # Ожидаем остановки процесса принятия метаданных

    def start_jobs(self):       # Определение метода активации процессов
        cv_connector.start_jobs(self)       # Вызов метода активации процессов базового класса
        with self.meta_mutex:               # Захватываем мьютекс
            self.meta_cv.notify_all()       # Освобождаем условную блокировку

    def __meta_receiver(self):  # Определение метода-процесса принятия метаданных
        with self.meta_mutex:   # Захватываем мьютекс
            self.meta_cv.wait() # Блокируем процесс пока не будет вызван метод активации процессов

        while True:     # В бесконечном цикле
            packet = cv_network_controller.receive_packet(self.meta_udp)    # Принимаем пакет метаданных по UDP от ядра видеоаналитики
            if len(packet.keys()) == 0:     # Если пакет - пустая спецификация JSON
                print 'exit from __meta_receiver'   # То выводим информацию о завершении потока в интерфейс командной строки
                return  # Выходим из процесса
            self.meta_handler(packet)   # Вызов метода обработки принятых метаданных


class Test:
    meta_type = 0xff

    def __init__(self):
        from cv_kernel import cv_network_controller, cv_client

        self.test_queue = multiprocessing.Queue()
        self.prev_state = multiprocessing.Value('i', cv_connector.state_closed)
        self.test_proc = multiprocessing.Process(target=self.__test)

        cv_kernel_path = os.environ.get('CVKERNEL_PATH')
        self.network_controller = cv_network_controller(None)
        self.client = cv_client(
            network_controller=self.network_controller,
            cvkernel_json_path=cv_kernel_path + '/cv_kernel_settings.json',
            run_state_handler=self.test_on_run,
            ready_state_handler=self.test_on_ready,
            closed_state_handler=self.test_on_closed,
            meta_handler=self.meta_handler,
            cvproc_json_path=cv_kernel_path + '/fire_overlay_on_pc.json'
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
                    print 'kernel initialized proccess tree and is ready for work'
                    self.client.run()
            elif packet['type'] == cv_connector.state_run:
                print 'video processing is run'
            elif packet['type'] == Test.meta_type:
                print 'meta: {}'.format(packet['meta'])

            self.prev_state = packet['type']
        self.network_controller.stop()
        print 'exit from __test.'


def test():
    print '====cv_client_test===='
    test_app = Test()

if __name__ == '__main__':
    test()
