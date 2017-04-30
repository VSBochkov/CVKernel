import json
import multiprocessing

from cv_kernel import cv_connector
from cv_kernel import cv_network_controller


class cv_client(cv_connector):
    def __init__(self, network_controller, cvkernel_json_path, run_state_handler, ready_state_handler,
                 closed_state_handler, meta_handler, cvproc_json_path):
        self.cv_process_description = json.load(open(cvproc_json_path, 'r'))
        self.meta_handler = meta_handler
        self.meta_mutex = multiprocessing.RLock()
        self.meta_cv = multiprocessing.Condition(self.meta_mutex)
        self.meta = multiprocessing.Process(target=self.__meta_receiver)
        self.meta_udp = cv_network_controller.create_udp_sock(int(self.cv_process_description['meta_udp_port']))
        self.meta.start()
        cv_connector.__init__(self, network_controller, cvkernel_json_path, run_state_handler,
                              ready_state_handler, closed_state_handler,
                              {'type': 'client'}, self.cv_process_description)

    def destruct(self):
        self.meta.join()

    def start_jobs(self):
        cv_connector.start_jobs(self)
        with self.meta_mutex:
            self.meta_cv.notify_all()

    def __meta_receiver(self):
        with self.meta_mutex:
            self.meta_cv.wait()

        while True:
            packet = cv_network_controller.receive_packet(self.meta_udp)
            if len(packet.keys()) == 0:
                print 'exit from __meta_receiver'
                return
            self.meta_handler(packet)


class Test:
    meta_type = 0xff

    def __init__(self):
        from cv_kernel import cv_network_controller, cv_client

        self.test_queue = multiprocessing.Queue()
        self.prev_state = multiprocessing.Value('i', cv_connector.state_closed)
        self.test_proc = multiprocessing.Process(target=self.__test)

        self.network_controller = cv_network_controller(None)
        self.client = cv_client(
            network_controller=self.network_controller,
            cvkernel_json_path='/home/vbochkov/workspace/development/CVKernel/cv_kernel_settings.json',
            run_state_handler=self.test_on_run,
            ready_state_handler=self.test_on_ready,
            closed_state_handler=self.test_on_closed,
            meta_handler=self.meta_handler,
            cvproc_json_path='/home/vbochkov/workspace/development/FireRobotDriver/fire_overlay_on_pc.json'
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