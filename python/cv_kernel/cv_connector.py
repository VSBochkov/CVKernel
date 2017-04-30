import json
import multiprocessing
import Queue

from cv_kernel import cv_network_controller


class cv_connector(object):
    state_closed = 0
    state_ready = 1
    state_run = 2
    com_run = 0
    com_stop = 1
    com_close = 2

    def __init__(self, network_controller, cvkernel_json_path, run_state_handler,
                 ready_state_handler, closed_state_handler, connector_type, connector_settings):
        self.cv_kernel_settings = json.load(open(cvkernel_json_path, 'r'))
        self.state_changed_handler = {
            cv_connector.state_closed: closed_state_handler,
            cv_connector.state_ready: ready_state_handler,
            cv_connector.state_run: run_state_handler
        }
        self.state_tcp = None
        self.connector_type = connector_type
        self.connector_settings = connector_settings
        self.network_controller = network_controller
        self.network_controller.set_cv_iot_mac_found_handler(self.connect_to_cvkernel)
        self.state_mutex = multiprocessing.RLock()
        self.state_cv = multiprocessing.Condition(self.state_mutex)
        self.rest_run = multiprocessing.Value('b', 0)
        self.tcp_queue = multiprocessing.Queue()
        self.rest = multiprocessing.Process(target=self.__state_machine)
        self.rest.start()
        self.network_controller.add_mac_handler(self.cv_kernel_settings['mac_address'],
                                                cv_network_controller.cv_iot_type)

    def run(self):
        rest = {'command': cv_connector.com_run}
        self.tcp_queue.put(rest)

    def stop(self):
        rest = {'command': cv_connector.com_stop}
        self.tcp_queue.put(rest)

    def close(self):
        rest = {'command': cv_connector.com_close}
        self.tcp_queue.put(rest)

    def connect_to_cvkernel(self, mac, tcp_address):
        self.tcp_queue.put(tcp_address)
        self.tcp_queue.put(self.connector_type)
        self.tcp_queue.put(self.connector_settings)
        self.start_jobs()

    def start_jobs(self):
        with self.state_mutex:
            self.state_cv.notify_all()

    def __state_machine(self):
        with self.state_mutex:
            self.state_cv.wait()

        kernel_address = self.tcp_queue.get()
        self.state_tcp = cv_network_controller.connect_to_tcp_host(
            kernel_address, self.cv_kernel_settings['tcp_server_port']
        )

        connection_state = cv_connector.state_closed
        prev_connection_state = cv_connector.state_closed

        def closed_by_originator():
            return connection_state == cv_connector.state_closed and \
                   prev_connection_state == cv_connector.state_ready

        while connection_state != cv_connector.state_run:
            packet = self.tcp_queue.get()
            cv_network_controller.send_packet(self.state_tcp, packet)
            response = cv_network_controller.receive_packet(self.state_tcp)
            try:
                connection_state = response['state']
            except KeyError:
                print 'Bad response from kernel: {}'.format(response)
            else:
                self.state_changed_handler[connection_state]()
                prev_connection_state = connection_state

        rest_run = True
        while rest_run:
            try:
                packet = self.tcp_queue.get(timeout=5)
            except Queue.Empty:
                pass
            else:
                print 'got packet {}'.format(packet)
                cv_network_controller.send_packet(self.state_tcp, packet)
            finally:
                response = cv_network_controller.async_receive_packet(self.state_tcp)
                if response is not None:
                    try:
                        connection_state = response['state']
                        self.state_changed_handler[connection_state]()
                    except KeyError:
                        print 'Bad response from kernel: {}'.format(response)
                    else:
                        if closed_by_originator():
                            self.state_tcp.close()
                            rest_run = 0
                        prev_connection_state = connection_state

        print 'exit from __state_machine'
