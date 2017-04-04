import json
import socket
import cv2
import time

import multiprocessing
import signal

localhost = '127.0.0.1'

state_closed = 0
state_ready = 1
state_run = 2

com_run = 0
com_stop = 1
com_close = 2

sizeof_uint64 = 8


def bytearr_to_uint(bytearr):
    uinteger = 0
    for i in range(0, sizeof_uint64):
        uinteger += ord(bytearr[i]) << (8 * i)
    return uinteger


def uint_to_bytearr(uinteger):
    bytearr = ''
    for i in range(0, sizeof_uint64):
        bytearr += chr((uinteger >> (i * 8)) & 0xff)
    return bytearr


def recv_all(sock, size):
    buffer = ''
    while len(buffer) < size:
        buffer += sock.recv(size - len(buffer))
    return buffer


def connect_to_cvkernel_state_sock(tcp_address, tcp_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((tcp_address, tcp_port))
    return sock


def create_metadata_udp_server(udp_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((localhost, udp_port))
    return sock


def connect_to_cvkernel(tcp_address, tcp_port, udp_port):
    return connect_to_cvkernel_state_sock(tcp_address, tcp_port), create_metadata_udp_server(udp_port)


def send_cv_parameters(sock, cv_parameters):
    buffer = json.dumps(cv_parameters)
    buffer_size = uint_to_bytearr(len(buffer))
    sock.sendall(buffer_size + buffer)


def get_cv_state(sock):
    size_arr = sock.recv(sizeof_uint64)
    if not size_arr:
        return

    size = bytearr_to_uint(size_arr)
    buf = recv_all(sock, size)
    return json.loads(buf)


def send_cv_command(sock, command):
    json_map = {'command': command}
    buf = json.dumps(json_map)
    buffer_size = uint_to_bytearr(len(buf))
    sock.sendall(buffer_size + buf)


def recv_metadata(metadata_sock):
    size_arr = metadata_sock.recv(sizeof_uint64)
    size = bytearr_to_uint(size_arr)
    if size == 0:
        return None
    buf = recv_all(metadata_sock, size)
    metadata = json.loads(buf)
    return metadata


def overlay_drawer(path, fps):
    overlay_cap = cv2.VideoCapture(path)
    while True:
        ret, frame = overlay_cap.read()
        if ret:
            cv2.imshow(path, frame)
        else:
            break
        cv2.waitKey(int(1000. / fps))
    cv2.destroyWindow(path)
    return


def metadata_receiver():
    while True:
        meta = recv_metadata(metadata_socket)
        print 'metadata = {}'.format(meta)
        if meta is None:
            return


if __name__ == '__main__':
    cvproc_json_path = '../../FireRobotDriver/fire_overlay_on_pc.json'
    cvkern_json_path = '../cv_kernel_settings.json'
    cv_process_description = json.load(open(cvproc_json_path, 'r'))
    cv_kernel_settings = json.load(open(cvkern_json_path, 'r'))
    state_socket, metadata_socket = connect_to_cvkernel(
        localhost, cv_process_description['state_tcp_port'], int(cv_process_description['meta_udp_path'].split(':')[1])
    )
    status = get_cv_state(state_socket)
    print 'state is closed: {}'.format(status['state'] == state_closed)
    send_cv_parameters(state_socket, cv_process_description)
    status = get_cv_state(state_socket)
    print 'state is ready: {}'.format(status['state'] == state_ready)
    send_cv_command(state_socket, com_run)
    status = get_cv_state(state_socket)
    print 'state is run: {}'.format(status['state'] == state_run)
    if status['state'] == state_run:
        pump_time = 0
        biggest_rect = None

        meta_proc = multiprocessing.Process(target=metadata_receiver)
        drawer_proc = multiprocessing.Process(
            target=overlay_drawer,
            args=(cv_process_description['output_path'], status['fps'])
        )
        meta_proc.start()
        drawer_proc.start()

        meta_proc.join()
        drawer_proc.terminate()

    status = get_cv_state(state_socket)
    if status['state'] == state_closed:
        print 'state is closed'
        state_socket.close()
        metadata_socket.close()
