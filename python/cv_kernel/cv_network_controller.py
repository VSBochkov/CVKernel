import socket
import json
import time
import multiprocessing
import Queue
import os
import xml.etree.cElementTree as ET
import subprocess


class cv_network_controller:
    sizeof_uint64 = 8
    localhost = '127.0.0.1'
    add_mac = 1
    stop_scanner = 2
    local_mac = 3
    cv_iot_type = 0x7f
    iot_type = 0x6f

    def __init__(self, iot_mac_found_handler):
        self.cv_iot_mac_found_handler = None
        self.iot_mac_found_handler = iot_mac_found_handler
        self.scanner_mtx = multiprocessing.RLock()
        self.scanner_queue = multiprocessing.Queue()
        self.scanner_cv = multiprocessing.Condition(self.scanner_mtx)
        #self.scanner_proc = multiprocessing.Process(target=self.__scanner)
        #self.scanner_proc.start()

    def set_cv_iot_mac_found_handler(self, cv_iot_mac_found_handler):
        self.cv_iot_mac_found_handler = cv_iot_mac_found_handler
        self.scanner_proc = multiprocessing.Process(target=self.__scanner)
        self.scanner_proc.start()

    def stop(self):
        self.scanner_queue.put({'type': cv_network_controller.stop_scanner})
        self.scanner_proc.join()

    def add_cvkernel_mac_handler(self, mac_address):
        with self.scanner_mtx:
            if cv_network_controller.is_local_mac_address(mac_address):
                self.scanner_queue.put({
                    'type': cv_network_controller.local_mac,
                    'handler_type': cv_network_controller.cv_iot_type
                })
            else:
                self.scanner_queue.put({
                    'type': cv_network_controller.add_mac,
                    'mac_address': mac_address,
                    'handler_type': cv_network_controller.cv_iot_type
                })

    def add_mac_handler(self, mac_address):
        with self.scanner_mtx:
            if cv_network_controller.is_local_mac_address(mac_address):
                self.scanner_queue.put({
                    'type': cv_network_controller.local_mac,
                    'handler_type': cv_network_controller.iot_type
                })
            else:
                self.scanner_queue.put({
                    'type': cv_network_controller.add_mac,
                    'mac_address': mac_address,
                    'handler_type': cv_network_controller.iot_type
                })

    @staticmethod
    def __update_mac_ip_map():
        mac_ip_map = {}
        xml_file = 'nmap.xml'
        hostname_pipe = os.popen('hostname -I')
        ip_address = hostname_pipe.read().split('.')
        if len(ip_address) < 4:
            return mac_ip_map
        ip_subnet = ip_address[0] + '.' + ip_address[1] + '.' + ip_address[2] + '.0/24'
        subprocess.call(['nmap', '-oX', xml_file, '-sn', ip_subnet])
        tree = ET.parse(xml_file)
        os.remove(xml_file)
        root = tree.getroot()
        hosts = [element for element in root if element.tag == 'host']
        for host in hosts:
            ip_address = ''
            addresses = [node.attrib for node in host if node.tag == 'address']
            for address in addresses:
                if address['addrtype'] == 'ipv4':
                    ip_address = address['addr']
                elif address['addrtype'] == 'mac':
                    mac_ip_map[address['addr']] = ip_address
        return mac_ip_map

    def __scanner(self):
        mac_found_handlers = {}
        mac_ip_map = {}

        while True:
            try:
                command = self.scanner_queue.get(timeout=5)
            except Queue.Empty:
                pass
            else:
                if command['type'] == cv_network_controller.stop_scanner:
                    print 'exit from __scanner'
                    return
                elif command['type'] == cv_network_controller.local_mac:
                    print '__scanner: localhost'
                    if command['handler_type'] == cv_network_controller.cv_iot_type:
                        self.cv_iot_mac_found_handler(None, cv_network_controller.localhost)
                    else:
                        self.iot_mac_found_handler(None, cv_network_controller.localhost)
                elif command['type'] == cv_network_controller.add_mac:
                    print '__scanner: added new mac_address - {}'.format(command['mac_address'])
                    mac_found_handlers[command['mac_address']] = command['handler_type']
            finally:
                if len(mac_found_handlers.keys()) > 0:
                    mac_ip_map = self.__update_mac_ip_map()
                for mac in mac_found_handlers.keys():
                    mac = mac.lower().upper()
                    if mac in mac_ip_map.keys():
                        print 'mac found: {}'.format(mac)
                        if mac_found_handlers[mac] == cv_network_controller.cv_iot_type:
                            self.cv_iot_mac_found_handler(mac, mac_ip_map[mac])
                        else:
                            self.iot_mac_found_handler(mac, mac_ip_map[mac])
                        mac_found_handlers.pop(mac)

    @staticmethod
    def async_receive_byte(sock):
        try:
            sock.setblocking(0)
            byte = sock.recv(1)
        except socket.error:
            return None
        else:
            return byte

    @staticmethod
    def is_local_mac_address(target_mac):
        target_mac = target_mac.upper().lower()
        result = False
        pipe = os.popen('ifconfig')
        out = pipe.readlines()
        for line in out:
            line = line[:-1]
            strings = line.split(' ')
            strings = [string for string in strings if string != '']
            if len(strings) > 4 and strings[3] == 'HWaddr' and strings[4] == target_mac:
                result = True
                break
        pipe.close()
        return result

    @staticmethod
    def create_udp_sock(udp_port):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((cv_network_controller.localhost, udp_port))
        return sock

    @staticmethod
    def create_tcp_sock(tcp_port):
        sock = socket.socket()
        sock_initialized = False
        while not sock_initialized:
            try:
                sock.bind(('', tcp_port))
            except socket.error:
                print 'Address already in use: try to bind again over 10 seconds'
                time.sleep(10)
            else:
                sock_initialized = True

        return sock

    @staticmethod
    def get_tcp_client(sock):
        sock.listen(1)
        conn, addr = sock.accept()
        return conn

    @staticmethod
    def connect_to_tcp_host(tcp_address, tcp_port):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        connected = False
        while not connected:
            try:
                sock.connect((tcp_address, tcp_port))
            except socket.error:
                time.sleep(5)
            else:
                connected = True
        return sock

    @staticmethod
    def async_receive_packet(sock):
        try:
            sock.setblocking(0)
            size_arr = sock.recv(cv_network_controller.sizeof_uint64)
        except socket.error:
            return None
        else:
            if len(size_arr) == 0:
                return None
            size = cv_network_controller.__bytearr_to_uint(size_arr)
            if size == 0:
                return None
            sock.setblocking(1)
            buf = cv_network_controller.__recv_all(sock, size)
            return json.loads(buf)

    @staticmethod
    def receive_packet(sock):
        size_arr = sock.recv(cv_network_controller.sizeof_uint64)
        size = cv_network_controller.__bytearr_to_uint(size_arr)
        if size == 0:
            return None
        buf = cv_network_controller.__recv_all(sock, size)
        return json.loads(buf)

    @staticmethod
    def send_packet(sock, rest_packet):
        packet = json.dumps(rest_packet, separators=(',', ':'))
        packet_size = cv_network_controller.__uint_to_bytearr(len(packet))
        sock.sendall(packet_size + packet)

    @staticmethod
    def __bytearr_to_uint(bytearr):
        uinteger = 0
        for i in range(0, cv_network_controller.sizeof_uint64):
            uinteger += ord(bytearr[i]) << (8 * i)
        return uinteger

    @staticmethod
    def __uint_to_bytearr(uinteger):
        bytearr = ''
        for i in range(0, cv_network_controller.sizeof_uint64):
            bytearr += chr((uinteger >> (i * 8)) & 0xff)
        return bytearr

    @staticmethod
    def __recv_all(sock, size):
        buffer = ''
        while len(buffer) < size:
            buffer += sock.recv(size - len(buffer))
        return buffer
