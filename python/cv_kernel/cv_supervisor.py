import json
import multiprocessing
import time

import cv2 as cv

from cv_kernel import cv_connector
from cv_kernel.cv_network_controller import *


class cv_supervisor(cv_connector):
    client_status = 0
    supervisor_status = 1
    supervisor_startup = 2
    supervision_info = 3
    stop_display = 0x7f

    def __init__(self, network_controller, cvkernel_json_path, cvsupervisor_json_path,
                 run_state_handler, ready_state_handler, closed_state_handler):
        self.cv_supervision_params = json.load(open(cvsupervisor_json_path, 'r'))
        self.display_processes = {}
        self.supervision_tcp = cv_network_controller.create_tcp_sock(int(self.cv_supervision_params['port']))
        self.supervision_mutex = multiprocessing.RLock()
        self.supervision_cv = multiprocessing.Condition(self.supervision_mutex)
        self.supervision = multiprocessing.Process(target=self.__supervision_process)
        self.supervision.start()
        cv_connector.__init__(
            self, network_controller, cvkernel_json_path, run_state_handler,
            ready_state_handler, closed_state_handler, {'type': 'supervisor'},
            self.cv_supervision_params
        )

    def __supervision_process(self):
        kernel_supervision_tcp_sock = cv_network_controller.get_tcp_client(self.supervision_tcp)

        while True:
            packet = cv_network_controller.receive_packet(kernel_supervision_tcp_sock)
            if len(packet.keys()) == 0:
                break

            if packet['type'] == cv_supervisor.client_status:
                self.__client_status_changed(packet)
            elif packet['type'] == cv_supervisor.supervisor_startup:
                self.__on_startup(packet)
            elif packet['type'] == cv_supervisor.supervision_info:
                cv_supervisor.__display_supervision_info(packet)
        kernel_supervision_tcp_sock.close()
        print 'exit from __supervision_process'

    def __display_overlay(self, client_id):
        overlay_path = self.display_processes[client_id]['overlay']

        cap = cv.VideoCapture(overlay_path)
        while not cap.isOpened():
            cap = cv.VideoCapture(overlay_path)

        fps = cap.get(cv.CAP_PROP_FPS)
        while fps > 100:
            fps /= 10.
        size = (int(cap.get(cv.CAP_PROP_FRAME_WIDTH)), int(cap.get(cv.CAP_PROP_FRAME_HEIGHT)))
        writer = cv.VideoWriter('out' + time.strftime("%dd%mm%Yy_%H:%M:%S_") + str(client_id) + '.avi',
                                1482049860, fps, size)
        while True:
            ret, frame = cap.read()
            if not ret:
                break
            writer.write(frame)
            cv.imshow(overlay_path, frame)
            cv.waitKey(int(1000 / fps))

        cap.release()
        writer.release()
        cv.destroyWindow(overlay_path)
        print 'exit from __display_overlay of #{}'.format(client_id)

    def __client_status_changed(self, packet):
        if packet['state'] == cv_connector.state_closed:
            print 'client #{} is closed'.format(packet['id'])
        elif packet['state'] == cv_connector.state_ready:
            print 'client #{} is ready'.format(packet['id'])
            if packet['overlay_path'] == '':
                return
            if packet['id'] in self.display_processes.keys():
                self.display_processes[packet['id']]['process'].terminate()
                self.display_processes.pop(packet['id'])
                print 'overlay displayer closed'
            else:
                self.display_processes[packet['id']] = {'overlay': packet['overlay_path']}
                process = multiprocessing.Process(target=self.__display_overlay, args=(packet['id'],))
                self.display_processes[packet['id']]['process'] = process
                print 'created overlay displayer'
        elif packet['state'] == cv_connector.state_run:
            print 'client #{} is run'.format(packet['id'])
            if packet['overlay_path'] == '':
                return
            if packet['id'] in self.display_processes.keys():
                self.display_processes[packet['id']]['process'].start()
            else:
                self.display_processes[packet['id']] = {'overlay': packet['overlay_path']}
                process = multiprocessing.Process(target=self.__display_overlay, args=(packet['id'],))
                self.display_processes[packet['id']]['process'] = process
                process.start()
            print 'overlay displayer is run'

    def __on_startup(self, packet):
        for client in packet['clients']:
            self.__client_status_changed(client)

    @staticmethod
    def __display_supervision_info(packet):
        print '{}'.format(json.dumps(packet, indent=4))


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